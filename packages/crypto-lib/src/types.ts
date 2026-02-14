// Branded types for cryptographic operations

// Brand utility type
type Brand<T, B> = T & { readonly __brand: B };

// Cryptographic key types
export type PublicKey = Brand<string, 'PublicKey'>;
export type PrivateKey = Brand<string, 'PrivateKey'>;
export type SymmetricKey = Brand<string, 'SymmetricKey'>;

// Data types
export type PlainText = Brand<string, 'PlainText'>;
export type CipherText = Brand<string, 'CipherText'>;
export type HashValue = Brand<string, 'HashValue'>;
export type Signature = Brand<string, 'Signature'>;

// Algorithm types
export type EncryptionAlgorithm = 'AES-256-GCM' | 'AES-192-GCM' | 'AES-128-GCM' | 'ChaCha20-Poly1305';
export type HashAlgorithm = 'SHA-256' | 'SHA-384' | 'SHA-512' | 'BLAKE2b';
export type SignatureAlgorithm = 'RSA-PSS' | 'ECDSA' | 'EdDSA';

// Legacy Result interfaces - DEPRECATED in favor of Result<T,E> pattern
// These are maintained for backward compatibility during migration
// @deprecated Use CryptoResult<T> from types/result.ts instead
export interface EncryptionResult {
  readonly cipherText: CipherText;
  readonly algorithm: EncryptionAlgorithm;
  readonly iv?: string;
  readonly authTag?: string;
}

// @deprecated Use CryptoResult<T> from types/result.ts instead
export interface DecryptionResult {
  readonly plainText: PlainText;
  readonly algorithm: EncryptionAlgorithm;
}

// @deprecated Use CryptoResult<T> from types/result.ts instead
export interface HashResult {
  readonly hash: HashValue;
  readonly algorithm: HashAlgorithm;
}

export interface KeyPair {
  readonly publicKey: PublicKey;
  readonly privateKey: PrivateKey;
}

// @deprecated Use CryptoResult<T> from types/result.ts instead
export interface SignatureResult {
  readonly signature: Signature;
  readonly algorithm: SignatureAlgorithm;
  readonly data: PlainText;
}

// @deprecated Use CryptoResult<T> from types/result.ts instead
export interface VerificationResult {
  readonly isValid: boolean;
  readonly signature: Signature;
  readonly algorithm: SignatureAlgorithm;
}

// Worker pool operation interfaces
export interface CryptoOperationOptions {
  readonly algorithm?: EncryptionAlgorithm | HashAlgorithm | SignatureAlgorithm;
  readonly key?: Buffer;
  readonly iv?: string;
  readonly authTag?: string;
  readonly publicKey?: PublicKey;
  readonly privateKey?: PrivateKey;
  readonly keySize?: number;
}

export interface CryptoOperationRequest {
  readonly operation: 'encrypt' | 'decrypt' | 'hash' | 'sign' | 'verify';
  readonly data: Buffer;
  readonly options?: CryptoOperationOptions;
}

export interface CryptoOperationResponse {
  readonly success: boolean;
  readonly data?: Buffer;
  readonly error?: string;
  readonly metadata?: {
    readonly algorithm?: string;
    readonly keyId?: string;
    readonly signature?: Signature;
  };
}

// Type guards for branded types
export const createPublicKey = (value: string): PublicKey => value as PublicKey;
export const createPrivateKey = (value: string): PrivateKey => value as PrivateKey;
export const createSymmetricKey = (value: string): SymmetricKey => value as SymmetricKey;
export const createPlainText = (value: string): PlainText => value as PlainText;
export const createCipherText = (value: string): CipherText => value as CipherText;
export const createHashValue = (value: string): HashValue => value as HashValue;
export const createSignature = (value: string): Signature => value as Signature;