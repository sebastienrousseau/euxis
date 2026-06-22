#include "euxis/sbom/types.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <ctime>
#include <random>
#include <sstream>

namespace euxis::sbom {

auto component_type_str(ComponentType t) noexcept -> const char* {
    switch (t) {
        case ComponentType::Application:           return "application";
        case ComponentType::Framework:             return "framework";
        case ComponentType::Library:               return "library";
        case ComponentType::Container:             return "container";
        case ComponentType::Platform:              return "platform";
        case ComponentType::OperatingSystem:       return "operating-system";
        case ComponentType::Device:                return "device";
        case ComponentType::Firmware:              return "firmware";
        case ComponentType::File:                  return "file";
        case ComponentType::MachineLearningModel:  return "machine-learning-model";
        case ComponentType::Data:                  return "data";
    }
    return "library";
}

auto hash_alg_cyclonedx(HashAlgorithm a) noexcept -> const char* {
    // CycloneDX uses hyphenated names per IANA hash-function-text-names.
    switch (a) {
        case HashAlgorithm::Md5:         return "MD5";
        case HashAlgorithm::Sha1:        return "SHA-1";
        case HashAlgorithm::Sha256:      return "SHA-256";
        case HashAlgorithm::Sha384:      return "SHA-384";
        case HashAlgorithm::Sha512:      return "SHA-512";
        case HashAlgorithm::Sha3_256:    return "SHA3-256";
        case HashAlgorithm::Sha3_512:    return "SHA3-512";
        case HashAlgorithm::Blake2b_256: return "BLAKE2b-256";
        case HashAlgorithm::Blake2b_512: return "BLAKE2b-512";
        case HashAlgorithm::Blake3:      return "BLAKE3";
    }
    return "SHA-256";
}

auto hash_alg_spdx(HashAlgorithm a) noexcept -> const char* {
    // SPDX 3.0.1 §SHA1, SHA256, etc. — no hyphens, lowercase blake.
    switch (a) {
        case HashAlgorithm::Md5:         return "MD5";
        case HashAlgorithm::Sha1:        return "SHA1";
        case HashAlgorithm::Sha256:      return "SHA256";
        case HashAlgorithm::Sha384:      return "SHA384";
        case HashAlgorithm::Sha512:      return "SHA512";
        case HashAlgorithm::Sha3_256:    return "SHA3-256";
        case HashAlgorithm::Sha3_512:    return "SHA3-512";
        case HashAlgorithm::Blake2b_256: return "BLAKE2b-256";
        case HashAlgorithm::Blake2b_512: return "BLAKE2b-512";
        case HashAlgorithm::Blake3:      return "BLAKE3";
    }
    return "SHA256";
}

auto is_valid_hash(const Hash& hash) noexcept -> bool {
    if (hash.value.empty()) return false;
    if (auto expected = digest_hex_length(hash.algorithm); expected != 0) {
        if (hash.value.size() != expected) return false;
    }
    return std::all_of(hash.value.begin(), hash.value.end(),
                       [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });
}

auto external_ref_type_str(ExternalRefType t) noexcept -> const char* {
    switch (t) {
        case ExternalRefType::Vcs:                return "vcs";
        case ExternalRefType::IssueTracker:       return "issue-tracker";
        case ExternalRefType::Website:            return "website";
        case ExternalRefType::Advisories:         return "advisories";
        case ExternalRefType::Bom:                return "bom";
        case ExternalRefType::MailingList:        return "mailing-list";
        case ExternalRefType::SocialMedia:        return "social";
        case ExternalRefType::Chat:               return "chat";
        case ExternalRefType::Documentation:      return "documentation";
        case ExternalRefType::Support:            return "support";
        case ExternalRefType::Distribution:       return "distribution";
        case ExternalRefType::DistributionIntake: return "distribution-intake";
        case ExternalRefType::License:            return "license";
        case ExternalRefType::BuildMeta:          return "build-meta";
        case ExternalRefType::BuildSystem:        return "build-system";
        case ExternalRefType::ReleaseNotes:       return "release-notes";
        case ExternalRefType::Other:              return "other";
    }
    return "other";
}

auto generate_serial_number() -> std::string {
    // Generate a UUIDv4 prefixed with "urn:uuid:" so it slots into the
    // CycloneDX serialNumber field directly. Uses std::random_device
    // for non-test seeding; deterministic builds should set
    // SbomDocument::serial_number explicitly.
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<std::uint64_t> dis;

    std::uint64_t a = dis(gen);
    std::uint64_t b = dis(gen);

    // RFC 4122 §4.4: set version (4) and variant (10) bits.
    //
    // The bit positions account for the big-endian print order
    // used by `append_bytes()` below — bits 60-63 of `a` land at
    // UUID char 14 (the version nibble), and bits 14-15 of `b`
    // land at the high two bits of UUID byte 8 (the variant).
    // Earlier masks targeted bit 14 of `a` and bit 63 of `b`,
    // putting the version digit and variant bits in the wrong
    // UUID positions — the bench/regex test caught it.
    a = (a & 0x0FFFFFFFFFFFFFFFULL) | 0x4000000000000000ULL;
    b = (b & 0xFFFFFFFFFFFF3FFFULL) | 0x0000000000008000ULL;

    const std::array<char, 16> hex{
        '0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f',
    };
    auto hex_byte = [&](std::uint8_t byte) {
        return std::array<char,2>{hex[byte >> 4], hex[byte & 0x0F]};
    };

    std::string out = "urn:uuid:";
    out.reserve(out.size() + 36);
    auto append_bytes = [&](std::uint64_t value, int n) {
        for (int i = n - 1; i >= 0; --i) {
            auto bp = hex_byte(static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFU));
            out.push_back(bp[0]);
            out.push_back(bp[1]);
        }
    };
    append_bytes(a, 4); out.push_back('-');
    append_bytes(a >> 32, 2); out.push_back('-');
    append_bytes(a >> 48, 2); out.push_back('-');
    append_bytes(b, 2); out.push_back('-');
    append_bytes(b >> 16, 6);
    return out;
}

auto to_rfc3339(std::chrono::system_clock::time_point tp) -> std::string {
    auto t = std::chrono::system_clock::to_time_t(tp);
    std::tm gm{};
#if defined(_WIN32)
    gmtime_s(&gm, &t);
#else
    gmtime_r(&t, &gm);
#endif
    char buf[32]{};
    (void) std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &gm);
    return std::string{buf};
}

} // namespace euxis::sbom
