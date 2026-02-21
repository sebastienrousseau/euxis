/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2024-2026 Euxis Contributors
 */

/**
 * Runtime Plugin Loader for Cryptographic Algorithms
 * Handles dynamic loading and registration of crypto plugins
 */

import { CryptoPlugin, AlgorithmRegistry, globalRegistry } from './interface.js';
import path from 'path';
import { promises as fs } from 'fs';
import { fileURLToPath } from 'url';

// ESM-compatible directory resolution
const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

/**
 * Plugin discovery configuration
 */
export interface PluginLoaderConfig {
  readonly pluginDirs: string[];
  readonly autoLoadBuiltins: boolean;
  readonly allowExternalPlugins: boolean;
  readonly maxPlugins: number;
  readonly validation: {
    readonly requireSignature: boolean;
    readonly trustedSources: string[];
  };
}

/**
 * Plugin loader for runtime discovery and loading
 */
export class PluginLoader {
  private readonly config: PluginLoaderConfig;
  private readonly registry: AlgorithmRegistry;

  constructor(config: PluginLoaderConfig, registry: AlgorithmRegistry = globalRegistry) {
    this.config = config;
    this.registry = registry;
  }

  /**
   * Load all plugins from configured directories
   */
  async loadPlugins(): Promise<void> {
    // Load builtin plugins first
    if (this.config.autoLoadBuiltins) {
      await this.loadBuiltinPlugins();
    }

    // Load external plugins if enabled
    if (this.config.allowExternalPlugins) {
      await this.loadExternalPlugins();
    }
  }

  /**
   * Load builtin plugins from the builtin directory
   */
  private async loadBuiltinPlugins(): Promise<void> {
    const builtinDir = path.join(__dirname, 'builtin');

    try {
      const files = await fs.readdir(builtinDir);
      const pluginFiles = files.filter(file => file.endsWith('.plugin.js') || file.endsWith('.plugin.ts'));

      for (const file of pluginFiles) {
        try {
          await this.loadPluginFile(path.join(builtinDir, file), 'builtin');
        } catch (error) {
          console.warn(`Failed to load builtin plugin ${file}:`, error);
        }
      }
    } catch (error) {
      console.warn('Failed to load builtin plugins directory:', error);
    }
  }

  /**
   * Load external plugins from configured directories
   */
  private async loadExternalPlugins(): Promise<void> {
    for (const pluginDir of this.config.pluginDirs) {
      try {
        await this.loadPluginsFromDirectory(pluginDir, 'external');
      } catch (error) {
        console.warn(`Failed to load plugins from ${pluginDir}:`, error);
      }
    }
  }

  /**
   * Load plugins from a specific directory
   */
  private async loadPluginsFromDirectory(directory: string, source: 'builtin' | 'external'): Promise<void> {
    try {
      const files = await fs.readdir(directory);
      const pluginFiles = files.filter(file =>
        file.endsWith('.plugin.js') || file.endsWith('.plugin.ts')
      );

      for (const file of pluginFiles) {
        try {
          await this.loadPluginFile(path.join(directory, file), source);
        } catch (error) {
          console.warn(`Failed to load plugin ${file}:`, error);
        }
      }
    } catch (error) {
      console.warn(`Failed to scan directory ${directory}:`, error);
    }
  }

  /**
   * Load a specific plugin file
   */
  private async loadPluginFile(filePath: string, source: 'builtin' | 'external'): Promise<void> {
    // Validate plugin before loading
    if (!await this.validatePluginFile(filePath)) {
      throw new Error(`Plugin validation failed for ${filePath}`);
    }

    // Check plugin count limit
    if (this.registry.listPlugins().length >= this.config.maxPlugins) {
      throw new Error(`Maximum plugin count (${this.config.maxPlugins}) reached`);
    }

    // Dynamic import of the plugin
    const pluginModule = await import(filePath);

    // Plugin modules should export a default plugin instance or factory
    let plugin: CryptoPlugin;
    if (typeof pluginModule.default === 'function') {
      plugin = pluginModule.default();
    } else if (typeof pluginModule.default === 'object') {
      plugin = pluginModule.default;
    } else if (typeof pluginModule.createPlugin === 'function') {
      plugin = pluginModule.createPlugin();
    } else {
      throw new Error(`Invalid plugin export in ${filePath}`);
    }

    // Validate plugin interface
    this.validatePluginInterface(plugin);

    // Register the plugin
    await this.registry.registerPlugin(plugin, source);
  }

  /**
   * Validate plugin file before loading
   */
  private async validatePluginFile(filePath: string): Promise<boolean> {
    try {
      // Check file exists and is readable
      await fs.access(filePath, fs.constants.R_OK);

      // Additional validation based on configuration
      if (this.config.validation.requireSignature) {
        // In production, implement digital signature verification
        console.warn('Plugin signature validation not implemented');
      }

      return true;
    } catch (error) {
      return false;
    }
  }

  /**
   * Validate plugin implements required interface
   */
  private validatePluginInterface(plugin: unknown): void {
    if (!plugin || typeof plugin !== 'object') {
      throw new Error('Plugin must be an object');
    }

    if (!plugin.metadata || typeof plugin.metadata !== 'object') {
      throw new Error('Plugin must have metadata property');
    }

    const required = ['name', 'version', 'description', 'author', 'capabilities', 'algorithms'];
    for (const prop of required) {
      if (!plugin.metadata[prop]) {
        throw new Error(`Plugin metadata missing required property: ${prop}`);
      }
    }

    if (typeof plugin.initialize !== 'function') {
      throw new Error('Plugin must implement initialize() method');
    }

    if (typeof plugin.cleanup !== 'function') {
      throw new Error('Plugin must implement cleanup() method');
    }
  }

  /**
   * Load a single plugin by name
   */
  async loadPlugin(pluginPath: string, source: 'builtin' | 'external' | 'dynamic' = 'dynamic'): Promise<void> {
    await this.loadPluginFile(pluginPath, source);
  }

  /**
   * Unload a plugin by name
   */
  async unloadPlugin(name: string): Promise<void> {
    await this.registry.unregisterPlugin(name);
  }

  /**
   * Get registry instance
   */
  getRegistry(): AlgorithmRegistry {
    return this.registry;
  }
}

/**
 * Default plugin loader configuration
 */
export const defaultPluginConfig: PluginLoaderConfig = {
  pluginDirs: [
    path.join(process.cwd(), 'plugins'),
    path.join(process.cwd(), 'crypto-plugins'),
  ],
  autoLoadBuiltins: true,
  allowExternalPlugins: true,
  maxPlugins: 50,
  validation: {
    requireSignature: false, // Set to true in production
    trustedSources: []
  }
};

/**
 * Global plugin loader instance
 */
export const globalLoader = new PluginLoader(defaultPluginConfig, globalRegistry);