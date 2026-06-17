#include "euxis/crypto/key_derivation.hpp"

#include <sodium.h>

#include <algorithm>
#include <cassert>
#include <cstring>

namespace euxis::crypto {

/// P10-R2: Maximum key size to prevent excessive allocation.
constexpr size_t kMaxKeySize = 1024;

auto derive_key(std::span<const std::byte> password,
                std::span<const std::byte> salt,
                uint32_t iterations,
                size_t key_size)
    -> std::expected<DerivedKey, CryptoError> {

    // Bounded-input contract: callers may pass any size; out-of-range values yield an
    // explicit error (matches the documented std::expected return type — historically
    // these were assert()s, but tests like KeySizeTooSmall verify the error path).
    if (key_size == 0 || key_size > kMaxKeySize) {
        return std::unexpected(CryptoError::KeyDerivationFailed);
    }
    // Note: empty password is allowed (see KeyDerivationTest.EmptyPasswordWorks);
    // libsodium handles it via the underlying KDF.

    // Fast-path: if iterations is 0, use BLAKE2b (crypto_generichash) instead of Argon2id.
    // WARNING: BLAKE2b is NOT memory-hard. Only use iterations==0 for deterministic
    // session key derivation where brute-force resistance is not required.
    // Do NOT use iterations==0 for user-password-based encryption.
    if (iterations == 0) {
        if (key_size < crypto_generichash_BYTES_MIN || key_size > crypto_generichash_BYTES_MAX) {
            return std::unexpected(CryptoError::KeyDerivationFailed);
        }
        
        DerivedKey result;
        result.key.resize(key_size);
        result.salt.assign(salt.begin(), salt.end());
        
        // Use salt as the key for BLAKE2b
        const int rc = crypto_generichash(
            reinterpret_cast<unsigned char*>(result.key.data()),
            key_size,
            reinterpret_cast<const unsigned char*>(password.data()),
            password.size(),
            reinterpret_cast<const unsigned char*>(salt.data()),
            salt.size());
            
        if (rc != 0) {
            return std::unexpected(CryptoError::KeyDerivationFailed);
        }
        return result;
    }

    // libsodium requires exactly crypto_pwhash_SALTBYTES (16) bytes of salt.
    if (salt.size() != crypto_pwhash_SALTBYTES) {
        return std::unexpected(CryptoError::KeyDerivationFailed);
    }

    if (key_size < crypto_pwhash_BYTES_MIN || key_size > crypto_pwhash_BYTES_MAX) {
        return std::unexpected(CryptoError::KeyDerivationFailed);
    }

    // Map the iterations parameter to opslimit.  Argon2id's opslimit is not
    // the same concept as PBKDF2 iterations, but we honour the caller's intent:
    //   >= 100'000  -> SENSITIVE  (4)
    //   >= 10'000   -> MODERATE   (3)
    //   otherwise   -> INTERACTIVE (2, libsodium minimum)
    unsigned long long opslimit = 0;
    if (iterations >= 100'000) {
        opslimit = crypto_pwhash_OPSLIMIT_SENSITIVE;
    } else if (iterations >= 10'000) {
        opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
    } else {
        opslimit = crypto_pwhash_OPSLIMIT_INTERACTIVE;
    }

    // Use MODERATE memory for SENSITIVE and MODERATE ops alike,
    // INTERACTIVE otherwise, to keep runtime reasonable while still
    // being secure.
    size_t memlimit = 0;
    if (opslimit >= crypto_pwhash_OPSLIMIT_MODERATE) {
        memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
    } else {
        memlimit = crypto_pwhash_MEMLIMIT_INTERACTIVE;
    }

    DerivedKey result;
    result.key.resize(key_size);
    result.salt.assign(salt.begin(), salt.end());

    const int rc = crypto_pwhash(
        reinterpret_cast<unsigned char*>(result.key.data()),
        static_cast<unsigned long long>(key_size),
        reinterpret_cast<const char*>(password.data()),
        static_cast<unsigned long long>(password.size()),
        reinterpret_cast<const unsigned char*>(salt.data()),
        opslimit,
        memlimit,
        crypto_pwhash_ALG_ARGON2ID13);

    if (rc != 0) {
        return std::unexpected(CryptoError::KeyDerivationFailed);
    }

    return result;
}

auto generate_key(size_t size) -> std::vector<std::byte> {
    assert(size > 0 && size <= kMaxKeySize && "P10-R2: key size bounded");

    std::vector<std::byte> key(size);
    randombytes_buf(key.data(), key.size());

    assert(!sodium_is_zero(reinterpret_cast<const unsigned char*>(key.data()), key.size())
           && "P10-R5: generated key must not be all zeros");
    return key;
}

} // namespace euxis::crypto
