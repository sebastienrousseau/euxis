/**
 * Error Context Preservation Utilities
 * Provides standardized context builders for crypto operations
 * to ensure consistent error reporting across the library
 */

import { CryptoErrorType } from './result';

/**
 * Base error context interface
 */
export interface BaseErrorContext {
  readonly timestamp: string;
  readonly operation: string;
  readonly severity: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
}

/**
 * Key operation error context
 */
export interface KeyErrorContext extends BaseErrorContext {
  readonly keyType: 'symmetric' | 'public' | 'private' | 'keypair';
  readonly algorithm?: string;
  readonly keyLength?: number;
  readonly keyId?: string;
  readonly source?: 'generated' | 'imported' | 'derived';
}

/**
 * Encryption/Decryption error context
 */
export interface CryptoOperationContext extends BaseErrorContext {
  readonly algorithm: string;
  readonly dataSize: number;
  readonly keyId?: string;
  readonly mode?: string;
  readonly ivLength?: number;
  readonly tagLength?: number;
}

/**
 * Signature operation error context
 */
export interface SignatureOperationContext extends BaseErrorContext {
  readonly algorithm: string;
  readonly dataSize: number;
  readonly keyId?: string;
  readonly hashAlgorithm?: string;
  readonly signatureLength?: number;
}

/**
 * Plugin operation error context
 */
export interface PluginErrorContext extends BaseErrorContext {
  readonly pluginName: string;
  readonly pluginVersion?: string;
  readonly capabilities?: string[];
  readonly supportedAlgorithms?: string[];
}

/**
 * Network/Infrastructure error context
 */
export interface InfrastructureErrorContext extends BaseErrorContext {
  readonly service?: string;
  readonly endpoint?: string;
  readonly timeout?: number;
  readonly retryCount?: number;
  readonly lastError?: string;
}

/**
 * Error severity classification based on error type and context
 */
export const classifyErrorSeverity = (
  errorType: CryptoErrorType,
  context: Record<string, unknown>
): 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL' => {
  // Critical errors that could compromise security
  const criticalErrors = [
    CryptoErrorType.KEY_EXPIRED,
    CryptoErrorType.KEY_REVOKED,
    CryptoErrorType.AUTHENTICATION_FAILED,
    CryptoErrorType.SIGNATURE_EXPIRED,
    CryptoErrorType.CHECKSUM_MISMATCH,
    CryptoErrorType.INSUFFICIENT_ENTROPY
  ];

  // High severity errors that prevent operation completion
  const highSeverityErrors = [
    CryptoErrorType.INVALID_KEY,
    CryptoErrorType.KEY_GENERATION_FAILED,
    CryptoErrorType.ENCRYPTION_FAILED,
    CryptoErrorType.DECRYPTION_FAILED,
    CryptoErrorType.SIGNATURE_FAILED,
    CryptoErrorType.VERIFICATION_FAILED,
    CryptoErrorType.HARDWARE_ERROR
  ];

  // Medium severity errors that may be recoverable
  const mediumSeverityErrors = [
    CryptoErrorType.INVALID_ALGORITHM,
    CryptoErrorType.UNSUPPORTED_ALGORITHM,
    CryptoErrorType.PLUGIN_NOT_INITIALIZED,
    CryptoErrorType.PLUGIN_LOAD_FAILED,
    CryptoErrorType.NETWORK_ERROR,
    CryptoErrorType.TIMEOUT
  ];

  if (criticalErrors.includes(errorType)) {
    return 'CRITICAL';
  }
  if (highSeverityErrors.includes(errorType)) {
    return 'HIGH';
  }
  if (mediumSeverityErrors.includes(errorType)) {
    return 'MEDIUM';
  }
  return 'LOW';
};

/**
 * Enhanced key operation context builder
 */
export const createKeyErrorContext = (
  operation: string,
  keyType: 'symmetric' | 'public' | 'private' | 'keypair',
  options: {
    algorithm?: string;
    keyLength?: number;
    keyId?: string;
    source?: 'generated' | 'imported' | 'derived';
  } = {}
): KeyErrorContext => ({
  timestamp: new Date().toISOString(),
  operation,
  severity: 'MEDIUM', // Will be overridden based on error type
  keyType,
  ...options
});

/**
 * Enhanced encryption/decryption context builder
 */
export const createCryptoOperationContext = (
  operation: string,
  algorithm: string,
  dataSize: number,
  options: {
    keyId?: string;
    mode?: string;
    ivLength?: number;
    tagLength?: number;
  } = {}
): CryptoOperationContext => ({
  timestamp: new Date().toISOString(),
  operation,
  severity: 'HIGH', // Crypto failures are typically high severity
  algorithm,
  dataSize,
  ...options
});

/**
 * Enhanced signature operation context builder
 */
export const createSignatureOperationContext = (
  operation: string,
  algorithm: string,
  dataSize: number,
  options: {
    keyId?: string;
    hashAlgorithm?: string;
    signatureLength?: number;
  } = {}
): SignatureOperationContext => ({
  timestamp: new Date().toISOString(),
  operation,
  severity: 'HIGH', // Signature failures are typically high severity
  algorithm,
  dataSize,
  ...options
});

/**
 * Plugin operation context builder
 */
export const createPluginErrorContext = (
  operation: string,
  pluginName: string,
  options: {
    pluginVersion?: string;
    capabilities?: string[];
    supportedAlgorithms?: string[];
  } = {}
): PluginErrorContext => ({
  timestamp: new Date().toISOString(),
  operation,
  severity: 'MEDIUM', // Plugin errors are usually recoverable
  pluginName,
  ...options
});

/**
 * Infrastructure operation context builder
 */
export const createInfrastructureErrorContext = (
  operation: string,
  options: {
    service?: string;
    endpoint?: string;
    timeout?: number;
    retryCount?: number;
    lastError?: string;
  } = {}
): InfrastructureErrorContext => ({
  timestamp: new Date().toISOString(),
  operation,
  severity: 'MEDIUM', // Infrastructure errors may be temporary
  ...options
});

/**
 * Error context chain for tracking operation sequences
 */
export interface ErrorContextChain {
  readonly rootContext: BaseErrorContext;
  readonly chainedContexts: BaseErrorContext[];
  readonly totalOperations: number;
}

/**
 * Chain error contexts for complex operations
 */
export const chainErrorContexts = (
  rootContext: BaseErrorContext,
  ...additionalContexts: BaseErrorContext[]
): ErrorContextChain => ({
  rootContext,
  chainedContexts: additionalContexts,
  totalOperations: additionalContexts.length + 1
});

/**
 * Sanitize error context to remove sensitive information
 * for logging and monitoring purposes
 */
export const sanitizeErrorContext = (
  context: Record<string, unknown>
): Record<string, unknown> => {
  const sanitized = { ...context };

  // Remove potentially sensitive fields
  const sensitiveFields = ['key', 'privateKey', 'secret', 'password', 'token', 'data'];
  sensitiveFields.forEach(field => {
    if (field in sanitized) {
      sanitized[field] = '[REDACTED]';
    }
  });

  // Sanitize nested objects
  Object.keys(sanitized).forEach(key => {
    if (typeof sanitized[key] === 'object' && sanitized[key] !== null) {
      sanitized[key] = sanitizeErrorContext(sanitized[key] as Record<string, unknown>);
    }
  });

  return sanitized;
};