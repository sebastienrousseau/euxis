/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2024-2026 Euxis Contributors
 */

/**
 * Crypto library type definitions
 * Exports standardized Result<T, E> pattern and crypto-specific error types
 */

export {
  Result,
  CryptoResult,
  CryptoError,
  CryptoErrorType,
  Ok,
  Err,
  CryptoErr,
  isSuccess,
  isFailure,
  mapResult,
  chainResult,
  wrapAsync,
  createKeyContext,
  createEncryptionContext,
  createSignatureContext
} from './result.js';

export {
  BaseErrorContext,
  KeyErrorContext,
  CryptoOperationContext,
  SignatureOperationContext,
  PluginErrorContext,
  InfrastructureErrorContext,
  ErrorContextChain,
  classifyErrorSeverity,
  createKeyErrorContext,
  createCryptoOperationContext,
  createSignatureOperationContext,
  createPluginErrorContext,
  createInfrastructureErrorContext,
  chainErrorContexts,
  sanitizeErrorContext
} from './error-context.js';

/**
 * Key operation result types
 */
export type KeyGenerationResult = CryptoResult<{
  publicKey: string;
  privateKey: string;
  keyId: string;
}>;

export type KeyValidationResult = CryptoResult<{
  valid: boolean;
  algorithm: string;
  keyLength: number;
}>;

/**
 * Encryption operation result types
 */
export type EncryptionResult = CryptoResult<{
  ciphertext: string;
  nonce?: string;
  tag?: string;
}>;

export type DecryptionResult = CryptoResult<string>;

/**
 * Signature operation result types
 */
export type SignatureResult = CryptoResult<{
  signature: string;
  algorithm: string;
}>;

export type VerificationResult = CryptoResult<{
  verified: boolean;
  algorithm: string;
}>;