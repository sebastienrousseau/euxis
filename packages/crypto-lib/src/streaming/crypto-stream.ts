import { Transform, Readable, Writable } from 'stream';
import { randomBytes, createCipher, createDecipher } from 'crypto';
import { CryptoWorkerPool } from '../performance/worker-pool.js';

export interface StreamingCryptoOptions {
  algorithm?: string;
  key?: Buffer;
  chunkSize?: number;
  useWorkerPool?: boolean;
  workerPoolSize?: number;
}

export class CryptoTransformStream extends Transform {
  private algorithm: string;
  private key: Buffer;
  private chunkSize: number;
  private workerPool?: CryptoWorkerPool;
  private operation: 'encrypt' | 'decrypt';

  constructor(
    operation: 'encrypt' | 'decrypt',
    options: StreamingCryptoOptions = {}
  ) {
    super({
      objectMode: false,
      highWaterMark: options.chunkSize || 64 * 1024 // 64KB default
    });

    this.operation = operation;
    this.algorithm = options.algorithm || 'aes-256-cbc';
    this.key = options.key || randomBytes(32);
    this.chunkSize = options.chunkSize || 64 * 1024;

    if (options.useWorkerPool) {
      this.workerPool = new CryptoWorkerPool(options.workerPoolSize);
    }
  }

  async _transform(
    chunk: Buffer,
    encoding: BufferEncoding,
    callback: (error?: Error, data?: Buffer) => void
  ): Promise<void> {
    try {
      if (this.workerPool) {
        // Use worker thread for CPU-intensive operations
        const result = await this.workerPool.execute({
          id: `stream-${Date.now()}-${Math.random()}`,
          operation: this.operation,
          data: chunk,
          options: {
            algorithm: this.algorithm,
            key: this.key
          }
        });

        if (result.success && result.result) {
          callback(null, result.result);
        } else {
          callback(new Error(result.error || 'Crypto operation failed'));
        }
      } else {
        // Process in main thread for smaller chunks
        const processed = await this.processCrypto(chunk);
        callback(null, processed);
      }
    } catch (error) {
      callback(error instanceof Error ? error : new Error('Crypto transform failed'));
    }
  }

  private async processCrypto(data: Buffer): Promise<Buffer> {
    // Import crypto operations to avoid circular dependencies
    const { performCryptoOperation } = await import('../crypto-operations.js');

    const result = await performCryptoOperation({
      operation: this.operation,
      data,
      options: {
        algorithm: this.algorithm,
        key: this.key
      }
    });

    if (result.success && result.data) {
      return result.data;
    } else {
      throw new Error(result.error || 'Crypto operation failed');
    }
  }

  async _flush(callback: (error?: Error) => void): Promise<void> {
    try {
      // Cleanup worker pool if used
      if (this.workerPool) {
        await this.workerPool.terminate();
      }
      callback();
    } catch (error) {
      callback(error instanceof Error ? error : new Error('Stream flush failed'));
    }
  }

  // Secure memory cleanup
  destroy(error?: Error): this {
    // Zero out sensitive data
    if (this.key) {
      this.key.fill(0);
    }

    if (this.workerPool) {
      this.workerPool.terminate().catch(console.error);
    }

    return super.destroy(error);
  }
}

export class SecureReadableStream extends Readable {
  private source: Readable;
  private cryptoStream: CryptoTransformStream;

  constructor(source: Readable, options: StreamingCryptoOptions = {}) {
    super();
    this.source = source;
    this.cryptoStream = new CryptoTransformStream('decrypt', options);

    // Pipe source through crypto transform
    this.source.pipe(this.cryptoStream).on('data', (chunk) => {
      this.push(chunk);
    }).on('end', () => {
      this.push(null);
    }).on('error', (error) => {
      this.destroy(error);
    });
  }

  _read(): void {
    // Handled by pipe setup
  }

  destroy(error?: Error): this {
    this.cryptoStream?.destroy();
    this.source?.destroy();
    return super.destroy(error);
  }
}

export class SecureWritableStream extends Writable {
  private destination: Writable;
  private cryptoStream: CryptoTransformStream;

  constructor(destination: Writable, options: StreamingCryptoOptions = {}) {
    super();
    this.destination = destination;
    this.cryptoStream = new CryptoTransformStream('encrypt', options);

    // Pipe crypto output to destination
    this.cryptoStream.pipe(this.destination);
  }

  _write(
    chunk: Buffer,
    encoding: BufferEncoding,
    callback: (error?: Error) => void
  ): void {
    this.cryptoStream.write(chunk, encoding, callback);
  }

  _final(callback: (error?: Error) => void): void {
    this.cryptoStream.end(callback);
  }

  destroy(error?: Error): this {
    this.cryptoStream?.destroy();
    this.destination?.destroy();
    return super.destroy(error);
  }
}

// Utility functions for streaming crypto operations
export async function createEncryptStream(
  options: StreamingCryptoOptions = {}
): Promise<CryptoTransformStream> {
  return new CryptoTransformStream('encrypt', options);
}

export async function createDecryptStream(
  options: StreamingCryptoOptions = {}
): Promise<CryptoTransformStream> {
  return new CryptoTransformStream('decrypt', options);
}

// Memory cleanup utilities
export function secureCleanup(buffer: Buffer): void {
  if (Buffer.isBuffer(buffer)) {
    buffer.fill(0);
  }
}

export function secureCleanupArray(buffers: Buffer[]): void {
  buffers.forEach(secureCleanup);
}