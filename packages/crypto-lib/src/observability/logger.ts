import winston from 'winston';

const logFormat = winston.format.combine(
  winston.format.timestamp(),
  winston.format.errors({ stack: true }),
  winston.format.json()
);

export const cryptoLogger = winston.createLogger({
  level: process.env.LOG_LEVEL || 'info',
  format: logFormat,
  defaultMeta: { service: 'crypto-lib' },
  transports: [
    new winston.transports.Console({
      format: winston.format.combine(
        winston.format.colorize(),
        winston.format.simple()
      )
    }),
    new winston.transports.File({
      filename: 'crypto-error.log',
      level: 'error'
    }),
    new winston.transports.File({
      filename: 'crypto-combined.log'
    })
  ]
});

export class CryptoAuditLogger {
  private logger: winston.Logger;

  constructor() {
    this.logger = winston.createLogger({
      level: 'info',
      format: winston.format.combine(
        winston.format.timestamp(),
        winston.format.json()
      ),
      defaultMeta: { audit: true },
      transports: [
        new winston.transports.File({
          filename: 'crypto-audit.log',
          level: 'info'
        })
      ]
    });
  }

  logOperation(operation: string, result: any, metadata?: any) {
    this.logger.info('Crypto operation completed', {
      operation,
      result: result ? 'success' : 'failure',
      metadata,
      correlation_id: metadata?.correlationId
    });
  }
}

export const auditLogger = new CryptoAuditLogger();