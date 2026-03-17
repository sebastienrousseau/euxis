#include "euxis/crypto/key_derivation.hpp"

#include <sodium.h>

#include <algorithm>
#include <cstring>

namespace euxis::crypto {

auto derive_key(std::span<const std::byte> password,
                std::span<const std::byte> salt,
                uint32_t iterations,
                size_t key_size)
    -> std::expected<DerivedKey, CryptoError> {

    // Fast-path: if iterations is 0, use BLAKE2b (crypto_generichash) instead of Argon2id.
    // This is significantly faster and suitable for deterministic session key derivation 
    // where brute-force resistance (memory hardness) is not the primary goal.
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
    unsigned long long opslimit;
    if (iterations >= 100'000) {
        opslimit = crypto_pwhash_OPSLIMIT_SENSITIVE;
    } else if (iterations >= 10'000) {
        opslimit = crypto_pwhash_OPSLIMIT_MODERATE;
    } else {
        opslimit = crypto_pwhash_OPSLIMIT_INTERACTIVE;
    }

    // Use MODERATE memory for SENSITIVE ops, INTERACTIVE memory otherwise,
    // to keep runtime reasonable while still being secure.
    size_t memlimit;
    if (opslimit >= crypto_pwhash_OPSLIMIT_SENSITIVE) {
        memlimit = crypto_pwhash_MEMLIMIT_MODERATE;
    } else if (opslimit >= crypto_pwhash_OPSLIMIT_MODERATE) {
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
    std::vector<std::byte> key(size);
    randombytes_buf(key.data(), key.size());
    return key;
}

} // namespace euxis::crypto
