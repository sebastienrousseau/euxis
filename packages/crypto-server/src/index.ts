/**
 * @euxis/crypto-server - Main entry point
 * ESM module for cryptographic service server
 */

// Re-export health monitoring functionality
export * from './health/crypto-health.js';
export { healthMonitor as default } from './health/crypto-health.js';

// Version and server metadata
export const CRYPTO_SERVER_VERSION = '0.0.7';
export const SERVER_NAME = 'euxis-crypto-server';

// Dynamic import for health monitor (supports lazy loading)
export const loadHealthMonitor = async () => {
  const { healthMonitor } = await import('./health/crypto-health.js');
  return healthMonitor;
};

// Server initialization with dynamic imports for better modularity
export const initializeServer = async (config?: {
  port?: number;
  enableMetrics?: boolean;
  hsm?: {
    endpoint: string;
    keyId: string;
  };
}) => {
  // This would initialize the actual server when implemented
  const healthMonitor = await loadHealthMonitor();

  console.log(`${SERVER_NAME} v${CRYPTO_SERVER_VERSION} initializing...`);

  if (config?.enableMetrics) {
    console.log('Metrics collection enabled');
  }

  if (config?.hsm) {
    console.log(`HSM configured: ${config.hsm.endpoint}`);
    process.env.HSM_ENDPOINT = config.hsm.endpoint;
  }

  return {
    healthMonitor,
    version: CRYPTO_SERVER_VERSION,
    config: config || {}
  };
};

// Export supported server configurations
export const SUPPORTED_CONFIGURATIONS = {
  deployment: ['standalone', 'cluster', 'serverless'],
  security: ['standard', 'hsm', 'kms'],
  monitoring: ['basic', 'advanced', 'enterprise']
} as const;