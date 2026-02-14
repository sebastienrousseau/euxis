// ESM imports from published crypto-lib package
import type { CryptoMetrics } from '@euxis/crypto-lib';

// Mock metrics collector and logger for now (would be implemented when observability modules are available)
const metricsCollector = {
  getMetrics: (): CryptoMetrics => ({
    operationCount: 0,
    successRate: 100,
    errorCount: 0,
    averageDuration: 0
  }),
  getOperationStats: (operation: string) => ({
    count: 0,
    successRate: 100,
    p95Duration: 0
  })
};

const cryptoLogger = {
  logCryptoOperation: (params: {
    correlationId: string;
    operation: string;
    timestamp: string;
    service: string;
    success: boolean;
  }) => {
    console.log(`[${params.timestamp}] ${params.service}: ${params.operation} ${params.success ? 'SUCCESS' : 'FAILURE'} (${params.correlationId})`);
  }
};

export interface HealthStatus {
  status: 'healthy' | 'degraded' | 'unhealthy';
  timestamp: string;
  version: string;
  correlationId: string;
}

export interface CryptoHealthCheck extends HealthStatus {
  crypto: {
    metrics: CryptoMetrics;
    lastOperationStatus: 'success' | 'error' | 'none';
    errorRate: number;
    averageResponseTime: number;
  };
  dependencies: {
    [key: string]: {
      status: 'up' | 'down' | 'unknown';
      responseTime?: number;
      error?: string;
    };
  };
  warnings: string[];
}

class CryptoHealthMonitor {
  private readonly maxErrorRate = 5; // 5% error rate threshold
  private readonly maxResponseTime = 1000; // 1 second threshold

  async checkHealth(correlationId: string): Promise<CryptoHealthCheck> {
    const timestamp = new Date().toISOString();
    const metrics = metricsCollector.getMetrics();

    const healthCheck: CryptoHealthCheck = {
      status: 'healthy',
      timestamp,
      version: process.env.CRYPTO_SERVICE_VERSION || '1.0.0',
      correlationId,
      crypto: {
        metrics,
        lastOperationStatus: this.getLastOperationStatus(metrics),
        errorRate: 100 - metrics.successRate,
        averageResponseTime: metrics.averageDuration
      },
      dependencies: await this.checkDependencies(),
      warnings: []
    };

    // Evaluate overall health
    healthCheck.status = this.evaluateOverallHealth(healthCheck);

    // Log health check
    cryptoLogger.logCryptoOperation({
      correlationId,
      operation: 'HealthCheck',
      timestamp,
      service: 'crypto-server',
      success: healthCheck.status === 'healthy'
    });

    return healthCheck;
  }

  private getLastOperationStatus(metrics: CryptoMetrics): 'success' | 'error' | 'none' {
    if (metrics.operationCount === 0) return 'none';
    return metrics.errorCount === 0 ? 'success' : 'error';
  }

  private async checkDependencies(): Promise<CryptoHealthCheck['dependencies']> {
    const dependencies: CryptoHealthCheck['dependencies'] = {};

    // Check HSM/KMS availability (if configured)
    if (process.env.HSM_ENDPOINT) {
      try {
        const start = Date.now();
        // This would be actual HSM health check in real implementation
        await new Promise(resolve => setTimeout(resolve, 10));
        dependencies.hsm = {
          status: 'up',
          responseTime: Date.now() - start
        };
      } catch (error) {
        dependencies.hsm = {
          status: 'down',
          error: error instanceof Error ? error.message : 'Unknown error'
        };
      }
    }

    // Check key store availability
    try {
      const start = Date.now();
      // This would check actual key store in real implementation
      await new Promise(resolve => setTimeout(resolve, 5));
      dependencies.keystore = {
        status: 'up',
        responseTime: Date.now() - start
      };
    } catch (error) {
      dependencies.keystore = {
        status: 'down',
        error: error instanceof Error ? error.message : 'Unknown error'
      };
    }

    return dependencies;
  }

  private evaluateOverallHealth(healthCheck: CryptoHealthCheck): 'healthy' | 'degraded' | 'unhealthy' {
    const { crypto, dependencies, warnings } = healthCheck;

    // Check for critical failures
    const hasCriticalDependencyFailure = Object.values(dependencies)
      .some(dep => dep.status === 'down');

    if (hasCriticalDependencyFailure) {
      warnings.push('Critical dependency failure detected');
      return 'unhealthy';
    }

    // Check error rate
    if (crypto.errorRate > this.maxErrorRate) {
      warnings.push(`Error rate ${crypto.errorRate}% exceeds threshold ${this.maxErrorRate}%`);
      return crypto.errorRate > this.maxErrorRate * 2 ? 'unhealthy' : 'degraded';
    }

    // Check response time
    if (crypto.averageResponseTime > this.maxResponseTime) {
      warnings.push(`Average response time ${crypto.averageResponseTime}ms exceeds threshold ${this.maxResponseTime}ms`);
      return crypto.averageResponseTime > this.maxResponseTime * 2 ? 'unhealthy' : 'degraded';
    }

    // Check for degraded dependencies
    const hasDegradedDependencies = Object.values(dependencies)
      .some(dep => dep.responseTime && dep.responseTime > 500);

    if (hasDegradedDependencies) {
      warnings.push('Some dependencies showing high latency');
      return 'degraded';
    }

    return 'healthy';
  }

  async getDetailedMetrics(correlationId: string): Promise<{
    overall: CryptoMetrics;
    byOperation: Record<string, any>;
    alerts: string[];
  }> {
    const overall = metricsCollector.getMetrics();
    const operations = ['encrypt', 'decrypt', 'keygen', 'sign', 'verify'];

    const byOperation: Record<string, any> = {};
    const alerts: string[] = [];

    for (const operation of operations) {
      const stats = metricsCollector.getOperationStats(operation);
      byOperation[operation] = stats;

      // Generate alerts for problematic operations
      if (stats.successRate < 95 && stats.count > 0) {
        alerts.push(`${operation} success rate below 95%: ${stats.successRate.toFixed(1)}%`);
      }
      if (stats.p95Duration > this.maxResponseTime && stats.count > 0) {
        alerts.push(`${operation} P95 latency high: ${stats.p95Duration.toFixed(1)}ms`);
      }
    }

    return { overall, byOperation, alerts };
  }
}

export const healthMonitor = new CryptoHealthMonitor();
export default healthMonitor;