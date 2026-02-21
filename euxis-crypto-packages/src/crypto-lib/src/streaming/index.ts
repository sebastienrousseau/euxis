/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2024-2026 Euxis Contributors
 */

export {
  CryptoTransformStream,
  SecureReadableStream,
  SecureWritableStream,
  createEncryptStream,
  createDecryptStream,
  secureCleanup,
  secureCleanupArray,
  type StreamingCryptoOptions
} from './crypto-stream.js';

// Re-export for convenience
export * from './crypto-stream.js';