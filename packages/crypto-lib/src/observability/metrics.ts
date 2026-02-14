export interface OperationMetrics {
  operation: string;
  duration: number;
  success: boolean;
  timestamp: Date;
  correlationId?: string;
  metadata?: any;
}

export class CryptoMetricsCollector {
  private operations: OperationMetrics[] = [];
  private counters: Map<string, number> = new Map();
  private durations: Map<string, number[]> = new Map();

  recordOperation(metrics: OperationMetrics): void {
    this.operations.push({
      ...metrics,
      timestamp: new Date()
    });

    // Update counters
    const key = `${metrics.operation}:${metrics.success ? 'success' : 'error'}`;
    this.counters.set(key, (this.counters.get(key) || 0) + 1);

    // Track duration
    if (!this.durations.has(metrics.operation)) {
      this.durations.set(metrics.operation, []);
    }
    this.durations.get(metrics.operation)!.push(metrics.duration);
  }

  getMetrics() {
    const summary = {
      totalOperations: this.operations.length,
      counters: Object.fromEntries(this.counters),
      averageDurations: {} as Record<string, number>,
      p95Durations: {} as Record<string, number>
    };

    // Calculate duration statistics
    for (const [operation, durations] of this.durations) {
      const sorted = [...durations].sort((a, b) => a - b);
      const avg = durations.reduce((sum, d) => sum + d, 0) / durations.length;
      const p95Index = Math.ceil(durations.length * 0.95) - 1;

      summary.averageDurations[operation] = Math.round(avg * 100) / 100;
      summary.p95Durations[operation] = sorted[p95Index] || 0;
    }

    return summary;
  }

  exportMetrics(): string {
    const metrics = this.getMetrics();
    return JSON.stringify(metrics, null, 2);
  }

  reset(): void {
    this.operations = [];
    this.counters.clear();
    this.durations.clear();
  }
}

export const metricsCollector = new CryptoMetricsCollector();

// Helper function for operation timing
export function withMetrics<T>(
  operation: string,
  correlationId: string,
  fn: () => Promise<T>
): Promise<T> {
  const startTime = Date.now();

  return fn()
    .then(result => {
      metricsCollector.recordOperation({
        operation,
        duration: Date.now() - startTime,
        success: true,
        timestamp: new Date(),
        correlationId
      });
      return result;
    })
    .catch(error => {
      metricsCollector.recordOperation({
        operation,
        duration: Date.now() - startTime,
        success: false,
        timestamp: new Date(),
        correlationId,
        metadata: { error: error.message }
      });
      throw error;
    });
}

export default metricsCollector;