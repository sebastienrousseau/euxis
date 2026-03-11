/**
 * Security Configuration for Crypto Service
 * Emergency Response: Secure Key Management Architecture
 */

export interface SecurityConfig {
  keyManagement: {
    provider: 'HSM' | 'KMS' | 'LOCAL_SECURE';
    hsmConfig?: {
      endpoint: string;
      partition: string;
      credentials: {
        username: string;
        passwordSecretArn: string;
      };
    };
    kmsConfig?: {
      region: string;
      keyId: string;
      endpoint?: string;
    };
  };
  encryption: {
    algorithm: string;
    keyRotationIntervalHours: number;
  };
  audit: {
    enabled: boolean;
    logLevel: 'DEBUG' | 'INFO' | 'WARN' | 'ERROR';
  };
}

/**
 * Default security configuration using KMS for key management
 * No private keys stored in filesystem - all operations via secure key store
 */
export const defaultSecurityConfig: SecurityConfig = {
  keyManagement: {
    provider: 'KMS',
    kmsConfig: {
      region: process.env.AWS_REGION || 'us-east-1',
      keyId: process.env.KMS_KEY_ID || '',
    },
  },
  encryption: {
    algorithm: 'AES-256-GCM',
    keyRotationIntervalHours: 24,
  },
  audit: {
    enabled: true,
    logLevel: 'INFO',
  },
};

/**
 * HSM configuration for high-security environments
 */
export const hsmSecurityConfig: SecurityConfig = {
  keyManagement: {
    provider: 'HSM',
    hsmConfig: {
      endpoint: process.env.HSM_ENDPOINT || '',
      partition: process.env.HSM_PARTITION || 'euxis-crypto',
      credentials: {
        username: process.env.HSM_USERNAME || '',
        passwordSecretArn: process.env.HSM_PASSWORD_SECRET_ARN || '',
      },
    },
  },
  encryption: {
    algorithm: 'AES-256-GCM',
    keyRotationIntervalHours: 12,
  },
  audit: {
    enabled: true,
    logLevel: 'DEBUG',
  },
};

/**
 * Emergency key rotation configuration
 * Triggered during security incidents
 */
export const emergencyRotationConfig = {
  immediateRotation: true,
  rotationTimeoutMinutes: 5,
  backupKeyGeneration: true,
  auditTrail: {
    required: true,
    retentionDays: 2555, // 7 years
  },
};