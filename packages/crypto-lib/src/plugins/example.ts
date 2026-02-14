/**
 * Plugin Architecture Integration Example
 * Demonstrates how to use the extensible crypto plugin system
 */

import {
  initializePluginSystem,
  isAlgorithmAvailable,
  getAvailableAlgorithms,
  getAlgorithmProvider,
  globalRegistry
} from './index';
import { createPlainText, createSymmetricKey } from '../types';

/**
 * Example: Initialize and use the plugin system
 */
export async function pluginArchitectureExample(): Promise<void> {
  console.log('=== Crypto Plugin Architecture Example ===\n');

  // 1. Initialize the plugin system (loads built-in plugins)
  console.log('1. Initializing plugin system...');
  const registry = await initializePluginSystem({
    autoLoadBuiltins: true,
    allowExternalPlugins: false // For security in this example
  });
  console.log('Plugin system initialized successfully\n');

  // 2. List all available algorithms
  console.log('2. Available algorithms:');
  const algorithms = getAvailableAlgorithms();
  algorithms.forEach(alg => {
    const provider = getAlgorithmProvider(alg);
    console.log(`   - ${alg} (provided by ${provider?.name})`);
  });
  console.log();

  // 3. Check if specific algorithms are available
  console.log('3. Algorithm availability checks:');
  const testAlgorithms = ['AES-256-GCM', 'SHA-256', 'RSA-PSS'];
  testAlgorithms.forEach(alg => {
    const available = isAlgorithmAvailable(alg);
    console.log(`   - ${alg}: ${available ? 'Available' : 'Not available'}`);
  });
  console.log();

  // 4. Use a plugin for encryption
  console.log('4. Using AES plugin for encryption:');
  const aesPlugin = registry.getPluginForAlgorithm('AES-256-GCM');
  if (aesPlugin && aesPlugin.encrypt && aesPlugin.generateSymmetricKey) {
    try {
      // Generate a key
      console.log('   Generating symmetric key...');
      const key = await aesPlugin.generateSymmetricKey('AES-256-GCM');
      console.log(`   Key generated: ${key.substring(0, 20)}...`);

      // Encrypt some data
      console.log('   Encrypting data...');
      const plainText = createPlainText('Hello, Plugin Architecture!');
      const result = await aesPlugin.encrypt(plainText, key, 'AES-256-GCM');

      console.log(`   Original: ${plainText}`);
      console.log(`   Encrypted: ${result.cipherText.substring(0, 40)}...`);
      console.log(`   Algorithm: ${result.algorithm}`);
      console.log(`   IV: ${result.iv}`);

      // Decrypt the data
      if (aesPlugin.decrypt) {
        console.log('   Decrypting data...');
        const decrypted = await aesPlugin.decrypt(result.cipherText, key, 'AES-256-GCM');
        console.log(`   Decrypted: ${decrypted.plainText}`);
        console.log(`   Match: ${decrypted.plainText === plainText}`);
      }
    } catch (error) {
      console.error('   Encryption example failed:', error);
    }
  } else {
    console.log('   AES plugin not available or doesn\'t support encryption');
  }
  console.log();

  // 5. List all loaded plugins
  console.log('5. Loaded plugins:');
  const plugins = registry.listPlugins();
  plugins.forEach(registration => {
    const { plugin, loadedAt, source } = registration;
    const { name, version, description, capabilities } = plugin.metadata;
    console.log(`   - ${name} v${version} (${source})`);
    console.log(`     Description: ${description}`);
    console.log(`     Loaded: ${loadedAt.toISOString()}`);
    console.log(`     Capabilities: ${Object.entries(capabilities)
      .filter(([, enabled]) => enabled)
      .map(([cap]) => cap)
      .join(', ')}`);
    console.log();
  });

  // 6. Cleanup
  console.log('6. Cleaning up...');
  await registry.cleanup();
  console.log('Cleanup completed\n');

  console.log('=== Example completed successfully ===');
}

/**
 * Example: Dynamic plugin loading (for external plugins)
 */
export async function dynamicPluginExample(): Promise<void> {
  console.log('=== Dynamic Plugin Loading Example ===\n');

  // This example shows how to load external plugins at runtime
  // In a real scenario, you would have actual plugin files to load

  console.log('Note: This example demonstrates the API for dynamic loading.');
  console.log('In production, you would load actual external plugin files.\n');

  const registry = globalRegistry;

  try {
    // Example of how you would load an external plugin
    // await pluginLoader.loadPlugin('/path/to/external/plugin.js', 'external');

    console.log('Dynamic loading API available via PluginLoader class');
    console.log('Methods:');
    console.log('  - loadPlugin(path, source)');
    console.log('  - unloadPlugin(name)');
    console.log('  - loadPlugins() // Load from configured directories');
    console.log();

    console.log('Security considerations for external plugins:');
    console.log('  - Enable signature validation in production');
    console.log('  - Limit plugin directories to trusted locations');
    console.log('  - Set reasonable plugin count limits');
    console.log('  - Validate plugin interfaces before loading');

  } catch (error) {
    console.error('Dynamic plugin example error:', error);
  }

  console.log('\n=== Dynamic example completed ===');
}

/**
 * Run all examples
 */
export async function runAllExamples(): Promise<void> {
  try {
    await pluginArchitectureExample();
    console.log('\n' + '='.repeat(60) + '\n');
    await dynamicPluginExample();
  } catch (error) {
    console.error('Example execution failed:', error);
  }
}