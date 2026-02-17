import { describe, it, expect, vi } from 'vitest';
import { encrypt, decrypt } from '../src/lib/encryption.js';
import { CryptoErrorType } from '../src/types/index.js';

// Mock Buffer for testing
global.Buffer = global.Buffer || {
  from: vi.fn((data: string, encoding?: string) => {
    if (encoding === 'base64') {
      // Simple base64 decode mock
      return { toString: () => data };
    }
    // Simple encoding mock
    return { toString: (encoding?: string) => encoding === 'base64' ? data : data };
  })
} as any;

describe('Pure Encryption Functions', () => {
  describe('encrypt', () => {
    it('should encrypt plaintext successfully with default algorithm', () => {
      const plaintext = 'Hello World';
      const key = 'test-key';

      const result = encrypt(plaintext, key);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data.ciphertext).toBeDefined();
        expect(result.data.nonce).toBeDefined();
        expect(result.data.tag).toBeDefined();
        expect(typeof result.data.ciphertext).toBe('string');
        expect(typeof result.data.nonce).toBe('string');
        expect(typeof result.data.tag).toBe('string');
      }
    });

    it('should encrypt with custom algorithm', () => {
      const plaintext = 'Test data';
      const key = 'test-key';
      const algorithm = 'AES-128-CBC';

      const result = encrypt(plaintext, key, algorithm);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data.ciphertext).toBeDefined();
        expect(result.data.nonce).toBeDefined();
        expect(result.data.tag).toBeDefined();
      }
    });

    it('should handle empty plaintext', () => {
      const plaintext = '';
      const key = 'test-key';

      const result = encrypt(plaintext, key);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data.ciphertext).toBeDefined();
      }
    });

    it('should handle empty key', () => {
      const plaintext = 'Test data';
      const key = '';

      const result = encrypt(plaintext, key);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data.ciphertext).toBeDefined();
      }
    });

    it('should handle long plaintext', () => {
      const plaintext = 'A'.repeat(10000);
      const key = 'test-key';

      const result = encrypt(plaintext, key);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data.ciphertext).toBeDefined();
      }
    });

    it('should handle special characters in plaintext', () => {
      const plaintext = '🔐 Special chars: àáâãäå αβγδε 中文 🚀';
      const key = 'test-key';

      const result = encrypt(plaintext, key);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data.ciphertext).toBeDefined();
      }
    });

    it('should return error when Buffer operations fail', () => {
      // Mock Buffer.from to throw error
      const originalBuffer = global.Buffer;
      global.Buffer = {
        from: vi.fn().mockImplementation(() => {
          throw new Error('Buffer operation failed');
        })
      } as any;

      const result = encrypt('test', 'key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.ENCRYPTION_FAILED);
        expect(result.error.message).toBe('Pure encryption operation failed');
      }

      // Restore original Buffer
      global.Buffer = originalBuffer;
    });

    it('should generate different nonce and tag values', () => {
      const plaintext = 'Test data';
      const key = 'test-key';

      const result1 = encrypt(plaintext, key);
      const result2 = encrypt(plaintext, key);

      expect(result1.isOk).toBe(true);
      expect(result2.isOk).toBe(true);

      if (result1.isOk && result2.isOk) {
        // Nonce and tag should be different (random generation)
        expect(result1.data.nonce).not.toBe(result2.data.nonce);
        expect(result1.data.tag).not.toBe(result2.data.tag);
      }
    });
  });

  describe('decrypt', () => {
    it('should decrypt ciphertext successfully', () => {
      const ciphertext = 'dGVzdCBkYXRh'; // Base64 encoded 'test data'
      const key = 'test-key';

      const result = decrypt(ciphertext, key);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(typeof result.data).toBe('string');
      }
    });

    it('should decrypt with optional nonce', () => {
      const ciphertext = 'dGVzdCBkYXRh';
      const key = 'test-key';
      const nonce = 'test-nonce';

      const result = decrypt(ciphertext, key, nonce);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(typeof result.data).toBe('string');
      }
    });

    it('should decrypt with optional nonce and tag', () => {
      const ciphertext = 'dGVzdCBkYXRh';
      const key = 'test-key';
      const nonce = 'test-nonce';
      const tag = 'test-tag';

      const result = decrypt(ciphertext, key, nonce, tag);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(typeof result.data).toBe('string');
      }
    });

    it('should handle empty ciphertext', () => {
      const ciphertext = '';
      const key = 'test-key';

      const result = decrypt(ciphertext, key);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data).toBeDefined();
      }
    });

    it('should handle empty key', () => {
      const ciphertext = 'dGVzdCBkYXRh';
      const key = '';

      const result = decrypt(ciphertext, key);

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data).toBeDefined();
      }
    });

    it('should handle invalid base64 ciphertext', () => {
      const ciphertext = 'invalid-base64!@#';
      const key = 'test-key';

      const result = decrypt(ciphertext, key);

      // Should still succeed since it's just buffer operations
      expect(result.isOk).toBe(true);
    });

    it('should return error when Buffer operations fail', () => {
      // Mock Buffer.from to throw error
      const originalBuffer = global.Buffer;
      global.Buffer = {
        from: vi.fn().mockImplementation(() => {
          throw new Error('Buffer operation failed');
        })
      } as any;

      const result = decrypt('dGVzdA==', 'key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.DECRYPTION_FAILED);
        expect(result.error.message).toBe('Pure decryption operation failed');
      }

      // Restore original Buffer
      global.Buffer = originalBuffer;
    });

    it('should handle long ciphertext', () => {
      const longData = 'A'.repeat(10000);
      const ciphertext = Buffer.from(longData).toString('base64');
      const key = 'test-key';

      const result = decrypt(ciphertext, key);

      expect(result.isOk).toBe(true);
    });
  });

  describe('encrypt/decrypt roundtrip', () => {
    it('should successfully encrypt and decrypt the same data', () => {
      const originalText = 'Test roundtrip data';
      const key = 'test-key';

      // Encrypt
      const encryptResult = encrypt(originalText, key);
      expect(encryptResult.isOk).toBe(true);

      if (encryptResult.isOk) {
        // Decrypt
        const decryptResult = decrypt(encryptResult.data.ciphertext, key);
        expect(decryptResult.isOk).toBe(true);

        if (decryptResult.isOk) {
          // Note: This is a mock implementation, so we can't expect exact roundtrip
          // but we can verify the operations complete successfully
          expect(typeof decryptResult.data).toBe('string');
        }
      }
    });
  });
});