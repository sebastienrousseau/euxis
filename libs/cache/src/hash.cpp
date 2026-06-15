#include "euxis/cache/hash.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <new>
#include <sodium.h>

namespace euxis::cache {

namespace {

bool sodium_inited = false;

void ensure_sodium() noexcept {
    if (!sodium_inited) {
        // sodium_init() is idempotent and safe to call from multiple
        // libraries. Returns 1 if already initialised, 0 on first
        // call, -1 on failure; we tolerate -1 because in that case
        // libsodium primitives still work but with degraded RNG.
        (void)sodium_init();
        sodium_inited = true;
    }
}

auto to_hex(std::span<const unsigned char> bytes) -> std::string {
    static constexpr std::array<char, 16> hex{
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f',
    };
    std::string out;
    out.reserve(bytes.size() * 2);
    for (unsigned char b : bytes) {
        out.push_back(hex[(b >> 4) & 0xF]);
        out.push_back(hex[b & 0xF]);
    }
    return out;
}

} // namespace

// ---- Hasher::State ---------------------------------------------------------

struct Hasher::State {
    crypto_generichash_state s{};
};

Hasher::Hasher() : state_(new State{}) {
    ensure_sodium();
    crypto_generichash_init(&state_->s, nullptr, 0, kDigestBytes);
}

Hasher::~Hasher() { delete state_; }

Hasher::Hasher(Hasher&& other) noexcept : state_(other.state_) {
    other.state_ = nullptr;
}

Hasher& Hasher::operator=(Hasher&& other) noexcept {
    if (this != &other) {
        delete state_;
        state_       = other.state_;
        other.state_ = nullptr;
    }
    return *this;
}

void Hasher::update(std::span<const std::byte> data) noexcept {
    if (state_ == nullptr || data.empty()) return;
    crypto_generichash_update(&state_->s,
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size());
}

void Hasher::update(std::string_view s) noexcept {
    if (state_ == nullptr || s.empty()) return;
    crypto_generichash_update(&state_->s,
        reinterpret_cast<const unsigned char*>(s.data()),
        s.size());
}

auto Hasher::finalize() -> std::string {
    if (state_ == nullptr) return {};
    std::array<unsigned char, kDigestBytes> digest{};
    crypto_generichash_final(&state_->s, digest.data(), digest.size());
    return to_hex(digest);
}

// ---- free functions --------------------------------------------------------

auto hash_bytes(std::span<const std::byte> data) -> std::string {
    ensure_sodium();
    std::array<unsigned char, kDigestBytes> digest{};
    crypto_generichash(digest.data(), digest.size(),
        reinterpret_cast<const unsigned char*>(data.data()),
        data.size(),
        nullptr, 0);
    return to_hex(digest);
}

auto hash_string(std::string_view s) -> std::string {
    ensure_sodium();
    std::array<unsigned char, kDigestBytes> digest{};
    crypto_generichash(digest.data(), digest.size(),
        reinterpret_cast<const unsigned char*>(s.data()),
        s.size(),
        nullptr, 0);
    return to_hex(digest);
}

auto hash_file(const std::filesystem::path& path)
    -> std::expected<std::string, HashError> {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) {
        return std::unexpected(HashError{
            .message = "Cannot open file",
            .file = path,
        });
    }
    ensure_sodium();

    crypto_generichash_state s{};
    crypto_generichash_init(&s, nullptr, 0, kDigestBytes);

    // 64 KiB matches the libsodium-recommended chunk size and lands
    // squarely below typical L2 cache so we don't churn memory on
    // small machines.
    std::array<char, 64 * 1024> buf{};
    while (f) {
        f.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize n = f.gcount();
        if (n > 0) {
            crypto_generichash_update(&s,
                reinterpret_cast<const unsigned char*>(buf.data()),
                static_cast<std::size_t>(n));
        }
    }
    if (f.bad()) {
        return std::unexpected(HashError{
            .message = "I/O error while hashing file",
            .file = path,
        });
    }

    std::array<unsigned char, kDigestBytes> digest{};
    crypto_generichash_final(&s, digest.data(), digest.size());
    return to_hex(digest);
}

auto is_valid_digest(std::string_view s) noexcept -> bool {
    if (s.size() != kDigestHexLen) return false;
    return std::all_of(s.begin(), s.end(),
        [](char c) {
            return std::isxdigit(static_cast<unsigned char>(c)) != 0;
        });
}

} // namespace euxis::cache
