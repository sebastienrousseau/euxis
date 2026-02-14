/**
 * Crypto Plugin Architecture - Main Export Module
 * Extensible cryptographic algorithm system with runtime loading
 */

// Core interfaces and classes
export {
  CryptoPlugin,
  PluginMetadata,
  PluginCapabilities,
  PluginRegistration,
  AlgorithmRegistry,
  globalRegistry
} from './interface';

// Plugin loader system
export {
  PluginLoader,
  PluginLoaderConfig,
  defaultPluginConfig,
  globalLoader
} from './loader';

// Built-in plugins are not exported directly to encourage dynamic loading
// Use the plugin loader to discover and load built-in plugins

/**
 * Quick-start helper function to initialize the plugin system
 */
export async function initializePluginSystem(config?: Partial<PluginLoaderConfig>): Promise<AlgorithmRegistry> {
  const loader = config ? new PluginLoader({ ...defaultPluginConfig, ...config }) : globalLoader;
  await loader.loadPlugins();
  return loader.getRegistry();
}

/**
 * Utility function to check if an algorithm is available
 */
export function isAlgorithmAvailable(algorithm: string): boolean {
  return globalRegistry.getPluginForAlgorithm(algorithm) !== undefined;
}

/**
 * Utility function to list all available algorithms
 */
export function getAvailableAlgorithms(): string[] {
  return globalRegistry.listAlgorithms();
}

/**
 * Utility function to get plugin information for an algorithm
 */
export function getAlgorithmProvider(algorithm: string): PluginMetadata | undefined {
  const plugin = globalRegistry.getPluginForAlgorithm(algorithm);
  return plugin?.metadata;
}