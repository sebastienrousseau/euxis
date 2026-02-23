# Encryption: AES-256-GCM

Protect your data confidentiality and integrity with `AES-256-GCM` (Advanced Encryption Standard Galois/Counter Mode).

Use this algorithm to encrypt sensitive memory arrays or network payloads. This implementation uses a strict 256-bit (32-byte) key length. It provides authenticated encryption with associated data (AEAD). 

## Encrypting Payloads

Encrypt raw bytes by passing them to the native Rust engine. 

### Requirements

* **Key:** Supply exactly 32 bytes (`[u8; 32]`).
* **Initialization Vector (Nonce):** Supply exactly 12 bytes (`[u8; 12]`).

### Implementation

The encryption operation automatically generates a Message Authentication Code (MAC) and appends it to your ciphertext. This ensures data authenticity.

```rust
let ciphertext = aes_gcm_encrypt(py, data, key, iv)?;
```

If you provide invalid boundaries, the library instantly raises a `ValueError`.

## Decrypting and Verifying

Decrypt ciphertext to recover the original binary data. The system guarantees structural integrity before exposing plaintext.

### The Integrity Check

The decryption method securely extracts the MAC and verifies the payload structure.

1. The cipher authenticates the block using your cryptographic key.
2. If the MAC fails verification, the payload was corrupted or tampered with.
3. The method instantly faults and raises a `ValueError` to block malicious injection.

```rust
let plaintext = aes_gcm_decrypt(py, ciphertext, key, iv)?;
```

Do not bypass error handling during decryption. Treat any `ValueError` as a potential security breach.
