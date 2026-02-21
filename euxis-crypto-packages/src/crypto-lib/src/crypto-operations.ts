/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2024-2026 Euxis Contributors
 */

// Crypto operations with proper branded types and worker pool integration

import { randomBytes, createCipheriv, createDecipheriv, createHash, generateKeyPair, sign, verify } from 'crypto';
import { promisify } from 'util';
import {
  SymmetricKey, PublicKey, PrivateKey, PlainText, CipherText, HashValue, Signature,
  EncryptionAlgorithm, HashAlgorithm, SignatureAlgorithm,
  EncryptionResult, DecryptionResult, HashResult, KeyPair, SignatureResult, VerificationResult,
  CryptoOperationOptions,
  createPublicKey, createPrivateKey, createCipherText, createPlainText, createHashValue, createSignature
} from './types.js';

const generateKeyPairAsync = promisify(generateKeyPair);

// Worker pool bridge interface
export interface CryptoOperation {
  operation: string;
  data: Buffer;
  options?: CryptoOperationOptions;
}

export interface CryptoOperationResult {
  success: boolean;
  data?: Buffer;
  error?: string;
  metadata?: {
    algorithm?: string;
    keyId?: string;
    performance?: {
      duration: number;
      memoryUsed: number;
    };
  };
}

export class CryptoOperations {
  private key: SymmetricKey;
  private encryptionAlgorithm: EncryptionAlgorithm;
  private hashAlgorithm: HashAlgorithm;

  constructor(key: SymmetricKey, encryptionAlgorithm: EncryptionAlgorithm, hashAlgorithm: HashAlgorithm) {
    this.key = key;
    this.encryptionAlgorithm = encryptionAlgorithm;
    this.hashAlgorithm = hashAlgorithm;
  }

  encrypt(data: PlainText): EncryptionResult {
    const startTime = performance.now();
    const memBefore = process.memoryUsage().heapUsed;

    try {
      // Real AES-GCM encryption
      const algorithm = this.mapEncryptionAlgorithm(this.encryptionAlgorithm);
      const iv = randomBytes(16); // 128-bit IV for AES
      const key = Buffer.from(this.key, 'utf8').slice(0, 32); // Ensure 256-bit key

      const cipher = createCipheriv(algorithm, key, iv);
      let encrypted = cipher.update(data, 'utf8', 'hex');
      encrypted += cipher.final('hex');

      const authTag = cipher.getAuthTag();
      const cipherText = createCipherText(encrypted);

      const duration = performance.now() - startTime;
      const memAfter = process.memoryUsage().heapUsed;

      return {
        cipherText,
        algorithm: this.encryptionAlgorithm,
        iv: iv.toString('hex'),
        authTag: authTag.toString('hex')
      };
    } catch (error) {
      // Secure cleanup on error
      this.secureCleanup();
      throw error;
    }
  }

  decrypt(data: CipherText, iv?: string, authTag?: string): DecryptionResult {
    const startTime = performance.now();
    const memBefore = process.memoryUsage().heapUsed;

    try {
      if (!iv || !authTag) {
        throw new Error('IV and authTag required for decryption');
      }

      const algorithm = this.mapEncryptionAlgorithm(this.encryptionAlgorithm);
      const key = Buffer.from(this.key, 'utf8').slice(0, 32);
      const ivBuffer = Buffer.from(iv, 'hex');
      const authTagBuffer = Buffer.from(authTag, 'hex');

      const decipher = createDecipheriv(algorithm, key, ivBuffer);
      decipher.setAuthTag(authTagBuffer);

      let decrypted = decipher.update(data, 'hex', 'utf8');
      decrypted += decipher.final('utf8');

      const plainText = createPlainText(decrypted);

      return {
        plainText,
        algorithm: this.encryptionAlgorithm
      };
    } catch (error) {
      this.secureCleanup();
      throw error;
    }
  }

  hash(input: PlainText): HashResult {
    const startTime = performance.now();

    try {
      const algorithm = this.mapHashAlgorithm(this.hashAlgorithm);
      const hash = createHash(algorithm);
      hash.update(input, 'utf8');

      const hashValue = createHashValue(hash.digest('hex'));

      return {
        hash: hashValue,
        algorithm: this.hashAlgorithm
      };
    } catch (error) {
      this.secureCleanup();
      throw error;
    }
  }

  private computeHash(input: PlainText): HashValue {
    return this.hash(input).hash;
  }

  async generateKeyPair(algorithm: SignatureAlgorithm = 'RSA-PSS'): Promise<KeyPair> {
    const startTime = performance.now();

    try {
      type KeyGenerationOptions = {
        type: 'rsa' | 'ec' | 'ed25519';
        modulusLength?: number;
        namedCurve?: string;
        publicKeyEncoding: { type: 'spki'; format: 'pem' };
        privateKeyEncoding: { type: 'pkcs8'; format: 'pem' };
      };

      let keyOptions: KeyGenerationOptions;

      switch (algorithm) {
        case 'RSA-PSS':
          keyOptions = {
            type: 'rsa',
            modulusLength: 4096,
            publicKeyEncoding: { type: 'spki', format: 'pem' },
            privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
          };
          break;
        case 'ECDSA':
          keyOptions = {
            type: 'ec',
            namedCurve: 'secp384r1',
            publicKeyEncoding: { type: 'spki', format: 'pem' },
            privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
          };
          break;
        case 'EdDSA':
          keyOptions = {
            type: 'ed25519',
            publicKeyEncoding: { type: 'spki', format: 'pem' },
            privateKeyEncoding: { type: 'pkcs8', format: 'pem' }
          };
          break;
        default:
          throw new Error(`Unsupported algorithm: ${algorithm}`);
      }

      const { publicKey, privateKey } = await generateKeyPairAsync(keyOptions.type, keyOptions);

      return {
        publicKey: createPublicKey(publicKey),
        privateKey: createPrivateKey(privateKey)
      };
    } catch (error) {
      this.secureCleanup();
      throw error;
    }
  }

  sign(data: PlainText, privateKey: PrivateKey, algorithm: SignatureAlgorithm): SignatureResult {
    const startTime = performance.now();

    try {
      const signAlgorithm = this.mapSignatureAlgorithm(algorithm);
      const signature = sign(signAlgorithm, Buffer.from(data, 'utf8'), privateKey);

      return {
        signature: createSignature(signature.toString('hex')),
        algorithm,
        data
      };
    } catch (error) {
      this.secureCleanup();
      throw error;
    }
  }

  verify(signature: Signature, data: PlainText, publicKey: PublicKey, algorithm: SignatureAlgorithm): VerificationResult {
    try {
      const verifyAlgorithm = this.mapSignatureAlgorithm(algorithm);
      const signatureBuffer = Buffer.from(signature, 'hex');
      const dataBuffer = Buffer.from(data, 'utf8');

      const isValid = verify(verifyAlgorithm, dataBuffer, publicKey, signatureBuffer);

      return {
        isValid,
        signature,
        algorithm
      };
    } catch (error) {
      this.secureCleanup();
      return {
        isValid: false,
        signature,
        algorithm
      };
    }
  }

  // Algorithm mapping helpers
  private mapEncryptionAlgorithm(algorithm: EncryptionAlgorithm): string {
    const mapping: Record<EncryptionAlgorithm, string> = {
      'AES-256-GCM': 'aes-256-gcm',
      'AES-192-GCM': 'aes-192-gcm',
      'AES-128-GCM': 'aes-128-gcm',
      'ChaCha20-Poly1305': 'chacha20-poly1305'
    };
    return mapping[algorithm] || 'aes-256-gcm';
  }

  private mapHashAlgorithm(algorithm: HashAlgorithm): string {
    const mapping: Record<HashAlgorithm, string> = {
      'SHA-256': 'sha256',
      'SHA-384': 'sha384',
      'SHA-512': 'sha512',
      'BLAKE2b': 'blake2b512'
    };
    return mapping[algorithm] || 'sha256';
  }

  private mapSignatureAlgorithm(algorithm: SignatureAlgorithm): string {
    const mapping: Record<SignatureAlgorithm, string> = {
      'RSA-PSS': 'rsa-pss-sha256',
      'ECDSA': 'sha384',
      'EdDSA': null // EdDSA doesn't need a separate hash algorithm
    };
    return mapping[algorithm] || 'rsa-pss-sha256';
  }

  // Secure memory cleanup
  private secureCleanup(): void {
    // Zero out sensitive data from memory
    if (typeof this.key === 'string') {
      // Can't directly zero strings in JS, but we can reassign
      // TypeScript allows this modification of private properties within the class
      Object.defineProperty(this, 'key', {
        value: '',
        writable: false,
        enumerable: false,
        configurable: false
      });
    }
  }

  // Public cleanup method
  destroy(): void {
    this.secureCleanup();
  }
}

// Worker pool bridge function - adapts between worker interface and typed operations
export async function performCryptoOperation(operation: CryptoOperation): Promise<CryptoOperationResult> {
  const startTime = performance.now();
  const memBefore = process.memoryUsage().heapUsed;

  try {
    const { operation: opType, data, options = {} } = operation;

    switch (opType) {
      case 'encrypt': {
        const key = createSymmetricKey(options.key?.toString('utf8') || 'default-key-32-bytes-for-aes-256');
        const algorithm = (options.algorithm as EncryptionAlgorithm) || 'AES-256-GCM';
        const crypto = new CryptoOperations(key, algorithm, 'SHA-256');

        const plainText = createPlainText(data.toString('utf8'));
        const result = crypto.encrypt(plainText);

        // Pack result into buffer format
        const packedResult = JSON.stringify({
          cipherText: result.cipherText,
          iv: result.iv,
          authTag: result.authTag,
          algorithm: result.algorithm
        });

        const duration = performance.now() - startTime;
        const memAfter = process.memoryUsage().heapUsed;

        crypto.destroy();

        return {
          success: true,
          data: Buffer.from(packedResult, 'utf8'),
          metadata: {
            algorithm,
            performance: {
              duration,
              memoryUsed: memAfter - memBefore
            }
          }
        };
      }

      case 'decrypt': {
        const key = createSymmetricKey(options.key?.toString('utf8') || 'default-key-32-bytes-for-aes-256');
        const algorithm = (options.algorithm as EncryptionAlgorithm) || 'AES-256-GCM';
        const crypto = new CryptoOperations(key, algorithm, 'SHA-256');

        try {
          const packedData = JSON.parse(data.toString('utf8'));
          const result = crypto.decrypt(
            createCipherText(packedData.cipherText),
            packedData.iv,
            packedData.authTag
          );

          const duration = performance.now() - startTime;
          const memAfter = process.memoryUsage().heapUsed;

          crypto.destroy();

          return {
            success: true,
            data: Buffer.from(result.plainText, 'utf8'),
            metadata: {
              algorithm,
              performance: {
                duration,
                memoryUsed: memAfter - memBefore
              }
            }
          };
        } catch (error) {
          crypto.destroy();
          throw error;
        }
      }

      case 'hash': {
        const algorithm = (options.algorithm as HashAlgorithm) || 'SHA-256';
        const crypto = new CryptoOperations(createSymmetricKey(''), 'AES-256-GCM', algorithm);

        const plainText = createPlainText(data.toString('utf8'));
        const result = crypto.hash(plainText);

        const duration = performance.now() - startTime;
        const memAfter = process.memoryUsage().heapUsed;

        crypto.destroy();

        return {
          success: true,
          data: Buffer.from(result.hash, 'utf8'),
          metadata: {
            algorithm,
            performance: {
              duration,
              memoryUsed: memAfter - memBefore
            }
          }
        };
      }

      case 'generateKeyPair': {
        const algorithm = (options.algorithm as SignatureAlgorithm) || 'RSA-PSS';
        const crypto = new CryptoOperations(createSymmetricKey(''), 'AES-256-GCM', 'SHA-256');

        const result = await crypto.generateKeyPair(algorithm);

        const packedResult = JSON.stringify({
          publicKey: result.publicKey,
          privateKey: result.privateKey
        });

        const duration = performance.now() - startTime;
        const memAfter = process.memoryUsage().heapUsed;

        crypto.destroy();

        return {
          success: true,
          data: Buffer.from(packedResult, 'utf8'),
          metadata: {
            algorithm,
            performance: {
              duration,
              memoryUsed: memAfter - memBefore
            }
          }
        };
      }

      case 'sign': {
        const algorithm = (options.algorithm as SignatureAlgorithm) || 'RSA-PSS';
        const crypto = new CryptoOperations(createSymmetricKey(''), 'AES-256-GCM', 'SHA-256');

        if (!options.privateKey) {
          throw new Error('Private key required for signing operation');
        }

        const plainText = createPlainText(data.toString('utf8'));
        const result = crypto.sign(plainText, options.privateKey, algorithm);

        const packedResult = JSON.stringify({
          signature: result.signature,
          algorithm: result.algorithm,
          data: result.data
        });

        const duration = performance.now() - startTime;
        const memAfter = process.memoryUsage().heapUsed;

        crypto.destroy();

        return {
          success: true,
          data: Buffer.from(packedResult, 'utf8'),
          metadata: {
            algorithm,
            performance: {
              duration,
              memoryUsed: memAfter - memBefore
            }
          }
        };
      }

      case 'verify': {
        const algorithm = (options.algorithm as SignatureAlgorithm) || 'RSA-PSS';
        const crypto = new CryptoOperations(createSymmetricKey(''), 'AES-256-GCM', 'SHA-256');

        const verifyData = JSON.parse(data.toString('utf8'));
        if (!verifyData.publicKey || !verifyData.signature || !verifyData.data) {
          throw new Error('PublicKey, signature, and data required for verification');
        }

        const publicKey = createPublicKey(verifyData.publicKey);
        const signature = createSignature(verifyData.signature);
        const plainText = createPlainText(verifyData.data);

        const result = crypto.verify(signature, plainText, publicKey, algorithm);

        const packedResult = JSON.stringify({
          isValid: result.isValid,
          signature: result.signature,
          algorithm: result.algorithm
        });

        const duration = performance.now() - startTime;
        const memAfter = process.memoryUsage().heapUsed;

        crypto.destroy();

        return {
          success: true,
          data: Buffer.from(packedResult, 'utf8'),
          metadata: {
            algorithm,
            performance: {
              duration,
              memoryUsed: memAfter - memBefore
            }
          }
        };
      }

      default:
        return {
          success: false,
          error: `Unsupported operation: ${opType}`
        };
    }

  } catch (error) {
    const duration = performance.now() - startTime;
    const memAfter = process.memoryUsage().heapUsed;

    return {
      success: false,
      error: error instanceof Error ? error.message : 'Unknown error',
      metadata: {
        performance: {
          duration,
          memoryUsed: memAfter - memBefore
        }
      }
    };
  }
}

// Memory-intensive operations that benefit from worker threads
export const CPU_INTENSIVE_OPS = [
  'generateKeyPair',  // RSA 4096-bit key generation
  'sign',             // RSA/ECDSA signing
  'verify',           // Signature verification
  'hash'              // Large data hashing
];

// Utility to check if operation should use worker pool
export function shouldUseWorkerPool(operation: string, dataSize: number = 0): boolean {
  if (CPU_INTENSIVE_OPS.includes(operation)) {
    return true;
  }

  // Use worker pool for large data encryption/decryption (>1MB)
  if ((operation === 'encrypt' || operation === 'decrypt') && dataSize > 1024 * 1024) {
    return true;
  }

  return false;
}