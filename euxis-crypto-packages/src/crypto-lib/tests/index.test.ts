import { describe, it, expect } from 'vitest';
import {
  CryptoOperations,
  CRYPTO_LIB_VERSION,
  SUPPORTED_ALGORITHMS,
  loadCryptoOperations,
  loadEncryptionAlgorithm,
  loadHashAlgorithm,
  loadSignatureAlgorithm
} from '../src/index.js';
import {
  createSymmetricKey,
  createPlainText,
  type EncryptionAlgorithm,
  type HashAlgorithm,
  type SignatureAlgorithm
} from '../src/types.js';

describe('Crypto Library Index', () => {
  describe('Exports', () => {
    it('should export CryptoOperations class', () => {
      expect(CryptoOperations).toBeDefined();
      expect(typeof CryptoOperations).toBe('function');
    });

    it('should export version constant', () => {
      expect(CRYPTO_LIB_VERSION).toBe('0.1.0');
    });

    it('should export supported algorithms', () => {
      expect(SUPPORTED_ALGORITHMS).toEqual({
        encryption: ['AES-256-GCM', 'AES-192-GCM', 'AES-128-GCM', 'ChaCha20-Poly1305'],
        hash: ['SHA-256', 'SHA-384', 'SHA-512', 'BLAKE2b'],
        signature: ['RSA-PSS', 'ECDSA', 'EdDSA']
      });
    });

    it('should have readonly algorithms object', () => {
      // TypeScript readonly - can't test at runtime, but we can verify structure
      expect(SUPPORTED_ALGORITHMS.encryption).toContain('AES-256-GCM');
      expect(SUPPORTED_ALGORITHMS.hash).toContain('SHA-256');
      expect(SUPPORTED_ALGORITHMS.signature).toContain('RSA-PSS');
    });
  });

  describe('loadCryptoOperations', () => {
    it('should dynamically load CryptoOperations', async () => {
      const LoadedCryptoOps = await loadCryptoOperations();
      expect(LoadedCryptoOps).toBeDefined();
      expect(LoadedCryptoOps).toBe(CryptoOperations);
    });

    it('should return a constructor function', async () => {
      const LoadedCryptoOps = await loadCryptoOperations();
      const key = createSymmetricKey('test');
      const instance = new LoadedCryptoOps(key, 'AES-256-GCM', 'SHA-256');
      expect(instance).toBeInstanceOf(LoadedCryptoOps);
    });
  });

  describe('loadEncryptionAlgorithm', () => {
    it('should load AES-256-GCM algorithm', async () => {
      const module = await loadEncryptionAlgorithm('AES-256-GCM');
      expect(module).toBeDefined();
      // Should fallback to crypto-operations module since algorithm-specific doesn't exist
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should load AES-192-GCM algorithm', async () => {
      const module = await loadEncryptionAlgorithm('AES-192-GCM');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should load AES-128-GCM algorithm', async () => {
      const module = await loadEncryptionAlgorithm('AES-128-GCM');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should load ChaCha20-Poly1305 algorithm', async () => {
      const module = await loadEncryptionAlgorithm('ChaCha20-Poly1305');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should throw error for unsupported algorithm', async () => {
      await expect(
        loadEncryptionAlgorithm('INVALID-ALGORITHM' as EncryptionAlgorithm)
      ).rejects.toThrow('Unsupported encryption algorithm: INVALID-ALGORITHM');
    });
  });

  describe('loadHashAlgorithm', () => {
    it('should load SHA-256 algorithm', async () => {
      const module = await loadHashAlgorithm('SHA-256');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should load SHA-384 algorithm', async () => {
      const module = await loadHashAlgorithm('SHA-384');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should load SHA-512 algorithm', async () => {
      const module = await loadHashAlgorithm('SHA-512');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should load BLAKE2b algorithm', async () => {
      const module = await loadHashAlgorithm('BLAKE2b');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should throw error for unsupported algorithm', async () => {
      await expect(
        loadHashAlgorithm('MD5' as HashAlgorithm)
      ).rejects.toThrow('Unsupported hash algorithm: MD5');
    });
  });

  describe('loadSignatureAlgorithm', () => {
    it('should load RSA-PSS algorithm', async () => {
      const module = await loadSignatureAlgorithm('RSA-PSS');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should load ECDSA algorithm', async () => {
      const module = await loadSignatureAlgorithm('ECDSA');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should load EdDSA algorithm', async () => {
      const module = await loadSignatureAlgorithm('EdDSA');
      expect(module).toBeDefined();
      expect(module.CryptoOperations).toBeDefined();
    });

    it('should throw error for unsupported algorithm', async () => {
      await expect(
        loadSignatureAlgorithm('DSA' as SignatureAlgorithm)
      ).rejects.toThrow('Unsupported signature algorithm: DSA');
    });
  });

  describe('Tree-shaking and Lazy Loading', () => {
    it('should support dynamic imports for all encryption algorithms', () => {
      SUPPORTED_ALGORITHMS.encryption.forEach(async (algorithm) => {
        const module = await loadEncryptionAlgorithm(algorithm);
        expect(module).toBeDefined();
      });
    });

    it('should support dynamic imports for all hash algorithms', () => {
      SUPPORTED_ALGORITHMS.hash.forEach(async (algorithm) => {
        const module = await loadHashAlgorithm(algorithm);
        expect(module).toBeDefined();
      });
    });

    it('should support dynamic imports for all signature algorithms', () => {
      SUPPORTED_ALGORITHMS.signature.forEach(async (algorithm) => {
        const module = await loadSignatureAlgorithm(algorithm);
        expect(module).toBeDefined();
      });
    });
  });

  describe('Integration Tests', () => {
    it('should work end-to-end with dynamic loading', async () => {
      const CryptoOpsClass = await loadCryptoOperations();
      const key = createSymmetricKey('integration-test-key');
      const cryptoOps = new CryptoOpsClass(key, 'AES-256-GCM', 'SHA-256');

      const plainText = createPlainText('Integration test data');
      const encrypted = cryptoOps.encrypt(plainText);
      const decrypted = cryptoOps.decrypt(encrypted.cipherText);

      expect(decrypted.plainText).toContain('Integration test data');
    });

    it('should handle multiple algorithm loading', async () => {
      const [encModule, hashModule, sigModule] = await Promise.all([
        loadEncryptionAlgorithm('AES-256-GCM'),
        loadHashAlgorithm('SHA-256'),
        loadSignatureAlgorithm('RSA-PSS')
      ]);

      expect(encModule).toBeDefined();
      expect(hashModule).toBeDefined();
      expect(sigModule).toBeDefined();
    });
  });
});