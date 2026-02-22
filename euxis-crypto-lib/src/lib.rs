use aes_gcm::{
    aead::{Aead, KeyInit, Payload},
    Aes256Gcm, Nonce
};
use pbkdf2::pbkdf2_hmac;
use sha2::Sha256;
use pyo3::prelude::*;
use pyo3::exceptions::PyValueError;
use pyo3::types::PyBytes;

/// Encrypts data using AES-256-GCM
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

/// Decrypts data using AES-256-GCM
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

/// Derives a key from a password using PBKDF2-HMAC-SHA256
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

/// A Python module implemented in Rust.
#[pymodule]
fn crypto_lib_rs(m: &Bound<'_, PyModule>) -> PyResult<()> {
    m.add_function(wrap_pyfunction!(aes_gcm_encrypt, m)?)?;
    m.add_function(wrap_pyfunction!(aes_gcm_decrypt, m)?)?;
    m.add_function(wrap_pyfunction!(derive_key_pbkdf2, m)?)?;
    Ok(())
}
