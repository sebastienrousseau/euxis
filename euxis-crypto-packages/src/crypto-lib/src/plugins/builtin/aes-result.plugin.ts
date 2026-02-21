/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2024-2026 Euxis Contributors
 */

/**
 * Built-in AES Encryption Plugin (Result-based)
 * Provides AES-256-GCM, AES-192-GCM, and AES-128-GCM encryption algorithms
 * Uses Result<T, E> pattern for predictable error handling without exceptions
 */

import {
  ResultCryptoPlugin,
  PluginMetadata,
  PluginCapabilities,
  EncryptionResult,
  DecryptionResult,
  KeyGenerationResult
} from '../result-interface';
import {
  EncryptionAlgorithm,
  PlainText,
  CipherText,
  SymmetricKey,
  createCipherText,
  createPlainText,
  createSymmetricKey
} from '../../types';
import {
  CryptoResult,
  CryptoErrorType,
  Ok,
  CryptoErr,
  createEncryptionContext,
  createKeyContext
} from '../../types/result';

/**
 * AES encryption plugin implementing the ResultCryptoPlugin interface
 * All operations return Result<T, CryptoError> instead of throwing exceptions
 */
class AESResultPlugin implements ResultCryptoPlugin {
  readonly metadata: PluginMetadata = {
    name: 'aes-result-builtin',
    version: '1.0.0',
    description: 'Built-in AES encryption plugin with Result-based error handling',
    author: 'Euxis Crypto Team',
    capabilities: {
      encryption: true,
      hashing: false,
      signing: false,
      keyGeneration: true
    },
    algorithms: {
      encryption: ['AES-256-GCM', 'AES-192-GCM', 'AES-128-GCM']
    }
  };

  private initialized = false;

  /**
   * Initialize the plugin
   */
  async initialize(): Promise<CryptoResult<void>> {
    if (this.initialized) {
      return Ok(undefined);
    }

    try {
      // In a real implementation, initialize crypto libraries here
      console.log('Initializing AES Result plugin...');

      // Simulate async initialization
      await new Promise(resolve => setTimeout(resolve, 10));

      this.initialized = true;
      console.log('AES Result plugin initialized successfully');
      return Ok(undefined);
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_NOT_INITIALIZED,
        'Failed to initialize AES plugin',
        'initialize',
        { pluginName: this.metadata.name },
        error instanceof Error ? error : new Error(String(error))
      );
    }
  }

  /**
   * Cleanup the plugin
   */
  async cleanup(): Promise<CryptoResult<void>> {
    if (!this.initialized) {
      return Ok(undefined);
    }

    try {
      console.log('Cleaning up AES Result plugin...');

      // In a real implementation, cleanup crypto resources here
      this.initialized = false;

      console.log('AES Result plugin cleanup completed');
      return Ok(undefined);
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_LOAD_FAILED,
        'Failed to cleanup AES plugin',
        'cleanup',
        { pluginName: this.metadata.name },
        error instanceof Error ? error : new Error(String(error))
      );
    }
  }

  /**
   * Encrypt data using AES algorithm
   */
  async encrypt(
    data: PlainText,
    key: SymmetricKey,
    algorithm: EncryptionAlgorithm
  ): Promise<EncryptionResult> {
    if (!this.initialized) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_NOT_INITIALIZED,
        'AES plugin not initialized',
        'encrypt',
        createEncryptionContext(algorithm, data.length)
      );
    }

    if (!this.metadata.algorithms.encryption?.includes(algorithm)) {
      return CryptoErr(
        CryptoErrorType.UNSUPPORTED_ALGORITHM,
        `Unsupported encryption algorithm: ${algorithm}`,
        'encrypt',
        createEncryptionContext(algorithm, data.length)
      );
    }

    try {
      // Validate key size
      const expectedKeySize = this.getKeySizeFromAlgorithm(algorithm);
      if (!expectedKeySize.success) {
        return expectedKeySize;
      }

      // Simulate encryption process
      const iv = this.generateIV();
      const authTag = this.generateAuthTag();

      // In a real implementation, use actual AES-GCM encryption
      const encryptedData = await this.performAESEncryption(data, key, algorithm, iv);
      if (!encryptedData.success) {
        return encryptedData;
      }

      return Ok({
        cipherText: createCipherText(encryptedData.data),
        algorithm,
        iv,
        authTag
      });
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.ENCRYPTION_FAILED,
        'AES encryption operation failed',
        'encrypt',
        createEncryptionContext(algorithm, data.length),
        error instanceof Error ? error : new Error(String(error))
      );
    }
  }

  /**
   * Decrypt data using AES algorithm
   */
  async decrypt(
    data: CipherText,
    key: SymmetricKey,
    algorithm: EncryptionAlgorithm
  ): Promise<DecryptionResult> {
    if (!this.initialized) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_NOT_INITIALIZED,
        'AES plugin not initialized',
        'decrypt',
        createEncryptionContext(algorithm, data.length)
      );
    }

    if (!this.metadata.algorithms.encryption?.includes(algorithm)) {
      return CryptoErr(
        CryptoErrorType.UNSUPPORTED_ALGORITHM,
        `Unsupported decryption algorithm: ${algorithm}`,
        'decrypt',
        createEncryptionContext(algorithm, data.length)
      );
    }

    try {
      // In a real implementation, use actual AES-GCM decryption
      const decryptedData = await this.performAESDecryption(data, key, algorithm);
      if (!decryptedData.success) {
        return decryptedData;
      }

      return Ok({
        plainText: createPlainText(decryptedData.data),
        algorithm
      });
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.DECRYPTION_FAILED,
        'AES decryption operation failed',
        'decrypt',
        createEncryptionContext(algorithm, data.length),
        error instanceof Error ? error : new Error(String(error))
      );
    }
  }

  /**
   * Generate a symmetric key for AES encryption
   */
  async generateSymmetricKey(algorithm: EncryptionAlgorithm): Promise<KeyGenerationResult> {
    if (!this.initialized) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_NOT_INITIALIZED,
        'AES plugin not initialized',
        'generateSymmetricKey',
        createKeyContext('symmetric', algorithm)
      );
    }

    if (!this.metadata.algorithms.encryption?.includes(algorithm)) {
      return CryptoErr(
        CryptoErrorType.UNSUPPORTED_ALGORITHM,
        `Unsupported algorithm for key generation: ${algorithm}`,
        'generateSymmetricKey',
        createKeyContext('symmetric', algorithm)
      );
    }

    try {
      const keySizeResult = this.getKeySizeFromAlgorithm(algorithm);
      if (!keySizeResult.success) {
        return keySizeResult;
      }

      const key = await this.generateRandomKey(keySizeResult.data);
      if (!key.success) {
        return key;
      }

      return Ok(createSymmetricKey(key.data));
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.KEY_GENERATION_FAILED,
        'Failed to generate symmetric key',
        'generateSymmetricKey',
        createKeyContext('symmetric', algorithm),
        error instanceof Error ? error : new Error(String(error))
      );
    }
  }

  /**
   * Get key size in bytes from algorithm name
   */
  private getKeySizeFromAlgorithm(algorithm: EncryptionAlgorithm): CryptoResult<number> {
    switch (algorithm) {
      case 'AES-256-GCM': return Ok(32);
      case 'AES-192-GCM': return Ok(24);
      case 'AES-128-GCM': return Ok(16);
      default:
        return CryptoErr(
          CryptoErrorType.INVALID_ALGORITHM,
          `Unknown algorithm: ${algorithm}`,
          'getKeySizeFromAlgorithm',
          { algorithm }
        );
    }
  }

  /**
   * Generate random initialization vector
   */
  private generateIV(): string {
    // In a real implementation, use cryptographically secure random
    return 'mock_iv_' + Math.random().toString(36).substring(2, 15);
  }

  /**
   * Generate authentication tag
   */
  private generateAuthTag(): string {
    // In a real implementation, this would be generated during encryption
    return 'mock_auth_tag_' + Math.random().toString(36).substring(2, 15);
  }

  /**
   * Generate random symmetric key
   */
  private async generateRandomKey(sizeBytes: number): Promise<CryptoResult<string>> {
    try {
      // In a real implementation, use cryptographically secure random
      if (sizeBytes < 16 || sizeBytes > 32) {
        return CryptoErr(
          CryptoErrorType.INVALID_PARAMETERS,
          `Invalid key size: ${sizeBytes}. Must be between 16 and 32 bytes`,
          'generateRandomKey',
          { requestedSize: sizeBytes }
        );
      }

      const key = 'mock_key_' + sizeBytes + '_' + Math.random().toString(36).substring(2, 15);
      return Ok(key);
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.INSUFFICIENT_ENTROPY,
        'Failed to generate random key',
        'generateRandomKey',
        { keySize: sizeBytes },
        error instanceof Error ? error : new Error(String(error))
      );
    }
  }

  /**
   * Perform actual AES encryption (mock implementation)
   */
  private async performAESEncryption(
    data: PlainText,
    key: SymmetricKey,
    algorithm: EncryptionAlgorithm,
    iv: string
  ): Promise<CryptoResult<string>> {
    try {
      if (!data || data.length === 0) {
        return CryptoErr(
          CryptoErrorType.INVALID_INPUT,
          'Empty data provided for encryption',
          'performAESEncryption',
          { algorithm, dataLength: 0 }
        );
      }

      // In a real implementation, use actual AES-GCM from crypto libraries
      const encrypted = `aes_encrypted_${algorithm}_${data}_with_${key}_iv_${iv}`;
      return Ok(encrypted);
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.ENCRYPTION_FAILED,
        'AES encryption primitive failed',
        'performAESEncryption',
        { algorithm, dataLength: data.length },
        error instanceof Error ? error : new Error(String(error))
      );
    }
  }

  /**
   * Perform actual AES decryption (mock implementation)
   */
  private async performAESDecryption(
    data: CipherText,
    key: SymmetricKey,
    algorithm: EncryptionAlgorithm
  ): Promise<CryptoResult<string>> {
    try {
      if (!data || data.length === 0) {
        return CryptoErr(
          CryptoErrorType.INVALID_CIPHERTEXT,
          'Empty ciphertext provided for decryption',
          'performAESDecryption',
          { algorithm }
        );
      }

      const dataStr = data.toString();
      if (dataStr.startsWith(`aes_encrypted_${algorithm}_`)) {
        // Extract original data (mock)
        const parts = dataStr.split('_');
        const decrypted = parts[3] || 'decrypted_data';
        return Ok(decrypted);
      }

      // For non-matching patterns, try generic decryption
      return Ok('decrypted_' + dataStr);
    } catch (error) {
      return CryptoErr(
        CryptoErrorType.DECRYPTION_FAILED,
        'AES decryption primitive failed',
        'performAESDecryption',
        { algorithm, ciphertextLength: data.length },
        error instanceof Error ? error : new Error(String(error))
      );
    }
  }
}

/**
 * Plugin factory function
 */
export function createResultPlugin(): ResultCryptoPlugin {
  return new AESResultPlugin();
}

/**
 * Default export for plugin loader
 */
export default createResultPlugin;