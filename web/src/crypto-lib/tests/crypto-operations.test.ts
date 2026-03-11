import { describe, it, expect, beforeEach } from 'vitest';
import { CryptoOperations } from '../src/crypto-operations.js';
import {
  createSymmetricKey,
  createPlainText,
  createCipherText,
  createPublicKey,
  createPrivateKey,
  createSignature,
  type SymmetricKey,
  type PlainText,
  type CipherText,
  type PublicKey,
  type PrivateKey,
  type Signature
} from '../src/types.js';

describe('CryptoOperations', () => {
  let cryptoOps: CryptoOperations;
  let testKey: SymmetricKey;

  beforeEach(() => {
    testKey = createSymmetricKey('test-symmetric-key');
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

    it('should accept ChaCha20-Poly1305 algorithm', () => {
      const ops = new CryptoOperations(testKey, 'ChaCha20-Poly1305', 'BLAKE2b');
      expect(ops).toBeInstanceOf(CryptoOperations);
    });
  });

  describe('encrypt', () => {
    it('should encrypt plaintext and return EncryptionResult', () => {
      const plainText = createPlainText('Hello World');
      const result = cryptoOps.encrypt(plainText);

      expect(result.cipherText).toBe('encrypted_Hello World' as CipherText);
      expect(result.algorithm).toBe('AES-256-GCM');
      expect(result.iv).toBe('mock_iv');
      expect(result.authTag).toBe('mock_auth_tag');
    });

    it('should encrypt empty string', () => {
      const plainText = createPlainText('');
      const result = cryptoOps.encrypt(plainText);

      expect(result.cipherText).toBe('encrypted_' as CipherText);
      expect(result.algorithm).toBe('AES-256-GCM');
    });

    it('should encrypt unicode text', () => {
      const plainText = createPlainText('Hello 世界 🌍');
      const result = cryptoOps.encrypt(plainText);

      expect(result.cipherText).toBe('encrypted_Hello 世界 🌍' as CipherText);
      expect(result.algorithm).toBe('AES-256-GCM');
    });

    it('should encrypt large text', () => {
      const largeText = 'A'.repeat(10000);
      const plainText = createPlainText(largeText);
      const result = cryptoOps.encrypt(plainText);

      expect(result.cipherText).toContain('encrypted_');
      expect(result.algorithm).toBe('AES-256-GCM');
    });
  });

  describe('decrypt', () => {
    it('should decrypt ciphertext and return DecryptionResult', () => {
      const cipherText = createCipherText('test-encrypted-data');
      const result = cryptoOps.decrypt(cipherText);

      expect(result.plainText).toBe('decrypted_test-encrypted-data' as PlainText);
      expect(result.algorithm).toBe('AES-256-GCM');
    });

    it('should decrypt empty ciphertext', () => {
      const cipherText = createCipherText('');
      const result = cryptoOps.decrypt(cipherText);

      expect(result.plainText).toBe('decrypted_' as PlainText);
    });

    it('should handle malformed ciphertext', () => {
      const cipherText = createCipherText('malformed-data');
      const result = cryptoOps.decrypt(cipherText);

      expect(result.plainText).toBe('decrypted_malformed-data' as PlainText);
      expect(result.algorithm).toBe('AES-256-GCM');
    });
  });

  describe('hash', () => {
    it('should hash input and return HashResult', () => {
      const input = createPlainText('test input');
      const result = cryptoOps.hash(input);

      expect(result.hash).toBe('hash_test input');
      expect(result.algorithm).toBe('SHA-256');
    });

    it('should hash empty input', () => {
      const input = createPlainText('');
      const result = cryptoOps.hash(input);

      expect(result.hash).toBe('hash_');
      expect(result.algorithm).toBe('SHA-256');
    });

    it('should hash unicode input', () => {
      const input = createPlainText('Hello 世界');
      const result = cryptoOps.hash(input);

      expect(result.hash).toBe('hash_Hello 世界');
      expect(result.algorithm).toBe('SHA-256');
    });

    it('should handle different hash algorithms', () => {
      const sha384Ops = new CryptoOperations(testKey, 'AES-256-GCM', 'SHA-384');
      const sha512Ops = new CryptoOperations(testKey, 'AES-256-GCM', 'SHA-512');
      const blake2bOps = new CryptoOperations(testKey, 'AES-256-GCM', 'BLAKE2b');

      const input = createPlainText('test');

      expect(sha384Ops.hash(input).algorithm).toBe('SHA-384');
      expect(sha512Ops.hash(input).algorithm).toBe('SHA-512');
      expect(blake2bOps.hash(input).algorithm).toBe('BLAKE2b');
    });
  });

  describe('generateKeyPair', () => {
    it('should generate a key pair', () => {
      const keyPair = cryptoOps.generateKeyPair();

      expect(keyPair.publicKey).toBe('mock_public_key' as PublicKey);
      expect(keyPair.privateKey).toBe('mock_private_key' as PrivateKey);
    });

    it('should generate consistent key pairs', () => {
      const keyPair1 = cryptoOps.generateKeyPair();
      const keyPair2 = cryptoOps.generateKeyPair();

      // Mock implementation returns same values - this would be different in real implementation
      expect(keyPair1.publicKey).toBe(keyPair2.publicKey);
      expect(keyPair1.privateKey).toBe(keyPair2.privateKey);
    });
  });

  describe('sign', () => {
    let privateKey: PrivateKey;

    beforeEach(() => {
      privateKey = createPrivateKey('test-private-key');
    });

    it('should sign data and return SignatureResult', () => {
      const data = createPlainText('data to sign');
      const result = cryptoOps.sign(data, privateKey, 'RSA-PSS');

      expect(result.signature).toBe('signature_of_data to sign' as Signature);
      expect(result.algorithm).toBe('RSA-PSS');
      expect(result.data).toBe(data);
    });

    it('should sign empty data', () => {
      const data = createPlainText('');
      const result = cryptoOps.sign(data, privateKey, 'ECDSA');

      expect(result.signature).toBe('signature_of_' as Signature);
      expect(result.algorithm).toBe('ECDSA');
    });

    it('should handle different signature algorithms', () => {
      const data = createPlainText('test');

      const rsaResult = cryptoOps.sign(data, privateKey, 'RSA-PSS');
      const ecdsaResult = cryptoOps.sign(data, privateKey, 'ECDSA');
      const eddsaResult = cryptoOps.sign(data, privateKey, 'EdDSA');

      expect(rsaResult.algorithm).toBe('RSA-PSS');
      expect(ecdsaResult.algorithm).toBe('ECDSA');
      expect(eddsaResult.algorithm).toBe('EdDSA');
    });

    it('should sign unicode data', () => {
      const data = createPlainText('签名测试 🔐');
      const result = cryptoOps.sign(data, privateKey, 'RSA-PSS');

      expect(result.signature).toContain('signature_of_签名测试 🔐');
    });
  });

  describe('verify', () => {
    let publicKey: PublicKey;
    let signature: Signature;

    beforeEach(() => {
      publicKey = createPublicKey('test-public-key');
      signature = createSignature('test-signature');
    });

    it('should verify signature and return VerificationResult', () => {
      const result = cryptoOps.verify(signature, publicKey, 'RSA-PSS');

      expect(result.isValid).toBe(true);
      expect(result.signature).toBe(signature);
      expect(result.algorithm).toBe('RSA-PSS');
    });

    it('should handle different signature algorithms', () => {
      const rsaResult = cryptoOps.verify(signature, publicKey, 'RSA-PSS');
      const ecdsaResult = cryptoOps.verify(signature, publicKey, 'ECDSA');
      const eddsaResult = cryptoOps.verify(signature, publicKey, 'EdDSA');

      expect(rsaResult.algorithm).toBe('RSA-PSS');
      expect(ecdsaResult.algorithm).toBe('ECDSA');
      expect(eddsaResult.algorithm).toBe('EdDSA');

      // Mock implementation always returns true - real implementation would validate
      expect(rsaResult.isValid).toBe(true);
      expect(ecdsaResult.isValid).toBe(true);
      expect(eddsaResult.isValid).toBe(true);
    });

    it('should handle malformed signature', () => {
      const malformedSig = createSignature('malformed');
      const result = cryptoOps.verify(malformedSig, publicKey, 'ECDSA');

      // Mock implementation always returns true
      expect(result.isValid).toBe(true);
      expect(result.signature).toBe(malformedSig);
    });
  });

  describe('Edge Cases and Error Conditions', () => {
    it('should handle very long plaintext', () => {
      const longText = 'A'.repeat(1000000);
      const plainText = createPlainText(longText);
      const result = cryptoOps.encrypt(plainText);

      expect(result.cipherText).toContain('encrypted_');
      expect(result.algorithm).toBe('AES-256-GCM');
    });

    it('should handle special characters in keys', () => {
      const specialKey = createSymmetricKey('key-with-symbols!@#$%^&*()');
      const ops = new CryptoOperations(specialKey, 'AES-256-GCM', 'SHA-256');

      expect(ops).toBeInstanceOf(CryptoOperations);
    });

    it('should handle null-like strings', () => {
      const nullLike = createPlainText('null');
      const undefinedLike = createPlainText('undefined');

      const result1 = cryptoOps.encrypt(nullLike);
      const result2 = cryptoOps.encrypt(undefinedLike);

      expect(result1.cipherText).toBe('encrypted_null' as CipherText);
      expect(result2.cipherText).toBe('encrypted_undefined' as CipherText);
    });
  });
});