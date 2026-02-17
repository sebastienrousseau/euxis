/**
 * Result<T, E> type for standardized error handling without exceptions
 * Based on Rust's Result enum pattern for predictable error handling
 */

export type Result<T, E = Error> = {
  readonly success: true;
  readonly data: T;
} | {
  readonly success: false;
  readonly error: E;
};

/**
 * Crypto operation error categories with specific context preservation
 * Organized by operation type for better error handling specificity
 */
export enum CryptoErrorType {
  // Key-related errors
  INVALID_KEY = 'INVALID_KEY',
  KEY_GENERATION_FAILED = 'KEY_GENERATION_FAILED',
  KEY_TOO_SHORT = 'KEY_TOO_SHORT',
  KEY_TOO_LONG = 'KEY_TOO_LONG',
  KEY_EXPIRED = 'KEY_EXPIRED',
  KEY_REVOKED = 'KEY_REVOKED',

  // Encryption/Decryption errors
  ENCRYPTION_FAILED = 'ENCRYPTION_FAILED',
  DECRYPTION_FAILED = 'DECRYPTION_FAILED',
  INVALID_CIPHERTEXT = 'INVALID_CIPHERTEXT',
  AUTHENTICATION_FAILED = 'AUTHENTICATION_FAILED',

  // Signature/Verification errors
  SIGNATURE_FAILED = 'SIGNATURE_FAILED',
  VERIFICATION_FAILED = 'VERIFICATION_FAILED',
  INVALID_SIGNATURE = 'INVALID_SIGNATURE',
  SIGNATURE_EXPIRED = 'SIGNATURE_EXPIRED',

  // Algorithm and configuration errors
  INVALID_ALGORITHM = 'INVALID_ALGORITHM',
  UNSUPPORTED_ALGORITHM = 'UNSUPPORTED_ALGORITHM',
  INVALID_PARAMETERS = 'INVALID_PARAMETERS',
  INSUFFICIENT_ENTROPY = 'INSUFFICIENT_ENTROPY',

  // System and infrastructure errors
  HARDWARE_ERROR = 'HARDWARE_ERROR',
  NETWORK_ERROR = 'NETWORK_ERROR',
  RESOURCE_EXHAUSTED = 'RESOURCE_EXHAUSTED',
  TIMEOUT = 'TIMEOUT',

  // Plugin and initialization errors
  PLUGIN_NOT_INITIALIZED = 'PLUGIN_NOT_INITIALIZED',
  PLUGIN_LOAD_FAILED = 'PLUGIN_LOAD_FAILED',
  PLUGIN_NOT_FOUND = 'PLUGIN_NOT_FOUND',

  // Data validation errors
  INVALID_INPUT = 'INVALID_INPUT',
  INVALID_FORMAT = 'INVALID_FORMAT',
  DATA_TOO_LARGE = 'DATA_TOO_LARGE',
  CHECKSUM_MISMATCH = 'CHECKSUM_MISMATCH'
}

/**
 * Structured crypto error with operation context
 */
export interface CryptoError {
  readonly type: CryptoErrorType;
  readonly message: string;
  readonly operation: string;
  readonly context?: Record<string, unknown>;
  readonly cause?: Error;
}

/**
 * Specialized Result types for crypto operations
 */
export type CryptoResult<T> = Result<T, CryptoError>;

/**
 * Helper constructors for Result pattern
 */
export const Ok = <T>(data: T): Result<T, never> => ({
  success: true,
  data
});

export const Err = <E>(error: E): Result<never, E> => ({
  success: false,
  error
});

/**
 * Create a crypto-specific error result
 */
export const CryptoErr = (
  type: CryptoErrorType,
  message: string,
  operation: string,
  context?: Record<string, unknown>,
  cause?: Error
): Result<never, CryptoError> => ({
  success: false,
  error: { type, message, operation, context, cause }
});

/**
 * Utility functions for working with Result types
 */
export const isSuccess = <T, E>(result: Result<T, E>): result is { success: true; data: T } =>
  result.success;

export const isFailure = <T, E>(result: Result<T, E>): result is { success: false; error: E } =>
  !result.success;

/**
 * Map a successful Result to a new value
 */
export const mapResult = <T, U, E>(
  result: Result<T, E>,
  mapper: (value: T) => U
): Result<U, E> => {
  if (result.success) {
    return Ok(mapper(result.data));
  }
  return result;
};

/**
 * Chain Result operations together (flatMap/bind)
 */
export const chainResult = <T, U, E>(
  result: Result<T, E>,
  mapper: (value: T) => Result<U, E>
): Result<U, E> => {
  if (result.success) {
    return mapper(result.data);
  }
  return result;
};

/**
 * Convert a Promise that might throw into a Result
 */
export const wrapAsync = async <T>(
  operation: string,
  promise: Promise<T>
): Promise<CryptoResult<T>> => {
  try {
    const data = await promise;
    return Ok(data);
  } catch (error) {
    return CryptoErr(
      CryptoErrorType.HARDWARE_ERROR,
      `Async operation failed: ${error instanceof Error ? error.message : 'Unknown error'}`,
      operation,
      {},
      error instanceof Error ? error : new Error(String(error))
    );
  }
};

/**
 * Error context builders for common crypto operations
 */
export const createKeyContext = (
  keyType: string,
  algorithm?: string,
  keyLength?: number
): Record<string, unknown> => ({
  keyType,
  ...(algorithm && { algorithm }),
  ...(keyLength && { keyLength })
});

export const createEncryptionContext = (
  algorithm: string,
  dataSize: number,
  keyId?: string
): Record<string, unknown> => ({
  algorithm,
  dataSize,
  ...(keyId && { keyId })
});

export const createSignatureContext = (
  algorithm: string,
  dataSize: number,
  keyId?: string
): Record<string, unknown> => ({
  algorithm,
  dataSize,
  ...(keyId && { keyId })
});