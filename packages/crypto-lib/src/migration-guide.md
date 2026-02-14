# Migration Guide: Result<T, E> Error Handling

This guide explains how to migrate from exception-based error handling to the standardized Result<T, E> pattern in the crypto library.

## Overview

The crypto library now uses the Result<T, E> pattern for predictable error handling without exceptions. This provides:

- **Explicit error handling**: All errors are returned as values, not thrown
- **Type safety**: Compile-time guarantees about error handling
- **Structured errors**: Rich error context with operation details
- **No hidden control flow**: No surprise exceptions to catch

## Key Changes

### 1. Return Types

**Before (Exception-based):**
```typescript
// Throws exceptions on error
async function encrypt(data: string, key: string): Promise<{ ciphertext: string }> {
  if (!key) {
    throw new Error('Invalid key');
  }
  // ... encryption logic
  return { ciphertext: 'encrypted_data' };
}
```

**After (Result-based):**
```typescript
import { CryptoResult, Ok, CryptoErr, CryptoErrorType } from './types/result';

// Returns Result<T, CryptoError>
async function encrypt(data: string, key: string): Promise<CryptoResult<{ ciphertext: string }>> {
  if (!key) {
    return CryptoErr(
      CryptoErrorType.INVALID_KEY,
      'Key cannot be empty',
      'encrypt',
      { keyLength: key.length }
    );
  }
  // ... encryption logic
  return Ok({ ciphertext: 'encrypted_data' });
}
```

### 2. Error Handling

**Before (Try-Catch):**
```typescript
try {
  const result = await encrypt(data, key);
  console.log('Encrypted:', result.ciphertext);
} catch (error) {
  console.error('Encryption failed:', error.message);
}
```

**After (Result Pattern):**
```typescript
import { isSuccess, isFailure } from './types/result';

const result = await encrypt(data, key);
if (isSuccess(result)) {
  console.log('Encrypted:', result.data.ciphertext);
} else {
  console.error('Encryption failed:', result.error.message);
  console.error('Error type:', result.error.type);
  console.error('Context:', result.error.context);
}
```

### 3. Plugin Interface Migration

**Before (Exception-based Plugin):**
```typescript
class OldPlugin implements CryptoPlugin {
  async encrypt(data: PlainText, key: SymmetricKey): Promise<EncryptionResult> {
    if (!this.initialized) {
      throw new Error('Plugin not initialized');
    }
    // ... implementation
    return {
      cipherText: createCipherText(encrypted),
      algorithm: 'AES-256-GCM'
    };
  }
}
```

**After (Result-based Plugin):**
```typescript
import { ResultCryptoPlugin, EncryptionResult } from './plugins/result-interface';

class NewPlugin implements ResultCryptoPlugin {
  async encrypt(data: PlainText, key: SymmetricKey): Promise<EncryptionResult> {
    if (!this.initialized) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_NOT_INITIALIZED,
        'Plugin not initialized',
        'encrypt',
        createEncryptionContext('AES-256-GCM', data.length)
      );
    }
    // ... implementation
    return Ok({
      cipherText: createCipherText(encrypted),
      algorithm: 'AES-256-GCM'
    });
  }
}
```

## Migration Steps

### Step 1: Update Dependencies

```typescript
// Add these imports to use Result types
import {
  CryptoResult,
  Ok,
  CryptoErr,
  CryptoErrorType,
  isSuccess,
  isFailure,
  mapResult,
  chainResult
} from './types/result';

// For enhanced error context
import {
  createKeyErrorContext,
  createCryptoOperationContext,
  createSignatureOperationContext
} from './types/error-context';
```

### Step 2: Convert Function Signatures

Replace `Promise<T>` with `Promise<CryptoResult<T>>` for all crypto operations:

```typescript
// Before
async function generateKey(algorithm: string): Promise<string>

// After
async function generateKey(algorithm: string): Promise<CryptoResult<string>>
```

### Step 3: Replace Exception Throwing

**Before:**
```typescript
if (invalid) {
  throw new Error('Validation failed');
}
```

**After:**
```typescript
if (invalid) {
  return CryptoErr(
    CryptoErrorType.INVALID_INPUT,
    'Validation failed',
    'functionName',
    { /* context */ }
  );
}
```

### Step 4: Update Error Handling

Replace try-catch blocks with Result pattern checks:

**Before:**
```typescript
try {
  const result = await operation();
  return processResult(result);
} catch (error) {
  throw new Error(`Operation failed: ${error.message}`);
}
```

**After:**
```typescript
const result = await operation();
if (!result.success) {
  return CryptoErr(
    CryptoErrorType.HARDWARE_ERROR,
    `Operation failed: ${result.error.message}`,
    'parentOperation',
    {},
    new Error(result.error.message)
  );
}
return Ok(processResult(result.data));
```

### Step 5: Chain Operations

Use `chainResult` for sequential operations:

```typescript
const result = await generateKey('AES-256-GCM');
const finalResult = chainResult(result, async (key) => {
  return await encrypt(data, key);
});

if (isSuccess(finalResult)) {
  console.log('Success:', finalResult.data);
}
```

## Error Type Guidelines

### Choose Appropriate Error Types

- **Key Operations**: `INVALID_KEY`, `KEY_GENERATION_FAILED`, `KEY_EXPIRED`
- **Encryption**: `ENCRYPTION_FAILED`, `DECRYPTION_FAILED`, `AUTHENTICATION_FAILED`
- **Signatures**: `SIGNATURE_FAILED`, `VERIFICATION_FAILED`, `INVALID_SIGNATURE`
- **Algorithms**: `INVALID_ALGORITHM`, `UNSUPPORTED_ALGORITHM`
- **System**: `HARDWARE_ERROR`, `NETWORK_ERROR`, `TIMEOUT`
- **Plugins**: `PLUGIN_NOT_INITIALIZED`, `PLUGIN_LOAD_FAILED`

### Provide Rich Context

Always include relevant context in error reports:

```typescript
return CryptoErr(
  CryptoErrorType.ENCRYPTION_FAILED,
  'AES encryption failed due to invalid key size',
  'encrypt',
  createCryptoOperationContext('encrypt', 'AES-256-GCM', data.length, {
    keyId: keyId,
    expectedKeySize: 32,
    actualKeySize: key.length
  }),
  originalError
);
```

## Benefits of Migration

1. **Predictable Error Handling**: No hidden exceptions to catch
2. **Rich Error Context**: Detailed information about what went wrong
3. **Type Safety**: Compile-time guarantees about error handling
4. **Better Testing**: Easier to test error scenarios
5. **Consistent API**: All operations follow the same pattern
6. **Performance**: No exception overhead

## Backward Compatibility

During the migration period:

- Legacy interfaces are marked as `@deprecated`
- Both exception-based and Result-based plugins are supported
- Wrapper functions can convert between patterns if needed

```typescript
// Wrapper to convert Result to exception for legacy code
async function legacyEncrypt(data: string, key: string): Promise<string> {
  const result = await encryptWithResult(data, key);
  if (!result.success) {
    throw new Error(result.error.message);
  }
  return result.data.ciphertext;
}
```

## Testing Result-based Code

```typescript
import { describe, it, expect } from 'vitest';

describe('Result-based encryption', () => {
  it('should return success for valid input', async () => {
    const result = await encrypt(validData, validKey);
    expect(result.success).toBe(true);
    if (result.success) {
      expect(result.data.ciphertext).toBeDefined();
    }
  });

  it('should return specific error for invalid key', async () => {
    const result = await encrypt(validData, '');
    expect(result.success).toBe(false);
    if (!result.success) {
      expect(result.error.type).toBe(CryptoErrorType.INVALID_KEY);
      expect(result.error.operation).toBe('encrypt');
    }
  });
});
```

This migration enables more robust, predictable error handling while maintaining the flexibility to handle complex crypto operations safely.