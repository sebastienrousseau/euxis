# Key Derivation: PBKDF2

Derive uniform cryptographic material from human-readable passwords using `PBKDF2-HMAC-SHA256` (Password-Based Key Derivation Function 2).

Use this function when your system accepts user credentials or configuration passphrases. It consistently pads and hashes passwords into 32-byte arrays suitable for `AES-256-GCM`.

## Threat Mitigation

Raw passwords lack the entropy required for direct encryption. You must derive all encryption keys using a secure hashing algorithm.

* **Rainbow Tables:** Inject a secure 16-byte cryptographic salt into the derivation process. This prevents pre-calculation attacks.
* **Brute Force:** Mandate high iteration counts. This deliberately slows down the derivation process.

## Generating Material

Generate exactly the byte length you need for your integrated cipher.

### Parameters

* `password`: The `&[u8]` representation of the raw credential.
* `salt`: A unique, randomly generated array. Store the salt alongside your ciphertext.
* `iterations`: The total computational rounds. For modern systems, specify at least `210_000`.
* `key_size`: The desired output length. Specify `32` for `AES-256-GCM`.

### Implementation

```rust
let derived_key = derive_key_pbkdf2(py, password, salt, 210_000, 32)?;
```

## Security Posture

Evaluate your hardware capabilities to determine the maximum viable iteration count. Increase the count aggressively until you achieve exactly 1000ms execution latency.
