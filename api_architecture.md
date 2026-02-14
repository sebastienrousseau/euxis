# API Architecture Design

## Overview
Enterprise-grade API-first architecture supporting multiple interface patterns, comprehensive authentication, and robust rate limiting for scalable multi-tenant operations.

## Core API Patterns

### REST API Endpoints
```
/api/v1/
├── /projects/{id}           # Project CRUD operations
├── /agents/{id}             # Agent management
├── /sessions/{id}           # Session lifecycle
├── /dispatch/{id}           # Task orchestration
├── /cortex/memories         # Knowledge storage
├── /graph/entities          # Graph memory
└── /fleet/status           # System health
```

**REST Design Principles:**
- Resource-based URLs with standard HTTP verbs
- JSON request/response format with consistent error schema
- Stateless operations with token-based authentication
- Pagination via `limit`, `offset`, `cursor` parameters
- ETags for optimistic concurrency control

### GraphQL Schema
```graphql
type Query {
  project(id: ID!): Project
  agent(id: ID!): Agent
  memories(filter: MemoryFilter): [Memory]
  graphEntities(type: EntityType): [Entity]
}

type Mutation {
  createProject(input: ProjectInput!): Project
  dispatchTask(manifest: DispatchManifest!): TaskResult
  storeMemory(content: String!, type: MemoryType!): Memory
}

type Subscription {
  taskProgress(sessionId: ID!): TaskUpdate
  fleetStatus: FleetStatusUpdate
}
```

**GraphQL Advantages:**
- Single endpoint with flexible query capabilities
- Real-time subscriptions for task progress
- Strongly typed schema with introspection
- Client-driven data fetching

## Webhook System

### Webhook Events
| Event Type | Trigger | Payload Schema |
|------------|---------|----------------|
| `task.completed` | Task finishes | `{sessionId, status, artifacts[]}` |
| `agent.escalated` | Agent escalation | `{fromAgent, toAgent, reason, priority}` |
| `fleet.health` | Health change | `{timestamp, status, metrics}` |
| `security.incident` | Security alert | `{severity, description, affectedResources[]}` |

### Webhook Delivery
```yaml
Reliability:
  - Exponential backoff (1s, 2s, 4s, 8s, 16s)
  - Maximum 5 retry attempts
  - Dead letter queue for permanent failures
  - Webhook signature verification (HMAC-SHA256)

Security:
  - TLS 1.3 required for all webhook endpoints
  - IP allowlist support for enterprise customers
  - Configurable timeout (5-30 seconds)
```

## SDK Framework

### Multi-Language Support
```python
# Python SDK Example
from euxis import Client, TaskManifest

client = Client(api_key="...", base_url="https://api.euxis.co")

# Dispatch task
manifest = TaskManifest(
    agent="architect",
    task="Design microservice architecture",
    priority="P1",
    verify_cmd="test -f architecture.md"
)
result = client.dispatch(manifest)

# Real-time progress
for update in client.subscribe_task_progress(result.session_id):
    print(f"Status: {update.status}")
```

```typescript
// TypeScript SDK Example
import { EuxisClient, TaskStatus } from '@euxis/sdk';

const client = new EuxisClient({
  apiKey: process.env.EUXIS_API_KEY,
  baseUrl: 'https://api.euxis.co'
});

// GraphQL query
const projects = await client.graphql(`
  query GetProjects($limit: Int!) {
    projects(limit: $limit) {
      id
      name
      agents { id role }
    }
  }
`, { limit: 10 });
```

### SDK Features
- Automatic authentication token management
- Built-in retry logic with circuit breaker pattern
- Request/response logging and debugging
- Type-safe interfaces generated from OpenAPI/GraphQL schemas
- Async/await support with streaming responses

## Enterprise Authentication

### Multi-Tenant Identity
```yaml
Authentication Stack:
  - OAuth 2.0 + PKCE for web applications
  - API keys with scope-based permissions
  - JWT tokens (RS256) with 15-minute expiry
  - Refresh tokens with 7-day sliding window
  - SAML SSO for enterprise customers

Authorization Model:
  - Role-Based Access Control (RBAC)
  - Fine-grained permissions per API endpoint
  - Tenant isolation with data segregation
  - Service-to-service authentication via mTLS
```

### Permission Matrix
| Role | Projects | Agents | Dispatch | Admin |
|------|----------|--------|----------|--------|
| Viewer | Read | Read | - | - |
| Developer | CRUD | Read | Execute | - |
| Admin | CRUD | CRUD | CRUD | Read |
| Owner | CRUD | CRUD | CRUD | CRUD |

## Rate Limiting

### Adaptive Rate Limiting
```yaml
Tier-Based Limits:
  Free:     100 req/hour,   5 concurrent
  Pro:      1000 req/hour,  20 concurrent
  Enterprise: 10000 req/hour, 100 concurrent

Rate Limiting Strategy:
  - Sliding window with Redis backend
  - Per-tenant and per-endpoint granularity
  - Burst allowance (2x limit for 60 seconds)
  - Graceful degradation with 429 responses
```

### Rate Limit Headers
```http
X-RateLimit-Limit: 1000
X-RateLimit-Remaining: 847
X-RateLimit-Reset: 1643723400
X-RateLimit-Burst-Remaining: 50
Retry-After: 300
```

## Security Architecture

### API Security Layers
1. **Transport Security**: TLS 1.3, HSTS, certificate pinning
2. **Authentication**: Multi-factor with hardware tokens for admin
3. **Authorization**: Zero-trust with least-privilege access
4. **Input Validation**: JSON schema validation, SQL injection prevention
5. **Output Security**: Response filtering, sensitive data masking
6. **Monitoring**: Real-time threat detection, API abuse prevention

### Data Protection
- End-to-end encryption for sensitive payloads
- PII tokenization with secure vault storage
- Audit logging for all data access operations
- GDPR compliance with data portability and deletion

## Monitoring & Observability

### API Metrics
```yaml
Golden Signals:
  - Latency: P50, P95, P99 response times
  - Traffic: Requests per second by endpoint
  - Errors: 4xx/5xx error rates and types
  - Saturation: CPU, memory, connection pool usage

Custom Metrics:
  - Agent utilization rates
  - Task completion success rates
  - Webhook delivery success rates
  - Authentication failure patterns
```

### Health Checks
```http
GET /health/live    # Kubernetes liveness probe
GET /health/ready   # Readiness probe with dependency checks
GET /health/metrics # Prometheus metrics endpoint
```

## Deployment Architecture

### Infrastructure Requirements
- Kubernetes with Istio service mesh for traffic management
- PostgreSQL with read replicas for metadata storage
- Redis cluster for caching and rate limiting
- Message queue (Apache Kafka) for event streaming
- CDN for SDK distribution and documentation

### Scalability Patterns
- Horizontal pod autoscaling based on request volume
- Database connection pooling with PgBouncer
- Circuit breaker pattern for external dependencies
- Graceful degradation during partial outages

## Migration Strategy

### Backward Compatibility
- API versioning with minimum 12-month support lifecycle
- Gradual deprecation with clear migration timelines
- SDK version compatibility matrix
- Automated compatibility testing in CI/CD

### Rollout Plan
1. **Phase 1**: Core REST API with basic authentication
2. **Phase 2**: GraphQL endpoint and webhook system
3. **Phase 3**: Multi-language SDK framework
4. **Phase 4**: Enterprise features and advanced rate limiting
5. **Phase 5**: Full observability and monitoring suite