/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2024-2026 Euxis Contributors
 */

// Performance benchmarks for crypto operations with and without worker pools
import { randomBytes } from 'crypto';
import { CryptoWorkerPool } from './worker-pool.js';
import { CryptoTransformStream, createEncryptStream, createDecryptStream } from '../streaming/crypto-stream.js';
import { CryptoOperations, shouldUseWorkerPool, CPU_INTENSIVE_OPS } from '../crypto-operations.js';
import { createSymmetricKey, createPlainText } from '../types.js';

export interface BenchmarkResult {
  operation: string;
  mode: 'main-thread' | 'worker-pool' | 'streaming';
  iterations: number;
  totalTime: number;
  averageTime: number;
  throughput: number;
  memoryUsage: {
    peak: number;
    average: number;
  };
  errors: number;
}

export interface BenchmarkSuite {
  name: string;
  results: BenchmarkResult[];
  summary: {
    totalOperations: number;
    totalTime: number;
    workerPoolSpeedup: Record<string, number>;
    recommendations: string[];
  };
}

export class CryptoBenchmark {
  private workerPool: CryptoWorkerPool;
  private memoryBaseline: number;

  constructor(workerPoolSize?: number) {
    this.workerPool = new CryptoWorkerPool(workerPoolSize);
    this.memoryBaseline = process.memoryUsage().heapUsed;
  }

  /**
   * Benchmark single-threaded crypto operations
   */
  async benchmarkMainThread(operation: string, iterations: number = 100): Promise<BenchmarkResult> {
    const startTime = performance.now();
    const memoryReadings: number[] = [];
    let errors = 0;

    for (let i = 0; i < iterations; i++) {
      try {
        const memBefore = process.memoryUsage().heapUsed;
        await this.executeOperation(operation, 'main-thread');
        const memAfter = process.memoryUsage().heapUsed;
        memoryReadings.push(memAfter - memBefore);

        // Force GC periodically to prevent memory buildup
        if (i % 10 === 0 && global.gc) {
          global.gc();
        }
      } catch (error) {
        errors++;
        console.warn(`Main thread operation ${operation} failed:`, error);
      }
    }

    const totalTime = performance.now() - startTime;
    const averageTime = totalTime / iterations;
    const throughput = iterations / (totalTime / 1000); // ops/sec

    return {
      operation,
      mode: 'main-thread',
      iterations,
      totalTime,
      averageTime,
      throughput,
      memoryUsage: {
        peak: Math.max(...memoryReadings),
        average: memoryReadings.reduce((a, b) => a + b, 0) / memoryReadings.length
      },
      errors
    };
  }

  /**
   * Benchmark worker pool crypto operations
   */
  async benchmarkWorkerPool(operation: string, iterations: number = 100): Promise<BenchmarkResult> {
    const startTime = performance.now();
    const memoryReadings: number[] = [];
    let errors = 0;

    // Execute operations in parallel batches to maximize worker pool utilization
    const batchSize = this.workerPool.getPoolStats().poolSize * 2;
    const batches = Math.ceil(iterations / batchSize);

    for (let batch = 0; batch < batches; batch++) {
      const batchPromises: Promise<any>[] = [];
      const currentBatchSize = Math.min(batchSize, iterations - batch * batchSize);

      for (let i = 0; i < currentBatchSize; i++) {
        const memBefore = process.memoryUsage().heapUsed;

        const promise = this.executeOperation(operation, 'worker-pool')
          .then(() => {
            const memAfter = process.memoryUsage().heapUsed;
            memoryReadings.push(memAfter - memBefore);
          })
          .catch((error) => {
            errors++;
            console.warn(`Worker pool operation ${operation} failed:`, error);
          });

        batchPromises.push(promise);
      }

      await Promise.all(batchPromises);
    }

    const totalTime = performance.now() - startTime;
    const averageTime = totalTime / iterations;
    const throughput = iterations / (totalTime / 1000); // ops/sec

    return {
      operation,
      mode: 'worker-pool',
      iterations,
      totalTime,
      averageTime,
      throughput,
      memoryUsage: {
        peak: Math.max(...memoryReadings),
        average: memoryReadings.reduce((a, b) => a + b, 0) / memoryReadings.length
      },
      errors
    };
  }

  /**
   * Benchmark streaming crypto operations for large data
   */
  async benchmarkStreaming(operation: string, dataSize: number = 1024 * 1024): Promise<BenchmarkResult> {
    const startTime = performance.now();
    const memBefore = process.memoryUsage().heapUsed;
    let errors = 0;

    try {
      const data = randomBytes(dataSize);
      await this.executeStreamingOperation(operation, data);
    } catch (error) {
      errors++;
      console.warn(`Streaming operation ${operation} failed:`, error);
    }

    const totalTime = performance.now() - startTime;
    const memAfter = process.memoryUsage().heapUsed;
    const throughput = (dataSize / (totalTime / 1000)) / (1024 * 1024); // MB/sec

    return {
      operation,
      mode: 'streaming',
      iterations: 1,
      totalTime,
      averageTime: totalTime,
      throughput,
      memoryUsage: {
        peak: memAfter - memBefore,
        average: memAfter - memBefore
      },
      errors
    };
  }

  /**
   * Execute a single crypto operation in the specified mode
   */
  private async executeOperation(operation: string, mode: 'main-thread' | 'worker-pool'): Promise<any> {
    const testData = Buffer.from('Hello, Euxis! This is performance test data.', 'utf8');
    const taskId = `bench-${Date.now()}-${Math.random()}`;

    switch (operation) {
      case 'encrypt':
        if (mode === 'worker-pool') {
          return await this.workerPool.execute({
            id: taskId,
            operation: 'encrypt',
            data: testData,
            options: { algorithm: 'AES-256-GCM' }
          });
        } else {
          const crypto = new CryptoOperations(
            createSymmetricKey('test-key-32-bytes-for-aes-256'),
            'AES-256-GCM',
            'SHA-256'
          );
          const result = crypto.encrypt(createPlainText(testData.toString('utf8')));
          crypto.destroy();
          return result;
        }

      case 'generateKeyPair':
        if (mode === 'worker-pool') {
          return await this.workerPool.execute({
            id: taskId,
            operation: 'generateKeyPair',
            data: Buffer.alloc(0),
            options: { algorithm: 'RSA-PSS' }
          });
        } else {
          const crypto = new CryptoOperations(
            createSymmetricKey(''),
            'AES-256-GCM',
            'SHA-256'
          );
          const result = await crypto.generateKeyPair('RSA-PSS');
          crypto.destroy();
          return result;
        }

      case 'hash':
        const largeData = randomBytes(1024 * 100); // 100KB for hash testing
        if (mode === 'worker-pool') {
          return await this.workerPool.execute({
            id: taskId,
            operation: 'hash',
            data: largeData,
            options: { algorithm: 'SHA-256' }
          });
        } else {
          const crypto = new CryptoOperations(
            createSymmetricKey(''),
            'AES-256-GCM',
            'SHA-256'
          );
          const result = crypto.hash(createPlainText(largeData.toString('hex')));
          crypto.destroy();
          return result;
        }

      default:
        throw new Error(`Unsupported benchmark operation: ${operation}`);
    }
  }

  /**
   * Execute streaming operation for large data processing
   */
  private async executeStreamingOperation(operation: string, data: Buffer): Promise<any> {
    return new Promise((resolve, reject) => {
      if (operation === 'encrypt') {
        const encryptStream = new CryptoTransformStream('encrypt', {
          algorithm: 'aes-256-gcm',
          chunkSize: 64 * 1024,
          useWorkerPool: true,
          workerPoolSize: 4
        });

        const chunks: Buffer[] = [];

        encryptStream.on('data', (chunk) => {
          chunks.push(chunk);
        });

        encryptStream.on('end', () => {
          resolve(Buffer.concat(chunks));
        });

        encryptStream.on('error', reject);

        encryptStream.write(data);
        encryptStream.end();
      } else {
        reject(new Error(`Streaming operation ${operation} not implemented`));
      }
    });
  }

  /**
   * Run comprehensive benchmark suite
   */
  async runBenchmarkSuite(): Promise<BenchmarkSuite> {
    console.log('🚀 Starting Euxis Crypto Performance Benchmark Suite...\n');

    const results: BenchmarkResult[] = [];
    const operations = ['encrypt', 'generateKeyPair', 'hash'];

    // Test CPU-intensive operations with both main thread and worker pool
    for (const operation of operations) {
      console.log(`⚡ Benchmarking ${operation}...`);

      // Main thread benchmark
      console.log(`  📊 Main thread (100 iterations)...`);
      const mainThreadResult = await this.benchmarkMainThread(operation, 100);
      results.push(mainThreadResult);

      // Worker pool benchmark (only for CPU-intensive operations)
      if (CPU_INTENSIVE_OPS.includes(operation)) {
        console.log(`  🏭 Worker pool (100 iterations)...`);
        const workerPoolResult = await this.benchmarkWorkerPool(operation, 100);
        results.push(workerPoolResult);
      }

      console.log(`  ✅ ${operation} benchmark complete\n`);
    }

    // Streaming benchmarks for large data
    console.log(`🌊 Streaming benchmark (1MB data)...`);
    const streamingResult = await this.benchmarkStreaming('encrypt', 1024 * 1024);
    results.push(streamingResult);

    // Calculate speedup ratios
    const workerPoolSpeedup: Record<string, number> = {};
    operations.forEach(op => {
      const mainThreadResult = results.find(r => r.operation === op && r.mode === 'main-thread');
      const workerPoolResult = results.find(r => r.operation === op && r.mode === 'worker-pool');

      if (mainThreadResult && workerPoolResult) {
        workerPoolSpeedup[op] = mainThreadResult.averageTime / workerPoolResult.averageTime;
      }
    });

    // Generate recommendations
    const recommendations = this.generateRecommendations(results, workerPoolSpeedup);

    const suite: BenchmarkSuite = {
      name: 'Euxis Crypto Performance Suite',
      results,
      summary: {
        totalOperations: results.reduce((sum, r) => sum + r.iterations, 0),
        totalTime: results.reduce((sum, r) => sum + r.totalTime, 0),
        workerPoolSpeedup,
        recommendations
      }
    };

    console.log('🎯 Benchmark Suite Complete!\n');
    this.printSummary(suite);

    return suite;
  }

  private generateRecommendations(results: BenchmarkResult[], speedups: Record<string, number>): string[] {
    const recommendations: string[] = [];

    // Analyze speedups
    Object.entries(speedups).forEach(([operation, speedup]) => {
      if (speedup > 1.2) {
        recommendations.push(`✅ Use worker pool for ${operation} operations (${speedup.toFixed(2)}x speedup)`);
      } else if (speedup < 0.8) {
        recommendations.push(`⚠️  Use main thread for ${operation} operations (worker pool overhead detected)`);
      }
    });

    // Memory recommendations
    const highMemoryOps = results.filter(r => r.memoryUsage.peak > 10 * 1024 * 1024); // >10MB
    if (highMemoryOps.length > 0) {
      recommendations.push(`🧠 High memory operations detected: consider streaming for large data`);
    }

    // Streaming recommendations
    const streamingResult = results.find(r => r.mode === 'streaming');
    if (streamingResult && streamingResult.throughput > 50) { // >50 MB/s
      recommendations.push(`🌊 Streaming provides good throughput (${streamingResult.throughput.toFixed(2)} MB/s)`);
    }

    return recommendations;
  }

  private printSummary(suite: BenchmarkSuite): void {
    console.log('📈 PERFORMANCE SUMMARY');
    console.log('='.repeat(50));

    suite.results.forEach(result => {
      console.log(`${result.operation} (${result.mode}):`);
      console.log(`  ⏱️  Average: ${result.averageTime.toFixed(2)}ms`);
      console.log(`  🚀 Throughput: ${result.throughput.toFixed(2)} ${result.mode === 'streaming' ? 'MB/s' : 'ops/s'}`);
      console.log(`  💾 Memory: ${(result.memoryUsage.peak / 1024 / 1024).toFixed(2)} MB peak`);
      console.log(`  ❌ Errors: ${result.errors}/${result.iterations}`);
      console.log();
    });

    console.log('🎯 RECOMMENDATIONS:');
    suite.summary.recommendations.forEach(rec => console.log(`  ${rec}`));
  }

  async cleanup(): Promise<void> {
    await this.workerPool.terminate();
  }
}

// CLI runner for benchmarks
if (import.meta.url === `file://${process.argv[1]}`) {
  async function main() {
    const benchmark = new CryptoBenchmark();

    try {
      await benchmark.runBenchmarkSuite();
    } catch (error) {
      console.error('❌ Benchmark failed:', error);
      process.exit(1);
    } finally {
      await benchmark.cleanup();
      process.exit(0);
    }
  }

  main();
}