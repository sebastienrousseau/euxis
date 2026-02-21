/**
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (c) 2024-2026 Euxis Contributors
 */

/**
 * Result-based Plugin Architecture for Extensible Cryptographic Algorithms
 * Core interfaces using Result<T,E> pattern for predictable error handling
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
  KeyPair
} from '../types';

import {
  CryptoResult,
  CryptoError,
  CryptoErrorType,
  Ok,
  CryptoErr
} from '../types/result';

/**
 * Result-based operation types for crypto operations
 */
export type EncryptionResult = CryptoResult<{
  readonly cipherText: CipherText;
  readonly algorithm: EncryptionAlgorithm;
  readonly iv?: string;
  readonly authTag?: string;
}>;

export type DecryptionResult = CryptoResult<{
  readonly plainText: PlainText;
  readonly algorithm: EncryptionAlgorithm;
}>;

export type HashResult = CryptoResult<{
  readonly hash: HashValue;
  readonly algorithm: HashAlgorithm;
}>;

export type SignatureResult = CryptoResult<{
  readonly signature: Signature;
  readonly algorithm: SignatureAlgorithm;
  readonly keyId?: string;
}>;

export type VerificationResult = CryptoResult<{
  readonly isValid: boolean;
  readonly algorithm: SignatureAlgorithm;
  readonly keyId?: string;
}>;

export type KeyGenerationResult = CryptoResult<SymmetricKey>;
export type KeyPairGenerationResult = CryptoResult<KeyPair>;

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
 * Result-based plugin interface for cryptographic algorithm providers
 * All operations return Result<T, CryptoError> instead of throwing exceptions
 */
export interface ResultCryptoPlugin {
  readonly metadata: PluginMetadata;

  // Lifecycle methods
  initialize(): Promise<CryptoResult<void>>;
  cleanup(): Promise<CryptoResult<void>>;

  // Encryption operations (if supported)
  encrypt?(
    data: PlainText,
    key: SymmetricKey,
    algorithm: EncryptionAlgorithm
  ): Promise<EncryptionResult>;

  decrypt?(
    data: CipherText,
    key: SymmetricKey,
    algorithm: EncryptionAlgorithm
  ): Promise<DecryptionResult>;

  // Hashing operations (if supported)
  hash?(input: PlainText, algorithm: HashAlgorithm): Promise<HashResult>;

  // Signature operations (if supported)
  sign?(
    data: PlainText,
    privateKey: PrivateKey,
    algorithm: SignatureAlgorithm
  ): Promise<SignatureResult>;

  verify?(
    signature: Signature,
    data: PlainText,
    publicKey: PublicKey,
    algorithm: SignatureAlgorithm
  ): Promise<VerificationResult>;

  // Key generation (if supported)
  generateSymmetricKey?(algorithm: EncryptionAlgorithm): Promise<KeyGenerationResult>;
  generateKeyPair?(algorithm: SignatureAlgorithm): Promise<KeyPairGenerationResult>;
}

/**
 * Result-based plugin registration entry
 */
export interface ResultPluginRegistration {
  readonly plugin: ResultCryptoPlugin;
  readonly loadedAt: Date;
  readonly source: 'builtin' | 'external' | 'dynamic';
}

/**
 * Result-based algorithm registry for runtime algorithm resolution
 * All operations return Result<T, CryptoError> instead of throwing exceptions
 */
export class ResultAlgorithmRegistry {
  private readonly plugins = new Map<string, ResultPluginRegistration>();
  private readonly algorithmMap = new Map<string, string>(); // algorithm -> plugin name

  /**
   * Register a crypto plugin with the registry
   */
  async registerPlugin(
    plugin: ResultCryptoPlugin,
    source: 'builtin' | 'external' | 'dynamic' = 'external'
  ): Promise<CryptoResult<void>> {
    const { name } = plugin.metadata;

    if (this.plugins.has(name)) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_LOAD_FAILED,
        `Plugin '${name}' is already registered`,
        'registerPlugin',
        { pluginName: name, source }
      );
    }

    // Initialize the plugin
    const initResult = await plugin.initialize();
    if (!initResult.success) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_NOT_INITIALIZED,
        `Failed to initialize plugin '${name}'`,
        'registerPlugin',
        { pluginName: name, source },
        new Error(initResult.error.message)
      );
    }

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

    return Ok(undefined);
  }

  /**
   * Get plugin by name
   */
  getPlugin(name: string): CryptoResult<ResultCryptoPlugin> {
    const registration = this.plugins.get(name);
    if (!registration) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_NOT_FOUND,
        `Plugin '${name}' is not registered`,
        'getPlugin',
        { pluginName: name }
      );
    }
    return Ok(registration.plugin);
  }

  /**
   * Get plugin that supports a specific algorithm
   */
  getPluginForAlgorithm(algorithm: string): CryptoResult<ResultCryptoPlugin> {
    const pluginName = this.algorithmMap.get(algorithm);
    if (!pluginName) {
      return CryptoErr(
        CryptoErrorType.UNSUPPORTED_ALGORITHM,
        `No plugin found for algorithm '${algorithm}'`,
        'getPluginForAlgorithm',
        { algorithm }
      );
    }
    return this.getPlugin(pluginName);
  }

  /**
   * List all registered plugins
   */
  listPlugins(): ResultPluginRegistration[] {
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
  async unregisterPlugin(name: string): Promise<CryptoResult<void>> {
    const registration = this.plugins.get(name);
    if (!registration) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_NOT_FOUND,
        `Plugin '${name}' is not registered`,
        'unregisterPlugin',
        { pluginName: name }
      );
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
    const cleanupResult = await registration.plugin.cleanup();
    if (!cleanupResult.success) {
      // Log the cleanup error but still remove the plugin
      console.warn(`Plugin cleanup failed for '${name}':`, cleanupResult.error);
    }

    // Remove from registry
    this.plugins.delete(name);
    return Ok(undefined);
  }

  /**
   * Cleanup all plugins
   */
  async cleanup(): Promise<CryptoResult<void>> {
    const errors: CryptoError[] = [];

    for (const registration of this.plugins.values()) {
      const cleanupResult = await registration.plugin.cleanup();
      if (!cleanupResult.success) {
        errors.push(cleanupResult.error);
      }
    }

    this.plugins.clear();
    this.algorithmMap.clear();

    if (errors.length > 0) {
      return CryptoErr(
        CryptoErrorType.PLUGIN_LOAD_FAILED,
        `Failed to cleanup ${errors.length} plugins`,
        'cleanup',
        { errorCount: errors.length, errors: errors.map(e => e.message) }
      );
    }

    return Ok(undefined);
  }
}

/**
 * Global result-based algorithm registry instance
 */
export const globalResultRegistry = new ResultAlgorithmRegistry();