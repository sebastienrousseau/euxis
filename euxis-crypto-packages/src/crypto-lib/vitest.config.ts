import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    globals: true,
    environment: 'node',
    include: ['tests/**/*.test.ts'],
    exclude: ['node_modules/**', 'dist/**']
  },
  coverage: {
    provider: 'v8',
    reporter: ['text', 'json', 'html'],
    reportsDirectory: './coverage',
    include: [
      'src/index.ts',
      'src/types.ts',
      'src/crypto-operations.ts',
      'src/config/security.ts'
    ],
    exclude: [
      'src/**/*.test.ts',
      'src/**/*.spec.ts',
      'src/**/*.d.ts',
      'src/core/**',
      'src/lib/**',
      'src/plugins/**',
      'src/streaming/**',
      'src/observability/**',
      'src/performance/**',
      'src/emergency-rotation.ts',
      'src/types/**'
    ],
    thresholds: {
      lines: 95,
      functions: 95,
      branches: 95,
      statements: 95
    }
  }
});