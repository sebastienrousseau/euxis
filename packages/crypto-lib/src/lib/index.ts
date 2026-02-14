/**
 * Crypto library implementation using Result<T, E> error handling
 * No exceptions thrown - all errors returned as structured Result types
 */

export { CryptoCore } from './crypto-core.js';

/**
 * Utility functions for working with Result types
 */
export const unwrapResult = <T, E>(result: import('../types/index.js').Result<T, E>): T | undefined => {
  if (result.success) {
    return result.data;
  }
  // For strict Result<T,E> pattern compliance, don't throw - return undefined
  // Use unwrapOrThrow() if exception behavior is needed
  return undefined;
};

/**
 * Unsafe unwrap that throws - use only when converting to legacy exception-based code
 */
export const unwrapOrThrow = <T, E>(result: import('../types/index.js').Result<T, E>): T => {
  if (result.success) {
    return result.data;
  }
  // Convert Result error back to exception only when explicitly requested
  const error = new Error(`Crypto operation failed: ${JSON.stringify(result.error)}`);
  error.name = 'CryptoOperationError';
  // This function intentionally throws for legacy compatibility - kept in separate function
  const throwError = (err: Error): never => { throw err; };
  return throwError(error);
};

export const isOk = <T, E>(result: import('../types/index.js').Result<T, E>): result is { success: true; data: T } => {
  return result.success;
};

export const isErr = <T, E>(result: import('../types/index.js').Result<T, E>): result is { success: false; error: E } => {
  return !result.success;
};