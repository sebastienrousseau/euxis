import { describe, it, expect, vi } from 'vitest';
import { CryptoCore } from '../src/lib/crypto-core.js';
import { CryptoErrorType } from '../src/types/index.js';

// Mock crypto.subtle for testing
const mockCrypto = {
  subtle: {
    generateKey: vi.fn(),
    exportKey: vi.fn(),
    importKey: vi.fn(),
    encrypt: vi.fn(),
    decrypt: vi.fn()
  },
  randomUUID: vi.fn()
};

// Mock global crypto
Object.defineProperty(global, 'crypto', {
  value: mockCrypto,
  writable: true
});

// Mock btoa/atob for Node.js environment
global.btoa = (str: string) => Buffer.from(str, 'binary').toString('base64');
global.atob = (str: string) => Buffer.from(str, 'base64').toString('binary');

describe('CryptoCore', () => {
  describe('generateKeyPair', () => {
    it('should generate key pair successfully with default algorithm', async () => {
      const mockKeyPair = {
        publicKey: {} as CryptoKey,
        privateKey: {} as CryptoKey
      };
      const mockPublicKeyBuffer = new Uint8Array([1, 2, 3]);
      const mockPrivateKeyBuffer = new Uint8Array([4, 5, 6]);
      const mockKeyId = 'test-key-id';

      mockCrypto.subtle.generateKey.mockResolvedValue(mockKeyPair);
      mockCrypto.subtle.exportKey
        .mockResolvedValueOnce(mockPublicKeyBuffer.buffer)
        .mockResolvedValueOnce(mockPrivateKeyBuffer.buffer);
      mockCrypto.randomUUID.mockReturnValue(mockKeyId);

      const result = await CryptoCore.generateKeyPair();

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data.keyId).toBe(mockKeyId);
        expect(result.data.publicKey).toBeDefined();
        expect(result.data.privateKey).toBeDefined();
      }
    });

    it('should generate key pair with custom algorithm', async () => {
      const mockKeyPair = {
        publicKey: {} as CryptoKey,
        privateKey: {} as CryptoKey
      };
      const mockPublicKeyBuffer = new Uint8Array([1, 2, 3]);
      const mockPrivateKeyBuffer = new Uint8Array([4, 5, 6]);

      mockCrypto.subtle.generateKey.mockResolvedValue(mockKeyPair);
      mockCrypto.subtle.exportKey
        .mockResolvedValueOnce(mockPublicKeyBuffer.buffer)
        .mockResolvedValueOnce(mockPrivateKeyBuffer.buffer);
      mockCrypto.randomUUID.mockReturnValue('test-key-id');

      const result = await CryptoCore.generateKeyPair('ECDSA');

      expect(result.isOk).toBe(true);
      expect(mockCrypto.subtle.generateKey).toHaveBeenCalledWith(
        expect.objectContaining({ name: 'ECDSA' }),
        true,
        ['encrypt', 'decrypt']
      );
    });

    it('should return error when Web Crypto API is not available', async () => {
      // Mock crypto as undefined
      Object.defineProperty(global, 'crypto', {
        value: undefined,
        writable: true
      });

      const result = await CryptoCore.generateKeyPair();

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.HARDWARE_ERROR);
        expect(result.error.message).toBe('Web Crypto API not available');
      }

      // Restore crypto mock
      Object.defineProperty(global, 'crypto', {
        value: mockCrypto,
        writable: true
      });
    });

    it('should return error when key generation fails', async () => {
      mockCrypto.subtle.generateKey.mockRejectedValue(new Error('Key generation failed'));

      const result = await CryptoCore.generateKeyPair();

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.KEY_GENERATION_FAILED);
        expect(result.error.message).toBe('Key generation failed');
      }
    });

    it('should return error when key export fails', async () => {
      const mockKeyPair = {
        publicKey: {} as CryptoKey,
        privateKey: {} as CryptoKey
      };

      mockCrypto.subtle.generateKey.mockResolvedValue(mockKeyPair);
      mockCrypto.subtle.exportKey.mockRejectedValue(new Error('Export failed'));

      const result = await CryptoCore.generateKeyPair();

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.KEY_GENERATION_FAILED);
      }
    });
  });

  describe('encrypt', () => {
    it('should encrypt data successfully', async () => {
      const mockPublicKey = {} as CryptoKey;
      const mockEncryptedBuffer = new Uint8Array([7, 8, 9]);

      mockCrypto.subtle.importKey.mockResolvedValue(mockPublicKey);
      mockCrypto.subtle.encrypt.mockResolvedValue(mockEncryptedBuffer.buffer);

      const result = await CryptoCore.encrypt('test data', 'mock-public-key');

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data.ciphertext).toBeDefined();
      }
    });

    it('should return error for empty data', async () => {
      const result = await CryptoCore.encrypt('', 'mock-public-key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.INVALID_KEY);
        expect(result.error.message).toBe('Data and public key are required');
      }
    });

    it('should return error for empty public key', async () => {
      const result = await CryptoCore.encrypt('test data', '');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.INVALID_KEY);
        expect(result.error.message).toBe('Data and public key are required');
      }
    });

    it('should return error when key import fails', async () => {
      mockCrypto.subtle.importKey.mockRejectedValue(new Error('Import failed'));

      const result = await CryptoCore.encrypt('test data', 'invalid-key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.ENCRYPTION_FAILED);
      }
    });

    it('should return error when encryption fails', async () => {
      const mockPublicKey = {} as CryptoKey;

      mockCrypto.subtle.importKey.mockResolvedValue(mockPublicKey);
      mockCrypto.subtle.encrypt.mockRejectedValue(new Error('Encryption failed'));

      const result = await CryptoCore.encrypt('test data', 'mock-public-key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.ENCRYPTION_FAILED);
      }
    });
  });

  describe('decrypt', () => {
    it('should decrypt data successfully', async () => {
      const mockPrivateKey = {} as CryptoKey;
      const mockDecryptedBuffer = new TextEncoder().encode('decrypted data');

      mockCrypto.subtle.importKey.mockResolvedValue(mockPrivateKey);
      mockCrypto.subtle.decrypt.mockResolvedValue(mockDecryptedBuffer.buffer);

      const result = await CryptoCore.decrypt('Y2lwaGVydGV4dA==', 'mock-private-key');

      expect(result.isOk).toBe(true);
      if (result.isOk) {
        expect(result.data).toBe('decrypted data');
      }
    });

    it('should return error for empty ciphertext', async () => {
      const result = await CryptoCore.decrypt('', 'mock-private-key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.INVALID_KEY);
        expect(result.error.message).toBe('Ciphertext and private key are required');
      }
    });

    it('should return error for empty private key', async () => {
      const result = await CryptoCore.decrypt('Y2lwaGVydGV4dA==', '');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.INVALID_KEY);
        expect(result.error.message).toBe('Ciphertext and private key are required');
      }
    });

    it('should return error when key import fails', async () => {
      mockCrypto.subtle.importKey.mockRejectedValue(new Error('Import failed'));

      const result = await CryptoCore.decrypt('Y2lwaGVydGV4dA==', 'invalid-key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.DECRYPTION_FAILED);
      }
    });

    it('should return error when decryption fails', async () => {
      const mockPrivateKey = {} as CryptoKey;

      mockCrypto.subtle.importKey.mockResolvedValue(mockPrivateKey);
      mockCrypto.subtle.decrypt.mockRejectedValue(new Error('Decryption failed'));

      const result = await CryptoCore.decrypt('Y2lwaGVydGV4dA==', 'mock-private-key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.DECRYPTION_FAILED);
      }
    });

    it('should handle non-Error exceptions in decrypt', async () => {
      const mockPrivateKey = {} as CryptoKey;

      mockCrypto.subtle.importKey.mockResolvedValue(mockPrivateKey);
      mockCrypto.subtle.decrypt.mockRejectedValue('String error');

      const result = await CryptoCore.decrypt('Y2lwaGVydGV4dA==', 'mock-private-key');

      expect(result.isOk).toBe(false);
      if (!result.isOk) {
        expect(result.error.type).toBe(CryptoErrorType.DECRYPTION_FAILED);
        expect(result.error.message).toBe('Decryption failed');
      }
    });
  });
});