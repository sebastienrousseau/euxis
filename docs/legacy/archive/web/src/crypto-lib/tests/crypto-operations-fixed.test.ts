import { describe, it, expect, beforeEach } from 'vitest';
import { CryptoOperations } from '../src/crypto-operations.js';
import {
  createSymmetricKey,
  createPlainText,
  createCipherText,
  createPublicKey,
  createPrivateKey,
  type SymmetricKey,
  type PlainText,
  type PublicKey,
  type PrivateKey,
  type Signature
} from '../src/types.js';

describe('CryptoOperations - Fixed Tests', () => {
  let cryptoOps: CryptoOperations;
  let testKey: SymmetricKey;

  beforeEach(() => {
    // Use a proper 32-byte key for AES-256
    testKey = createSymmetricKey('this-is-exactly-32-byte-test-key!');
    cryptoOps = new CryptoOperations(testKey, 'AES-256-GCM', 'SHA-256');
  });

  describe('constructor', () => {
    it('should create instance with valid parameters', () => {
      expect(cryptoOps).toBeInstanceOf(CryptoOperations);
    });

    it('should accept AES-192-GCM algorithm', () => {
      const ops = new CryptoOperations(testKey, 'AES-192-GCM', 'SHA-384');
      expect(ops).toBeInstanceOf(CryptoOperations);
    });

    it('should accept AES-128-GCM algorithm', () => {
      const ops = new CryptoOperations(testKey, 'AES-128-GCM', 'SHA-512');
      expect(ops).toBeInstanceOf(CryptoOperations);
    });
  });

  describe('encrypt', () => {
    it('should encrypt plaintext and return EncryptionResult', () => {
      const plainText = createPlainText('Hello World');
      const result = cryptoOps.encrypt(plainText);

      // Test actual cryptographic properties
      expect(result.cipherText).toBeDefined();
      expect(typeof result.cipherText).toBe('string');
      expect(result.algorithm).toBe('AES-256-GCM');
      expect(result.iv).toBeDefined();
      expect(result.authTag).toBeDefined();
      // IV should be 32 hex chars (16 bytes)
      expect(result.iv).toMatch(/^[0-9a-f]{32}$/);
      // AuthTag should be 32 hex chars (16 bytes)
      expect(result.authTag).toMatch(/^[0-9a-f]{32}$/);
    });

    it('should encrypt empty string', () => {
      const plainText = createPlainText('');
      const result = cryptoOps.encrypt(plainText);

      expect(result.cipherText).toBeDefined();
      expect(result.algorithm).toBe('AES-256-GCM');
      expect(result.iv).toMatch(/^[0-9a-f]{32}$/);
      expect(result.authTag).toMatch(/^[0-9a-f]{32}$/);
    });

    it('should encrypt unicode text', () => {
      const plainText = createPlainText('Hello 世界 🌍');
      const result = cryptoOps.encrypt(plainText);

      expect(result.cipherText).toBeDefined();
      expect(result.algorithm).toBe('AES-256-GCM');
      expect(result.iv).toMatch(/^[0-9a-f]{32}$/);
      expect(result.authTag).toMatch(/^[0-9a-f]{32}$/);
    });

    it('should generate different IVs for same plaintext', () => {
      const plainText = createPlainText('Hello World');
      const result1 = cryptoOps.encrypt(plainText);
      const result2 = cryptoOps.encrypt(plainText);

      expect(result1.iv).not.toBe(result2.iv);
      expect(result1.cipherText).not.toBe(result2.cipherText);
    });

    it('should throw error for invalid key length', () => {
      const shortKey = createSymmetricKey('short');
      const shortKeyCrypto = new CryptoOperations(shortKey, 'AES-256-GCM', 'SHA-256');

      expect(() => {
        shortKeyCrypto.encrypt(createPlainText('test'));
      }).toThrow();
    });
  });

  describe('decrypt', () => {
    it('should decrypt ciphertext and return DecryptionResult', () => {
      const originalText = 'Hello World';
      const plainText = createPlainText(originalText);
      const encryptResult = cryptoOps.encrypt(plainText);

      const decryptResult = cryptoOps.decrypt(
        encryptResult.cipherText,
        encryptResult.iv,
        encryptResult.authTag
      );

      expect(decryptResult.plainText).toBe(originalText);
      expect(decryptResult.algorithm).toBe('AES-256-GCM');
    });

    it('should throw error without IV', () => {
      const cipherText = createCipherText('test');

      expect(() => {
        cryptoOps.decrypt(cipherText);
      }).toThrow('IV and authTag required for decryption');
    });

    it('should throw error without authTag', () => {
      const cipherText = createCipherText('test');
      const iv = 'a'.repeat(32);

      expect(() => {
        cryptoOps.decrypt(cipherText, iv);
      }).toThrow('IV and authTag required for decryption');
    });

    it('should handle encrypt/decrypt roundtrip', () => {
      const originalText = 'This is a test message with unicode 世界 🌍';
      const plainText = createPlainText(originalText);

      const encryptResult = cryptoOps.encrypt(plainText);
      const decryptResult = cryptoOps.decrypt(
        encryptResult.cipherText,
        encryptResult.iv,
        encryptResult.authTag
      );

      expect(decryptResult.plainText).toBe(originalText);
    });
  });

  describe('hash', () => {
    it('should hash input and return HashResult', () => {
      const input = 'test input';
      const result = cryptoOps.hash(createPlainText(input));

      expect(result.hash).toBeDefined();
      expect(typeof result.hash).toBe('string');
      expect(result.algorithm).toBe('SHA-256');
      // SHA-256 produces 64 hex chars (32 bytes)
      expect(result.hash).toMatch(/^[0-9a-f]{64}$/);
    });

    it('should hash empty input', () => {
      const result = cryptoOps.hash(createPlainText(''));

      expect(result.hash).toBeDefined();
      expect(result.algorithm).toBe('SHA-256');
      // SHA-256 of empty string is known value
      expect(result.hash).toBe('e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855');
    });

    it('should produce consistent hashes for same input', () => {
      const input = 'consistent test';
      const result1 = cryptoOps.hash(createPlainText(input));
      const result2 = cryptoOps.hash(createPlainText(input));

      expect(result1.hash).toBe(result2.hash);
    });

    it('should produce different hashes for different inputs', () => {
      const result1 = cryptoOps.hash(createPlainText('input1'));
      const result2 = cryptoOps.hash(createPlainText('input2'));

      expect(result1.hash).not.toBe(result2.hash);
    });
  });

  describe('generateKeyPair', () => {
    it('should generate RSA-PSS key pair', async () => {
      const keyPair = await cryptoOps.generateKeyPair('RSA-PSS');

      expect(keyPair.publicKey).toBeDefined();
      expect(keyPair.privateKey).toBeDefined();
      expect(typeof keyPair.publicKey).toBe('string');
      expect(typeof keyPair.privateKey).toBe('string');
      expect(keyPair.publicKey).toContain('-----BEGIN PUBLIC KEY-----');
      expect(keyPair.privateKey).toContain('-----BEGIN PRIVATE KEY-----');
    });

    it('should generate ECDSA key pair', async () => {
      const keyPair = await cryptoOps.generateKeyPair('ECDSA');

      expect(keyPair.publicKey).toBeDefined();
      expect(keyPair.privateKey).toBeDefined();
      expect(keyPair.publicKey).toContain('-----BEGIN PUBLIC KEY-----');
      expect(keyPair.privateKey).toContain('-----BEGIN PRIVATE KEY-----');
    });

    it('should generate EdDSA key pair', async () => {
      const keyPair = await cryptoOps.generateKeyPair('EdDSA');

      expect(keyPair.publicKey).toBeDefined();
      expect(keyPair.privateKey).toBeDefined();
      expect(keyPair.publicKey).toContain('-----BEGIN PUBLIC KEY-----');
      expect(keyPair.privateKey).toContain('-----BEGIN PRIVATE KEY-----');
    });

    it('should throw error for unsupported algorithm', async () => {
      await expect(cryptoOps.generateKeyPair('INVALID' as any))
        .rejects.toThrow('Unsupported algorithm');
    });
  });

  describe('sign and verify', () => {
    it('should sign and verify with RSA-PSS', async () => {
      const keyPair = await cryptoOps.generateKeyPair('RSA-PSS');
      const data = createPlainText('test data');

      const signResult = cryptoOps.sign(data, keyPair.privateKey, 'RSA-PSS');
      expect(signResult.signature).toBeDefined();
      expect(signResult.algorithm).toBe('RSA-PSS');

      const verifyResult = cryptoOps.verify(signResult.signature, keyPair.publicKey, 'RSA-PSS');
      expect(verifyResult.isValid).toBe(true);
    });

    it('should sign and verify with ECDSA', async () => {
      const keyPair = await cryptoOps.generateKeyPair('ECDSA');
      const data = createPlainText('test data');

      const signResult = cryptoOps.sign(data, keyPair.privateKey, 'ECDSA');
      expect(signResult.signature).toBeDefined();
      expect(signResult.algorithm).toBe('ECDSA');

      const verifyResult = cryptoOps.verify(signResult.signature, keyPair.publicKey, 'ECDSA');
      expect(verifyResult.isValid).toBe(true);
    });

    it('should fail verification with wrong key', async () => {
      const keyPair1 = await cryptoOps.generateKeyPair('RSA-PSS');
      const keyPair2 = await cryptoOps.generateKeyPair('RSA-PSS');
      const data = createPlainText('test data');

      const signResult = cryptoOps.sign(data, keyPair1.privateKey, 'RSA-PSS');
      const verifyResult = cryptoOps.verify(signResult.signature, keyPair2.publicKey, 'RSA-PSS');

      expect(verifyResult.isValid).toBe(false);
    });
  });

  describe('Edge Cases and Error Conditions', () => {
    it('should handle very long plaintext', () => {
      const longText = 'A'.repeat(100000);
      const plainText = createPlainText(longText);

      const encryptResult = cryptoOps.encrypt(plainText);
      const decryptResult = cryptoOps.decrypt(
        encryptResult.cipherText,
        encryptResult.iv,
        encryptResult.authTag
      );

      expect(decryptResult.plainText).toBe(longText);
    });

    it('should handle unicode characters properly', () => {
      const unicodeText = '🔐 Crypto 世界 test ñáéíóú àèìòù âêîôû 🌍';
      const plainText = createPlainText(unicodeText);

      const encryptResult = cryptoOps.encrypt(plainText);
      const decryptResult = cryptoOps.decrypt(
        encryptResult.cipherText,
        encryptResult.iv,
        encryptResult.authTag
      );

      expect(decryptResult.plainText).toBe(unicodeText);
    });
  });
});