import { describe, it, expect, vi, beforeEach } from 'vitest';
import { EmergencyKeyRotation, executeEmergencyResponse, RotationResult } from '../src/emergency-rotation.js';
import { SecurityConfig } from '../src/config/security.js';

describe('EmergencyKeyRotation', () => {
  let validHsmConfig: SecurityConfig;
  let validKmsConfig: SecurityConfig;
  let invalidConfig: SecurityConfig;

  beforeEach(() => {
    vi.clearAllMocks();

    validHsmConfig = {
      keyManagement: {
        provider: 'HSM',
        hsmConfig: {
          endpoint: 'hsm://test-endpoint',
          credentials: 'test-creds'
        }
      }
    } as SecurityConfig;

    validKmsConfig = {
      keyManagement: {
        provider: 'KMS',
        kmsConfig: {
          region: 'us-east-1',
          accessKey: 'test-key'
        }
      }
    } as SecurityConfig;

    invalidConfig = {
      keyManagement: {
        provider: 'LOCAL_SECURE'
      }
    } as SecurityConfig;
  });

  describe('constructor', () => {
    it('should create instance with valid HSM config', () => {
      const rotation = new EmergencyKeyRotation(validHsmConfig);
      expect(rotation).toBeInstanceOf(EmergencyKeyRotation);
    });

    it('should create instance with valid KMS config', () => {
      const rotation = new EmergencyKeyRotation(validKmsConfig);
      expect(rotation).toBeInstanceOf(EmergencyKeyRotation);
    });
  });

  describe('executeEmergencyRotation', () => {
    it('should complete emergency rotation successfully with HSM', async () => {
      const rotation = new EmergencyKeyRotation(validHsmConfig);

      const result: RotationResult = await rotation.executeEmergencyRotation();

      expect(result.success).toBe(true);
      expect(result.newKeyId).toBeDefined();
      expect(result.oldKeyId).toBeDefined();
      expect(result.timestamp).toBeDefined();
      expect(result.auditTrail).toBeInstanceOf(Array);
      expect(result.auditTrail.length).toBeGreaterThan(0);

      // Verify audit trail contains expected entries
      expect(result.auditTrail.some(entry => entry.includes('Emergency rotation initiated'))).toBe(true);
      expect(result.auditTrail.some(entry => entry.includes('Configuration validated: HSM'))).toBe(true);
      expect(result.auditTrail.some(entry => entry.includes('New key generated'))).toBe(true);
      expect(result.auditTrail.some(entry => entry.includes('Key rotation:'))).toBe(true);
      expect(result.auditTrail.some(entry => entry.includes('Key rotation verification successful'))).toBe(true);
      expect(result.auditTrail.some(entry => entry.includes('Emergency rotation completed successfully'))).toBe(true);
    });

    it('should complete emergency rotation successfully with KMS', async () => {
      const rotation = new EmergencyKeyRotation(validKmsConfig);

      const result: RotationResult = await rotation.executeEmergencyRotation();

      expect(result.success).toBe(true);
      expect(result.newKeyId).toBeDefined();
      expect(result.oldKeyId).toBeDefined();
      expect(result.auditTrail.some(entry => entry.includes('Configuration validated: KMS'))).toBe(true);
    });

    it('should fail with LOCAL_SECURE provider', async () => {
      const rotation = new EmergencyKeyRotation(invalidConfig);

      await expect(rotation.executeEmergencyRotation()).rejects.toThrow(
        'Emergency key rotation failed: Emergency rotation requires HSM or KMS - LOCAL_SECURE not permitted'
      );
    });

    it('should fail with HSM provider but missing HSM config', async () => {
      const invalidHsmConfig = {
        keyManagement: {
          provider: 'HSM'
          // hsmConfig missing
        }
      } as SecurityConfig;

      const rotation = new EmergencyKeyRotation(invalidHsmConfig);

      await expect(rotation.executeEmergencyRotation()).rejects.toThrow(
        'Emergency key rotation failed: HSM configuration missing'
      );
    });

    it('should fail with KMS provider but missing KMS config', async () => {
      const invalidKmsConfig = {
        keyManagement: {
          provider: 'KMS'
          // kmsConfig missing
        }
      } as SecurityConfig;

      const rotation = new EmergencyKeyRotation(invalidKmsConfig);

      await expect(rotation.executeEmergencyRotation()).rejects.toThrow(
        'Emergency key rotation failed: KMS configuration missing'
      );
    });

    it('should generate unique key IDs', async () => {
      const rotation1 = new EmergencyKeyRotation(validHsmConfig);
      const rotation2 = new EmergencyKeyRotation(validHsmConfig);

      const result1 = await rotation1.executeEmergencyRotation();
      const result2 = await rotation2.executeEmergencyRotation();

      expect(result1.newKeyId).not.toBe(result2.newKeyId);
    });

    it('should include timestamp in ISO format', async () => {
      const rotation = new EmergencyKeyRotation(validHsmConfig);

      const result = await rotation.executeEmergencyRotation();

      expect(result.timestamp).toMatch(/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z$/);
    });

    it('should maintain audit trail throughout process', async () => {
      const rotation = new EmergencyKeyRotation(validHsmConfig);

      const result = await rotation.executeEmergencyRotation();

      // Verify audit trail is comprehensive and ordered
      expect(result.auditTrail.length).toBeGreaterThanOrEqual(5);

      // Check order of audit entries
      const auditText = result.auditTrail.join('\n');
      const initiatedIndex = auditText.indexOf('Emergency rotation initiated');
      const validatedIndex = auditText.indexOf('Configuration validated');
      const keyGenIndex = auditText.indexOf('New key generated');
      const rotationIndex = auditText.indexOf('Key rotation:');
      const verificationIndex = auditText.indexOf('Key rotation verification');
      const completedIndex = auditText.indexOf('Emergency rotation completed');

      expect(initiatedIndex).toBeLessThan(validatedIndex);
      expect(validatedIndex).toBeLessThan(keyGenIndex);
      expect(keyGenIndex).toBeLessThan(rotationIndex);
      expect(rotationIndex).toBeLessThan(verificationIndex);
      expect(verificationIndex).toBeLessThan(completedIndex);
    });
  });
});

describe('executeEmergencyResponse', () => {
  let validConfig: SecurityConfig;

  beforeEach(() => {
    validConfig = {
      keyManagement: {
        provider: 'HSM',
        hsmConfig: {
          endpoint: 'hsm://test-endpoint',
          credentials: 'test-creds'
        }
      }
    } as SecurityConfig;
  });

  it('should execute emergency response successfully', async () => {
    const result = await executeEmergencyResponse(validConfig);

    expect(result.success).toBe(true);
    expect(result.newKeyId).toBeDefined();
    expect(result.oldKeyId).toBeDefined();
    expect(result.timestamp).toBeDefined();
    expect(result.auditTrail).toBeInstanceOf(Array);
  });

  it('should fail with invalid config', async () => {
    const invalidConfig = {
      keyManagement: {
        provider: 'LOCAL_SECURE'
      }
    } as SecurityConfig;

    await expect(executeEmergencyResponse(invalidConfig)).rejects.toThrow(
      'Emergency key rotation failed'
    );
  });

  it('should return same result as direct class usage', async () => {
    const directRotation = new EmergencyKeyRotation(validConfig);
    const directResult = await directRotation.executeEmergencyRotation();

    const functionResult = await executeEmergencyResponse(validConfig);

    // Results should have same structure (but different values due to timestamps)
    expect(functionResult.success).toBe(directResult.success);
    expect(typeof functionResult.newKeyId).toBe(typeof directResult.newKeyId);
    expect(typeof functionResult.oldKeyId).toBe(typeof directResult.oldKeyId);
    expect(typeof functionResult.timestamp).toBe(typeof directResult.timestamp);
    expect(Array.isArray(functionResult.auditTrail)).toBe(Array.isArray(directResult.auditTrail));
  });
});