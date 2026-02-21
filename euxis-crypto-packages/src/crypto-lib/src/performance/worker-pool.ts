/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2024-2026 Euxis Contributors
 */

import { Worker, isMainThread, parentPort, workerData } from 'worker_threads';
import { cpus } from 'os';
import { CryptoOperationOptions } from '../types.js';

export interface WorkerTask {
  id: string;
  operation: 'encrypt' | 'decrypt' | 'hash' | 'sign' | 'verify';
  data: Buffer;
  options?: CryptoOperationOptions;
}

export interface WorkerResult {
  id: string;
  success: boolean;
  result?: Buffer;
  error?: string;
  duration: number;
}

export class CryptoWorkerPool {
  private workers: Worker[] = [];
  private taskQueue: Array<{
    task: WorkerTask;
    resolve: (result: WorkerResult) => void;
    reject: (error: Error) => void;
  }> = [];
  private activeTasks = new Map<string, {
    resolve: (result: WorkerResult) => void;
    reject: (error: Error) => void;
  }>();
  private poolSize: number;

  constructor(poolSize: number = cpus().length) {
    this.poolSize = Math.max(1, Math.min(poolSize, cpus().length));
    this.initializeWorkers();
  }

  private initializeWorkers(): void {
    for (let i = 0; i < this.poolSize; i++) {
      const worker = new Worker(__filename);

      worker.on('message', (result: WorkerResult) => {
        const pending = this.activeTasks.get(result.id);
        if (pending) {
          this.activeTasks.delete(result.id);
          if (result.success) {
            pending.resolve(result);
          } else {
            pending.reject(new Error(result.error || 'Worker task failed'));
          }
        }
        this.processNextTask();
      });

      worker.on('error', (error) => {
        console.error('Worker error:', error);
        // Restart worker on error
        this.restartWorker(i);
      });

      worker.on('exit', (code) => {
        if (code !== 0) {
          console.error(`Worker stopped with exit code ${code}`);
          this.restartWorker(i);
        }
      });

      this.workers.push(worker);
    }
  }

  private restartWorker(index: number): void {
    this.workers[index]?.terminate();
    this.workers[index] = new Worker(__filename);
  }

  async execute(task: WorkerTask): Promise<WorkerResult> {
    return new Promise((resolve, reject) => {
      this.taskQueue.push({ task, resolve, reject });
      this.processNextTask();
    });
  }

  private processNextTask(): void {
    if (this.taskQueue.length === 0 || this.activeTasks.size >= this.poolSize) {
      return;
    }

    const { task, resolve, reject } = this.taskQueue.shift()!;
    this.activeTasks.set(task.id, { resolve, reject });

    // Find available worker
    const availableWorkerIndex = this.workers.findIndex((_, index) =>
      !Array.from(this.activeTasks.keys()).some(taskId =>
        this.activeTasks.get(taskId) === this.activeTasks.get(task.id)
      )
    );

    if (availableWorkerIndex !== -1) {
      this.workers[availableWorkerIndex].postMessage(task);
    }
  }

  async terminate(): Promise<void> {
    await Promise.all(this.workers.map(worker => worker.terminate()));
    this.workers = [];
    this.taskQueue = [];
    this.activeTasks.clear();
  }

  getPoolStats() {
    return {
      poolSize: this.poolSize,
      activeWorkers: this.activeTasks.size,
      queuedTasks: this.taskQueue.length,
      totalWorkers: this.workers.length
    };
  }
}

// Worker thread execution context
if (!isMainThread && parentPort) {
  parentPort.on('message', async (task: WorkerTask) => {
    const startTime = performance.now();

    try {
      // Import crypto operations dynamically to avoid circular dependencies
      const { performCryptoOperation } = await import('../crypto-operations.js');

      const result = await performCryptoOperation({
        operation: task.operation,
        data: task.data,
        options: task.options
      });

      const duration = performance.now() - startTime;

      parentPort!.postMessage({
        id: task.id,
        success: true,
        result: result.data,
        duration
      } as WorkerResult);

    } catch (error) {
      const duration = performance.now() - startTime;

      parentPort!.postMessage({
        id: task.id,
        success: false,
        error: error instanceof Error ? error.message : 'Unknown error',
        duration
      } as WorkerResult);
    }
  });
}