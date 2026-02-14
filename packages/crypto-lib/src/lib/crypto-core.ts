/**
 * Core crypto operations using Result<T, E> pattern
 * No throwing errors - all failures returned as Result.error
 */

import {
  CryptoResult,
  CryptoErrorType,
  CryptoErr,
  Ok,
  EncryptionResult,
  DecryptionResult,
  KeyGenerationResult,
  SignatureResult,
  VerificationResult
} from '../types/index.js';

/**
 * Core cryptographic operations with predictable error handling
 */
export class CryptoCore {
  /**
   * Generate encryption key pair
   * Returns Result instead of throwing on failure
   */
  static async generateKeyPair(algorithm: string = 'RSA-OAEP'): Promise<KeyGenerationResult> {
    try {
      if (!crypto?.subtle) {
        return CryptoErr(
          CryptoErrorType.HARDWARE_ERROR,
          'Web Crypto API not available',
          'generateKeyPair'
        );
      }

      const keyPair = await crypto.subtle.generateKey(
        {
          name: algorithm,
          modulusLength: 2048,
          publicExponent: new Uint8Array([1, 0, 1]),
          hash: 'SHA-256'
        },
        true,
        ['encrypt', 'decrypt']
      );

      // Export keys to get string representations
      const publicKeyBuffer = await crypto.subtle.exportKey('spki', keyPair.publicKey);
      const privateKeyBuffer = await crypto.subtle.exportKey('pkcs8', keyPair.privateKey);

      const publicKey = btoa(String.fromCharCode(...new Uint8Array(publicKeyBuffer)));
      const privateKey = btoa(String.fromCharCode(...new Uint8Array(privateKeyBuffer)));
      const keyId = crypto.randomUUID();

      return Ok({
        publicKey,
        privateKey,
        keyId
      });
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.KEY_GENERATION_FAILED,
        error instanceof Error ? error.message : 'Unknown key generation error',
        'generateKeyPair',
        { algorithm },
        error instanceof Error ? error : undefined
      );
    }
  }

  /**
   * Encrypt data using public key
   * Returns Result with encrypted data or error
   */
  static async encrypt(data: string, publicKeyPem: string): Promise<EncryptionResult> {
    try {
      if (!data || !publicKeyPem) {
        return CryptoErr(
          CryptoErrorType.INVALID_KEY,
          'Data and public key are required',
          'encrypt'
        );
      }

      // Convert PEM to CryptoKey (simplified for example)
      const keyData = atob(publicKeyPem);
      const keyBuffer = new Uint8Array(keyData.length);
      for (let i = 0; i < keyData.length; i++) {
        keyBuffer[i] = keyData.charCodeAt(i);
      }

      const publicKey = await crypto.subtle.importKey(
        'spki',
        keyBuffer,
        { name: 'RSA-OAEP', hash: 'SHA-256' },
        false,
        ['encrypt']
      );

      const encoder = new TextEncoder();
      const dataBuffer = encoder.encode(data);

      const encryptedBuffer = await crypto.subtle.encrypt(
        'RSA-OAEP',
        publicKey,
        dataBuffer
      );

      const ciphertext = btoa(String.fromCharCode(...new Uint8Array(encryptedBuffer)));

      return Ok({
        ciphertext
      });
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.ENCRYPTION_FAILED,
        error instanceof Error ? error.message : 'Encryption failed',
        'encrypt',
        { dataLength: data?.length },
        error instanceof Error ? error : undefined
      );
    }
  }

  /**
   * Decrypt data using private key
   * Returns Result with decrypted data or error
   */
  static async decrypt(ciphertext: string, privateKeyPem: string): Promise<DecryptionResult> {
    try {
      if (!ciphertext || !privateKeyPem) {
        return CryptoErr(
          CryptoErrorType.INVALID_KEY,
          'Ciphertext and private key are required',
          'decrypt'
        );
      }

      // Convert PEM to CryptoKey (simplified for example)
      const keyData = atob(privateKeyPem);
      const keyBuffer = new Uint8Array(keyData.length);
      for (let i = 0; i < keyData.length; i++) {
        keyBuffer[i] = keyData.charCodeAt(i);
      }

      const privateKey = await crypto.subtle.importKey(
        'pkcs8',
        keyBuffer,
        { name: 'RSA-OAEP', hash: 'SHA-256' },
        false,
        ['decrypt']
      );

      const encryptedData = atob(ciphertext);
      const encryptedBuffer = new Uint8Array(encryptedData.length);
      for (let i = 0; i < encryptedData.length; i++) {
        encryptedBuffer[i] = encryptedData.charCodeAt(i);
      }

      const decryptedBuffer = await crypto.subtle.decrypt(
        'RSA-OAEP',
        privateKey,
        encryptedBuffer
      );

      const decoder = new TextDecoder();
      const decryptedData = decoder.decode(decryptedBuffer);

      return Ok(decryptedData);
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.DECRYPTION_FAILED,
        error instanceof Error ? error.message : 'Decryption failed',
        'decrypt',
        { ciphertextLength: ciphertext?.length },
        error instanceof Error ? error : undefined
      );
    }
  }
}