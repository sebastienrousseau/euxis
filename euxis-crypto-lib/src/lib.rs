//! # Euxis Cryptography Core
//!
//! Secure your Euxis infrastructure with high-performance cryptographic primitives.
//! 
//! Use this module to protect agent communication and persistent memory arrays. 
//! We architected this foundation in Rust to guarantee true memory safety. 
//! It exposes native execution speeds to Python via `PyO3` bindings.
//!
//! ## Core Capabilities
//! 
//! * **Authenticated Encryption:** Secure payloads using `AES-256-GCM` (Advanced Encryption Standard Galois/Counter Mode).
//! * **Key Derivation:** Generate secure keys using `PBKDF2-HMAC-SHA256` (Password-Based Key Derivation Function 2).
//! * **Structural Integrity:** Prevent tampering and unauthorized mutation during decryption.
//!
//! Evaluate your specific security constraints before implementation.

use aes_gcm::{
    aead::{Aead, KeyInit, Payload},
    Aes256Gcm, Nonce
};
use pbkdf2::pbkdf2_hmac;
use sha2::Sha256;
use pyo3::prelude::*;
use pyo3::exceptions::PyValueError;
use pyo3::types::PyBytes;

/// Encrypt binary payloads using `AES-256-GCM` (Advanced Encryption Standard Galois/Counter Mode).
///
/// Use this method to protect data confidentiality and integrity.
///
/// # Parameters
/// * `data` - The raw plaintext `&[u8]` bytes.
/// * `key` - A strict 32-byte cryptographic key.
/// * `iv` - A strict 12-byte initialization vector (nonce).
///
/// # Returns
/// Returns a `PyBytes` object containing the authenticated ciphertext.
///
/// # Errors
/// Raises `ValueError` if you provide an invalid `key` or `iv` length.
/// Raises `ValueError` if the underlying cipher operation fails.
#[pyfunction]
fn aes_gcm_encrypt<'py>(
    py: Python<'py>,
    data: &[u8],
    key: &[u8],
    iv: &[u8]
) -> PyResult<Bound<'py, PyBytes>> {
    if key.len() != 32 {
        return Err(PyValueError::new_err("AES-256 requires exactly a 32-byte key"));
    }
    if iv.len() != 12 {
        return Err(PyValueError::new_err("GCM mode requires exactly a 12-byte IV"));
    }

    let cipher = Aes256Gcm::new_from_slice(key)
        .map_err(|e| PyValueError::new_err(format!("Cipher init failed: {}", e)))?;
    let nonce = Nonce::from_slice(iv);

    let ciphertext = cipher.encrypt(nonce, Payload { msg: data, aad: &[] })
        .map_err(|e| PyValueError::new_err(format!("Encryption failed: {}", e)))?;

    Ok(PyBytes::new_bound(py, &ciphertext))
}

/// Decrypt authenticated ciphertext using `AES-256-GCM`.
///
/// Use this method to recover your original plaintext data. 
/// It guarantees structural integrity using the integrated MAC (Message Authentication Code). 
/// If the data is tampered with, this operation instantly fails.
///
/// # Parameters
/// * `ciphertext` - The encrypted `&[u8]` bytes.
/// * `key` - A strict 32-byte cryptographic key.
/// * `iv` - A strict 12-byte initialization vector (nonce).
///
/// # Returns
/// Returns a `PyBytes` object containing the verified plaintext.
///
/// # Errors
/// Raises `ValueError` if you provide an invalid `key` or `iv` length.
/// Raises `ValueError` if the MAC validation fails, indicating tampering.
#[pyfunction]
fn aes_gcm_decrypt<'py>(
    py: Python<'py>,
    ciphertext: &[u8],
    key: &[u8],
    iv: &[u8]
) -> PyResult<Bound<'py, PyBytes>> {
    if key.len() != 32 {
        return Err(PyValueError::new_err("AES-256 requires exactly a 32-byte key"));
    }
    if iv.len() != 12 {
        return Err(PyValueError::new_err("GCM mode requires exactly a 12-byte IV"));
    }

    let cipher = Aes256Gcm::new_from_slice(key)
        .map_err(|e| PyValueError::new_err(format!("Cipher init failed: {}", e)))?;
    let nonce = Nonce::from_slice(iv);

    let plaintext = cipher.decrypt(nonce, Payload { msg: ciphertext, aad: &[] })
        .map_err(|e| PyValueError::new_err(format!("Decryption failed: {}", e)))?;

    Ok(PyBytes::new_bound(py, &plaintext))
}

/// Derive a cryptographic key using `PBKDF2-HMAC-SHA256` (Password-Based Key Derivation Function 2).
///
/// Use this method to convert human-readable passwords into uniform cryptographic keys.
///
/// # Parameters
/// * `password` - The raw password `&[u8]` bytes.
/// * `salt` - Cryptographic salt to prevent rainbow table attacks.
/// * `iterations` - The integer iteration count. Higher counts increase security.
/// * `key_size` - The target byte length of the derived key.
///
/// # Returns
/// Returns a `PyBytes` object containing the derived uniform key.
#[pyfunction]
fn derive_key_pbkdf2<'py>(
    py: Python<'py>,
    password: &[u8],
    salt: &[u8],
    iterations: u32,
    key_size: usize
) -> PyResult<Bound<'py, PyBytes>> {
    let mut key = vec![0u8; key_size];
    pbkdf2_hmac::<Sha256>(password, salt, iterations, &mut key);
    Ok(PyBytes::new_bound(py, &key))
}

/// Register the native Rust cryptographic primitives into the Python runtime.
///
/// Initialize the module context to expose standard encryption routes.
#[pymodule]
fn crypto_lib_rs(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_function(wrap_pyfunction!(aes_gcm_encrypt, m)?)?;
    m.add_function(wrap_pyfunction!(aes_gcm_decrypt, m)?)?;
    m.add_function(wrap_pyfunction!(derive_key_pbkdf2, m)?)?;
    Ok(())
}
