/**
 * Plugin Architecture for Extensible Cryptographic Algorithms
 * Core interfaces for runtime plugin loading and algorithm registration
 */

import {
  EncryptionAlgorithm,
  HashAlgorithm,
  SignatureAlgorithm,
  PlainText,
  CipherText,
  HashValue,
  Signature,
  PublicKey,
  PrivateKey,
  SymmetricKey,
  EncryptionResult,
  DecryptionResult,
  HashResult,
  SignatureResult,
  VerificationResult
} from '../types';

/**
 * Plugin capability flags
 */
export interface PluginCapabilities {
  readonly encryption: boolean;
  readonly hashing: boolean;
  readonly signing: boolean;
  readonly keyGeneration: boolean;
}

/**
 * Plugin metadata for registration and discovery
 */
export interface PluginMetadata {
  readonly name: string;
  readonly version: string;
  readonly description: string;
  readonly author: string;
  readonly capabilities: PluginCapabilities;
  readonly algorithms: {
    readonly encryption?: EncryptionAlgorithm[];
    readonly hashing?: HashAlgorithm[];
    readonly signing?: SignatureAlgorithm[];
  };
}

/**
 * Core plugin interface for cryptographic algorithm providers
 * All plugins must implement this interface for runtime loading
 */
export interface CryptoPlugin {
  readonly metadata: PluginMetadata;

  // Lifecycle methods
  initialize(): Promise<void>;
  cleanup(): Promise<void>;

  // Encryption operations (if supported)
  encrypt?(data: PlainText, key: SymmetricKey, algorithm: EncryptionAlgorithm): Promise<EncryptionResult>;
  decrypt?(data: CipherText, key: SymmetricKey, algorithm: EncryptionAlgorithm): Promise<DecryptionResult>;

  // Hashing operations (if supported)
  hash?(input: PlainText, algorithm: HashAlgorithm): Promise<HashResult>;

  // Signature operations (if supported)
  sign?(data: PlainText, privateKey: PrivateKey, algorithm: SignatureAlgorithm): Promise<SignatureResult>;
  verify?(signature: Signature, data: PlainText, publicKey: PublicKey, algorithm: SignatureAlgorithm): Promise<VerificationResult>;

  // Key generation (if supported)
  generateSymmetricKey?(algorithm: EncryptionAlgorithm): Promise<SymmetricKey>;
  generateKeyPair?(algorithm: SignatureAlgorithm): Promise<{ publicKey: PublicKey; privateKey: PrivateKey }>;
}

/**
 * Plugin registration entry
 */
export interface PluginRegistration {
  readonly plugin: CryptoPlugin;
  readonly loadedAt: Date;
  readonly source: 'builtin' | 'external' | 'dynamic';
}

/**
 * Algorithm registry for runtime algorithm resolution
 * Maps algorithm names to their plugin providers
 */
export class AlgorithmRegistry {
  private readonly plugins = new Map<string, PluginRegistration>();
  private readonly algorithmMap = new Map<string, string>(); // algorithm -> plugin name

  /**
   * Register a crypto plugin with the registry
   */
  async registerPlugin(plugin: CryptoPlugin, source: 'builtin' | 'external' | 'dynamic' = 'external'): Promise<void> {
    const { name } = plugin.metadata;

    if (this.plugins.has(name)) {
      throw new Error(`Plugin '${name}' is already registered`);
    }

    // Initialize the plugin
    await plugin.initialize();

    // Register the plugin
    this.plugins.set(name, {
      plugin,
      loadedAt: new Date(),
      source
    });

    // Map algorithms to this plugin
    const { algorithms } = plugin.metadata;
    if (algorithms.encryption) {
      algorithms.encryption.forEach(alg => this.algorithmMap.set(alg, name));
    }
    if (algorithms.hashing) {
      algorithms.hashing.forEach(alg => this.algorithmMap.set(alg, name));
    }
    if (algorithms.signing) {
      algorithms.signing.forEach(alg => this.algorithmMap.set(alg, name));
    }
  }

  /**
   * Get plugin by name
   */
  getPlugin(name: string): CryptoPlugin | undefined {
    return this.plugins.get(name)?.plugin;
  }

  /**
   * Get plugin that supports a specific algorithm
   */
  getPluginForAlgorithm(algorithm: string): CryptoPlugin | undefined {
    const pluginName = this.algorithmMap.get(algorithm);
    return pluginName ? this.getPlugin(pluginName) : undefined;
  }

  /**
   * List all registered plugins
   */
  listPlugins(): PluginRegistration[] {
    return Array.from(this.plugins.values());
  }

  /**
   * List all available algorithms
   */
  listAlgorithms(): string[] {
    return Array.from(this.algorithmMap.keys());
  }

  /**
   * Unregister a plugin and clean up
   */
  async unregisterPlugin(name: string): Promise<void> {
    const registration = this.plugins.get(name);
    if (!registration) {
      throw new Error(`Plugin '${name}' is not registered`);
    }

    // Remove algorithm mappings
    const { algorithms } = registration.plugin.metadata;
    if (algorithms.encryption) {
      algorithms.encryption.forEach(alg => this.algorithmMap.delete(alg));
    }
    if (algorithms.hashing) {
      algorithms.hashing.forEach(alg => this.algorithmMap.delete(alg));
    }
    if (algorithms.signing) {
      algorithms.signing.forEach(alg => this.algorithmMap.delete(alg));
    }

    // Cleanup plugin
    await registration.plugin.cleanup();

    // Remove from registry
    this.plugins.delete(name);
  }

  /**
   * Cleanup all plugins
   */
  async cleanup(): Promise<void> {
    const cleanupPromises = Array.from(this.plugins.values()).map(reg => reg.plugin.cleanup());
    await Promise.all(cleanupPromises);
    this.plugins.clear();
    this.algorithmMap.clear();
  }
}

/**
 * Global algorithm registry instance
 */
export const globalRegistry = new AlgorithmRegistry();