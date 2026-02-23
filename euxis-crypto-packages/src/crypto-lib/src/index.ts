/**
 * @euxis/crypto-lib - Main entry point
 * ESM module with dynamic imports for cryptographic algorithms
 */

// Re-export types for external consumers
export * from './types.js';

// Re-export configuration
export * from './config/security.js';

// Default export for convenience
export { CryptoOperations } from './crypto-operations.js';

// Version and metadata
export const CRYPTO_LIB_VERSION = '0.0.2';
export const SUPPORTED_ALGORITHMS = {
  encryption: ['AES-256-GCM', 'AES-192-GCM', 'AES-128-GCM', 'ChaCha20-Poly1305'],
  hash: ['SHA-256', 'SHA-384', 'SHA-512', 'BLAKE2b'],
  signature: ['RSA-PSS', 'ECDSA', 'EdDSA']
} as const;

// Dynamic algorithm loaders to support tree-shaking and lazy loading
export const loadCryptoOperations = async () => {
  const { CryptoOperations } = await import('./crypto-operations.js');
  return CryptoOperations;
};

// Algorithm-specific dynamic imports for better encapsulation
export const loadEncryptionAlgorithm = async (algorithm: 'AES-256-GCM' | 'AES-192-GCM' | 'AES-128-GCM' | 'ChaCha20-Poly1305') => {
  switch (algorithm) {
    case 'AES-256-GCM':
    case 'AES-192-GCM':
    case 'AES-128-GCM':
      // Dynamic import for AES algorithms
      const aesModule = await import('./algorithms/aes.js').catch(() => {
        // Fallback to main operations if algorithm-specific module doesn't exist
        return import('./crypto-operations.js');
      });
      return aesModule;
    case 'ChaCha20-Poly1305':
      // Dynamic import for ChaCha20 algorithm
      const chachaModule = await import('./algorithms/chacha20.js').catch(() => {
        return import('./crypto-operations.js');
      });
      return chachaModule;
    default:
      throw new Error(`Unsupported encryption algorithm: ${algorithm}`);
  }
};

export const loadHashAlgorithm = async (algorithm: 'SHA-256' | 'SHA-384' | 'SHA-512' | 'BLAKE2b') => {
  switch (algorithm) {
    case 'SHA-256':
    case 'SHA-384':
    case 'SHA-512':
      // Dynamic import for SHA algorithms
      const shaModule = await import('./algorithms/sha.js').catch(() => {
        return import('./crypto-operations.js');
      });
      return shaModule;
    case 'BLAKE2b':
      // Dynamic import for BLAKE2b algorithm
      const blakeModule = await import('./algorithms/blake2b.js').catch(() => {
        return import('./crypto-operations.js');
      });
      return blakeModule;
    default:
      throw new Error(`Unsupported hash algorithm: ${algorithm}`);
  }
};

export const loadSignatureAlgorithm = async (algorithm: 'RSA-PSS' | 'ECDSA' | 'EdDSA') => {
  switch (algorithm) {
    case 'RSA-PSS':
      // Dynamic import for RSA-PSS algorithm
      const rsaModule = await import('./algorithms/rsa.js').catch(() => {
        return import('./crypto-operations.js');
      });
      return rsaModule;
    case 'ECDSA':
      // Dynamic import for ECDSA algorithm
      const ecdsaModule = await import('./algorithms/ecdsa.js').catch(() => {
        return import('./crypto-operations.js');
      });
      return ecdsaModule;
    case 'EdDSA':
      // Dynamic import for EdDSA algorithm
      const eddsaModule = await import('./algorithms/eddsa.js').catch(() => {
        return import('./crypto-operations.js');
      });
      return eddsaModule;
    default:
      throw new Error(`Unsupported signature algorithm: ${algorithm}`);
  }
};