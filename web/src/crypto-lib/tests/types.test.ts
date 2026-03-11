import { describe, it, expect } from 'vitest';
import {
  createPublicKey,
  createPrivateKey,
  createSymmetricKey,
  createPlainText,
  createCipherText,
  createHashValue,
  createSignature,
  type PublicKey,
  type PrivateKey,
  type SymmetricKey,
  type PlainText,
  type CipherText,
  type HashValue,
  type Signature,
  type EncryptionAlgorithm,
  type HashAlgorithm,
  type SignatureAlgorithm
} from '../src/types.js';

describe('Branded Type Factories', () => {
  describe('createPublicKey', () => {
    it('should create PublicKey from string', () => {
      const key = createPublicKey('test-public-key');
      expect(key).toBe('test-public-key');
    });

    it('should handle empty string', () => {
      const key = createPublicKey('');
      expect(key).toBe('');
    });

    it('should handle special characters', () => {
      const key = createPublicKey('-----BEGIN PUBLIC KEY-----\nMIIBIjANBg...\n-----END PUBLIC KEY-----');
      expect(key).toContain('BEGIN PUBLIC KEY');
    });
  });

  describe('createPrivateKey', () => {
    it('should create PrivateKey from string', () => {
      const key = createPrivateKey('test-private-key');
      expect(key).toBe('test-private-key');
    });

    it('should handle empty string', () => {
      const key = createPrivateKey('');
      expect(key).toBe('');
    });

    it('should handle PEM format', () => {
      const key = createPrivateKey('-----BEGIN PRIVATE KEY-----\nMIIEvgIBADANBg...\n-----END PRIVATE KEY-----');
      expect(key).toContain('BEGIN PRIVATE KEY');
    });
  });

  describe('createSymmetricKey', () => {
    it('should create SymmetricKey from string', () => {
      const key = createSymmetricKey('test-symmetric-key');
      expect(key).toBe('test-symmetric-key');
    });

    it('should handle hex encoded key', () => {
      const key = createSymmetricKey('0123456789abcdef');
      expect(key).toBe('0123456789abcdef');
    });

    it('should handle base64 encoded key', () => {
      const key = createSymmetricKey('dGVzdC1rZXk=');
      expect(key).toBe('dGVzdC1rZXk=');
    });
  });

  describe('createPlainText', () => {
    it('should create PlainText from string', () => {
      const text = createPlainText('Hello World');
      expect(text).toBe('Hello World');
    });

    it('should handle empty string', () => {
      const text = createPlainText('');
      expect(text).toBe('');
    });

    it('should handle unicode characters', () => {
      const text = createPlainText('Hello 世界 🌍');
      expect(text).toBe('Hello 世界 🌍');
    });

    it('should handle newlines and whitespace', () => {
      const text = createPlainText('Line 1\nLine 2\n  Indented');
      expect(text).toContain('\n');
    });
  });

  describe('createCipherText', () => {
    it('should create CipherText from string', () => {
      const cipher = createCipherText('encrypted-data');
      expect(cipher).toBe('encrypted-data');
    });

    it('should handle base64 encoded data', () => {
      const cipher = createCipherText('aGVsbG8gd29ybGQ=');
      expect(cipher).toBe('aGVsbG8gd29ybGQ=');
    });

    it('should handle hex encoded data', () => {
      const cipher = createCipherText('48656c6c6f20576f726c64');
      expect(cipher).toBe('48656c6c6f20576f726c64');
    });
  });

  describe('createHashValue', () => {
    it('should create HashValue from string', () => {
      const hash = createHashValue('sha256-hash-value');
      expect(hash).toBe('sha256-hash-value');
    });

    it('should handle SHA-256 format', () => {
      const hash = createHashValue('2ef7bde608ce5404e97d5f042f95f89f1c232871');
      expect(hash).toHaveLength(40);
    });

    it('should handle SHA-512 format', () => {
      const hash = createHashValue('cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e');
      expect(hash).toHaveLength(128);
    });
  });

  describe('createSignature', () => {
    it('should create Signature from string', () => {
      const sig = createSignature('signature-data');
      expect(sig).toBe('signature-data');
    });

    it('should handle base64 encoded signature', () => {
      const sig = createSignature('MEUCIQDxKHiJ3+CX...');
      expect(sig).toContain('MEU');
    });

    it('should handle DER encoded signature', () => {
      const sig = createSignature('30450220...');
      expect(sig).toContain('3045');
    });
  });
});

describe('Algorithm Types', () => {
  describe('EncryptionAlgorithm', () => {
    it('should accept valid encryption algorithms', () => {
      const algorithms: EncryptionAlgorithm[] = [
        'AES-256-GCM',
        'AES-192-GCM',
        'AES-128-GCM',
        'ChaCha20-Poly1305'
      ];

      algorithms.forEach(alg => {
        expect(alg).toBeDefined();
      });
    });
  });

  describe('HashAlgorithm', () => {
    it('should accept valid hash algorithms', () => {
      const algorithms: HashAlgorithm[] = [
        'SHA-256',
        'SHA-384',
        'SHA-512',
        'BLAKE2b'
      ];

      algorithms.forEach(alg => {
        expect(alg).toBeDefined();
      });
    });
  });

  describe('SignatureAlgorithm', () => {
    it('should accept valid signature algorithms', () => {
      const algorithms: SignatureAlgorithm[] = [
        'RSA-PSS',
        'ECDSA',
        'EdDSA'
      ];

      algorithms.forEach(alg => {
        expect(alg).toBeDefined();
      });
    });
  });
});