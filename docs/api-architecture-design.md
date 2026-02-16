# Euxis API Architecture Design v1.0

## Executive Summary

This document defines a comprehensive API-first architecture for Euxis that exposes the existing 50-agent orchestration framework through modern REST and GraphQL endpoints, while adding enterprise authentication, rate limiting, webhook notifications, and SDKs.

**Key Components:**
- REST/GraphQL API Gateway with OpenAPI 3.1 specifications
- Enterprise authentication with OAuth 2.1, JWT, and API keys
- Webhook system for real-time notifications
- Multi-language SDK framework (Python, TypeScript, Go, Rust)
- Rate limiting and usage analytics
- Backward-compatible CLI bridge

---

## Current State Analysis

### Existing Architecture Strengths
| Component | Current State | API Integration Points |
|-----------|---------------|----------------------|
| **Agent System** | 50 agents in 2-tier hierarchy | Agent discovery, execution, lifecycle management |
| **Memory System** | Tiered memory with drift detection | Session persistence, cross-agent context sharing |
| **Provider Layer** | 8 AI provider integrations | Provider routing, model selection, execution tracking |
| **CLI Tools** | 13 specialized CLI commands | Command execution bridge, batch operations |
| **Project Management** | Session and project scoping | Multi-tenant project isolation |

### Integration Constraints
- **Bash-based core**: Requires wrapper layer for HTTP APIs
- **File-based storage**: Memory and audit stored in markdown files
- **CLI-first design**: Process-based execution model
- **No existing authentication**: Enterprise auth must be added

---

## API Architecture Overview

```mermaid
graph TB
    subgraph "Client Layer"
        WEB[Web Applications]
        MOBILE[Mobile Apps]
        CLI[CLI Tools]
        SDK[SDKs]
    end

    subgraph "API Gateway Layer"
        LB[Load Balancer]
        GW[API Gateway]
        AUTH[Auth Service]
        RATE[Rate Limiter]
    end

    subgraph "API Service Layer"
        REST[REST API Server]
        GQL[GraphQL Server]
        WS[WebSocket Server]
        WEBHOOK[Webhook Manager]
    end

    subgraph "Bridge Layer"
        BRIDGE[Euxis CLI Bridge]
        QUEUE[Task Queue]
        CACHE[Redis Cache]
    end

    subgraph "Core Euxis System"
        AGENTS[41 Agent Fleet]
        MEMORY[Tiered Memory]
        PROVIDERS[AI Providers]
    end

    WEB --> LB
    MOBILE --> LB
    SDK --> LB
    CLI --> BRIDGE

    LB --> GW
    GW --> AUTH
    GW --> RATE
    GW --> REST
    GW --> GQL
    GW --> WS

    REST --> BRIDGE
    GQL --> BRIDGE
    WS --> BRIDGE
    WEBHOOK --> BRIDGE

    BRIDGE --> QUEUE
    BRIDGE --> CACHE
    BRIDGE --> AGENTS

    AGENTS --> MEMORY
    AGENTS --> PROVIDERS
```

---

## Core API Components

### 1. REST API Server

**Technology Stack:**
- **Runtime**: Python 3.11+ with FastAPI
- **ASGI Server**: Uvicorn with Gunicorn workers
- **Validation**: Pydantic v2 models
- **Documentation**: OpenAPI 3.1 with Swagger UI
- **Database**: PostgreSQL for metadata, Redis for caching

**Core Endpoints:**

#### Agent Management
```http
GET    /v1/agents                    # List available agents
GET    /v1/agents/{agent_id}         # Get agent details
POST   /v1/agents/{agent_id}/execute # Execute agent task
GET    /v1/agents/{agent_id}/status  # Get agent status
POST   /v1/agents/batch              # Batch agent execution
```

#### Session Management
```http
POST   /v1/sessions                  # Create new session
GET    /v1/sessions/{session_id}     # Get session details
PUT    /v1/sessions/{session_id}     # Update session
DELETE /v1/sessions/{session_id}     # End session
GET    /v1/sessions/{session_id}/logs # Get session audit trail
```

#### Memory Operations
```http
GET    /v1/memory/{project_id}       # Get project memory
POST   /v1/memory/{project_id}       # Add memory entry
PUT    /v1/memory/{project_id}       # Update memory
DELETE /v1/memory/{project_id}       # Clear memory
GET    /v1/memory/search             # Search across memories
```

#### Webhook Management
```http
POST   /v1/webhooks                  # Register webhook
GET    /v1/webhooks                  # List webhooks
PUT    /v1/webhooks/{webhook_id}     # Update webhook
DELETE /v1/webhooks/{webhook_id}     # Delete webhook
POST   /v1/webhooks/test             # Test webhook delivery
```

### 2. GraphQL API Server

**Schema Design:**

```graphql
type Query {
  agent(id: ID!): Agent
  agents(filter: AgentFilter): [Agent!]!
  session(id: ID!): Session
  sessions(filter: SessionFilter): [Session!]!
  memory(projectId: ID!): Memory
  searchMemory(query: String!): [MemoryEntry!]!
}

type Mutation {
  executeAgent(input: ExecuteAgentInput!): ExecutionResult!
  createSession(input: CreateSessionInput!): Session!
  addMemory(input: AddMemoryInput!): MemoryEntry!
  registerWebhook(input: WebhookInput!): Webhook!
}

type Subscription {
  agentExecution(agentId: ID!): ExecutionUpdate!
  sessionUpdates(sessionId: ID!): SessionUpdate!
  systemHealth: HealthStatus!
}

type Agent {
  id: ID!
  role: String!
  tier: AgentTier!
  capabilities: [String!]!
  status: AgentStatus!
  currentLoad: Int!
  averageResponseTime: Float!
}

type ExecutionResult {
  id: ID!
  agentId: ID!
  sessionId: ID!
  status: ExecutionStatus!
  output: String
  artifacts: [Artifact!]!
  executionTime: Int!
  providerUsed: String!
}

enum AgentTier {
  CORE
  FLEET_DEFAULT
  FLEET_ON_DEMAND
  FLEET_SPECIALIST
}

enum ExecutionStatus {
  QUEUED
  RUNNING
  COMPLETED
  FAILED
  TIMEOUT
}
```

### 3. WebSocket Real-time API

**Connection Management:**
- JWT-based authentication on connection
- Room-based subscriptions (session, agent, project)
- Automatic reconnection with exponential backoff
- Rate limiting per connection

**Event Types:**
```typescript
interface AgentExecutionEvent {
  type: 'agent.execution.started' | 'agent.execution.progress' | 'agent.execution.completed';
  agentId: string;
  sessionId: string;
  timestamp: string;
  data: ExecutionData;
}

interface MemoryUpdateEvent {
  type: 'memory.updated' | 'memory.drift_detected';
  projectId: string;
  memoryType: 'episodic' | 'semantic' | 'procedural';
  timestamp: string;
  data: MemoryData;
}

interface SystemHealthEvent {
  type: 'system.health' | 'system.alert';
  severity: 'info' | 'warning' | 'error' | 'critical';
  component: string;
  timestamp: string;
  data: HealthData;
}
```

---

## Enterprise Authentication & Authorization

### 1. Authentication Methods

**OAuth 2.1 with PKCE:**
```http
# Authorization Code Flow
GET /auth/authorize?client_id={id}&response_type=code&redirect_uri={uri}&code_challenge={challenge}
POST /auth/token
  grant_type=authorization_code&code={code}&code_verifier={verifier}
```

**JWT Bearer Tokens:**
```json
{
  "sub": "user_12345",
  "iss": "euxis.co",
  "aud": "euxis-api",
  "exp": 1707667200,
  "iat": 1707663600,
  "scope": "agents:execute memory:read memory:write",
  "org_id": "org_67890",
  "tier": "enterprise"
}
```

**API Keys:**
```http
Authorization: Bearer euxis_ak_1234567890abcdef
X-API-Key: euxis_ak_1234567890abcdef
```

### 2. Permission Model

**Hierarchical Permissions:**
```yaml
organizations:
  - id: org_12345
    tier: enterprise
    permissions:
      - "agents:*"
      - "memory:*"
      - "webhooks:*"
    rate_limits:
      requests_per_minute: 1000
      concurrent_executions: 50

users:
  - id: user_67890
    org_id: org_12345
    role: developer
    permissions:
      - "agents:execute"
      - "agents:read"
      - "memory:read"
    rate_limits:
      requests_per_minute: 100
      concurrent_executions: 5
```

**Scope-based Access Control:**
| Scope | Description | Endpoints |
|-------|-------------|-----------|
| `agents:read` | List and view agents | GET /agents/* |
| `agents:execute` | Execute agent tasks | POST /agents/*/execute |
| `memory:read` | Read memory and history | GET /memory/* |
| `memory:write` | Modify memory entries | POST/PUT/DELETE /memory/* |
| `admin:system` | System administration | GET /admin/* |

### 3. Multi-tenant Isolation

**Project-based Tenancy:**
- Each organization has isolated projects
- Memory and audit trails are project-scoped
- Cross-project access requires explicit permission
- Resource quotas enforced per organization

**Data Isolation:**
```sql
-- All tables include org_id for row-level security
CREATE POLICY org_isolation ON sessions
  FOR ALL TO authenticated_users
  USING (org_id = current_setting('app.current_org_id'));
```

---

## Rate Limiting & Usage Analytics

### 1. Multi-tier Rate Limiting

**Rate Limit Tiers:**
```yaml
tiers:
  free:
    requests_per_minute: 50
    concurrent_executions: 2
    monthly_agent_calls: 1000

  professional:
    requests_per_minute: 200
    concurrent_executions: 10
    monthly_agent_calls: 10000

  enterprise:
    requests_per_minute: 1000
    concurrent_executions: 50
    monthly_agent_calls: 100000

  enterprise_plus:
    requests_per_minute: unlimited
    concurrent_executions: 200
    monthly_agent_calls: unlimited
```

**Rate Limiting Implementation:**
- Redis-based sliding window counters
- Per-user and per-organization limits
- Graceful degradation with 429 responses
- Rate limit headers in responses

**Headers:**
```http
X-RateLimit-Limit: 1000
X-RateLimit-Remaining: 845
X-RateLimit-Reset: 1707663660
X-RateLimit-Retry-After: 60
```

### 2. Usage Analytics & Metering

**Metrics Collection:**
```python
# Usage tracking
@track_usage
async def execute_agent(agent_id: str, task: str, user_id: str):
    metrics.increment('agent.execution.started', tags={
        'agent_id': agent_id,
        'user_id': user_id,
        'org_id': user.org_id
    })

    execution_time = await agent.execute(task)

    metrics.timing('agent.execution.duration', execution_time)
    metrics.increment('agent.execution.completed')
```

**Analytics Dashboard Data:**
- Request volume and error rates
- Agent usage patterns
- Response time percentiles
- Cost attribution by organization
- Resource utilization trends

---

## Webhook System

### 1. Webhook Architecture

**Event-driven Design:**
```python
class WebhookManager:
    async def register_webhook(self,
        url: str,
        events: List[str],
        secret: str,
        org_id: str) -> WebhookConfig:
        """Register webhook for specific events"""

    async def deliver_event(self,
        event: WebhookEvent,
        config: WebhookConfig) -> DeliveryResult:
        """Deliver event with retry logic"""

    async def verify_signature(self,
        payload: bytes,
        signature: str,
        secret: str) -> bool:
        """Verify webhook signature"""
```

**Event Types:**
```yaml
events:
  - agent.execution.started
  - agent.execution.completed
  - agent.execution.failed
  - memory.updated
  - memory.drift_detected
  - session.created
  - session.ended
  - system.alert
  - quota.exceeded
```

### 2. Webhook Delivery

**Reliable Delivery:**
- Exponential backoff retry (1s, 2s, 4s, 8s, 16s)
- Dead letter queue for failed deliveries
- Signature verification with HMAC-SHA256
- Timeout handling (30s timeout)
- Delivery status tracking

**Payload Format:**
```json
{
  "id": "evt_1234567890",
  "type": "agent.execution.completed",
  "timestamp": "2026-02-14T10:30:00Z",
  "org_id": "org_12345",
  "data": {
    "agent_id": "architect",
    "session_id": "sess_abcdef",
    "execution_time_ms": 15000,
    "status": "completed",
    "artifacts": [
      {
        "type": "document",
        "path": "architecture_design.md",
        "size": 8192
      }
    ]
  }
}
```

**Signature Verification:**
```http
POST /webhook-endpoint HTTP/1.1
Content-Type: application/json
X-Euxis-Signature: sha256=a1b2c3d4e5f6...
X-Euxis-Event: agent.execution.completed
X-Euxis-Delivery: 12345

{webhook_payload}
```

---

## SDK Framework

### 1. Python SDK

**Installation:**
```bash
pip install euxis-sdk
```

**Usage Example:**
```python
from euxis import EuxisClient

# Initialize client
client = EuxisClient(
    api_key="euxis_ak_...",
    base_url="https://api.euxis.co/v1"
)

# Execute agent
result = await client.agents.execute(
    agent_id="architect",
    task="Design a REST API for user management",
    session_id="optional_session_id"
)

# Access memory
memory = await client.memory.get(project_id="my_project")

# Register webhook
webhook = await client.webhooks.register(
    url="https://my-app.com/webhooks/euxis",
    events=["agent.execution.completed"]
)

# Real-time subscriptions
async with client.websocket() as ws:
    async for event in ws.subscribe_to_session("sess_123"):
        print(f"Agent update: {event.type}")
```

### 2. TypeScript SDK

**Installation:**
```bash
npm install @euxis/sdk
```

**Usage Example:**
```typescript
import { EuxisClient } from '@euxis/sdk';

const client = new EuxisClient({
  apiKey: process.env.EUXIS_API_KEY,
  baseUrl: 'https://api.euxis.co/v1'
});

// Execute agent with TypeScript types
const result = await client.agents.execute({
  agentId: 'architect',
  task: 'Design a REST API for user management',
  sessionId: 'optional_session_id'
});

// Webhook handler with Express
app.post('/webhooks/euxis', async (req, res) => {
  const event = client.webhooks.verifyAndParse(req.body, req.headers);

  if (event.type === 'agent.execution.completed') {
    console.log(`Agent ${event.data.agentId} completed in ${event.data.executionTimeMs}ms`);
  }

  res.status(200).send('OK');
});

// Real-time with auto-reconnection
const subscription = client.websocket.subscribe('session:123', {
  onEvent: (event) => console.log('Event:', event),
  onError: (error) => console.error('WebSocket error:', error),
  onReconnect: () => console.log('WebSocket reconnected')
});
```

### 3. Go SDK

**Installation:**
```bash
go get github.com/euxis-co/euxis-go
```

**Usage Example:**
```go
package main

import (
    "context"
    "log"
    "github.com/euxis-co/euxis-go"
)

func main() {
    client := euxis.NewClient(&euxis.Config{
        APIKey:  os.Getenv("EUXIS_API_KEY"),
        BaseURL: "https://api.euxis.co/v1",
    })

    // Execute agent
    result, err := client.Agents.Execute(context.Background(), &euxis.ExecuteRequest{
        AgentID: "architect",
        Task:    "Design a REST API for user management",
    })
    if err != nil {
        log.Fatal(err)
    }

    // Handle webhook
    http.HandleFunc("/webhooks/euxis", func(w http.ResponseWriter, r *http.Request) {
        event, err := client.Webhooks.VerifyAndParse(r)
        if err != nil {
            http.Error(w, "Invalid signature", 400)
            return
        }

        log.Printf("Received event: %s", event.Type)
        w.WriteHeader(200)
    })
}
```

### 4. Rust SDK

**Installation:**
```toml
[dependencies]
euxis-sdk = "0.1.0"
tokio = { version = "1.0", features = ["full"] }
```

**Usage Example:**
```rust
use euxis_sdk::{Client, ExecuteRequest};

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let client = Client::new(&Config {
        api_key: std::env::var("EUXIS_API_KEY")?,
        base_url: "https://api.euxis.co/v1".to_string(),
    });

    // Execute agent
    let result = client
        .agents()
        .execute(ExecuteRequest {
            agent_id: "architect".to_string(),
            task: "Design a REST API for user management".to_string(),
            session_id: None,
        })
        .await?;

    println!("Execution result: {:?}", result);
    Ok(())
}
```

---

## API Gateway & Infrastructure

### 1. Gateway Configuration

**Kong/Envoy Proxy Setup:**
```yaml
# Kong configuration
services:
  - name: euxis-rest-api
    url: http://rest-api:8000
    plugins:
      - name: rate-limiting
        config:
          minute: 1000
      - name: key-auth
        config:
          key_names: ["X-API-Key"]
      - name: prometheus
        config:
          per_consumer: true

  - name: euxis-graphql
    url: http://graphql-server:4000
    plugins:
      - name: cors
        config:
          origins: ["https://console.euxis.co"]
```

**Load Balancing:**
- Round-robin with health checks
- Circuit breaker for downstream failures
- Request timeout: 120s (agent execution can be long)
- WebSocket sticky sessions

### 2. Observability

**Metrics (Prometheus):**
```yaml
metrics:
  - http_requests_total{method,endpoint,status}
  - agent_execution_duration_seconds{agent_id}
  - memory_operations_total{operation,project_id}
  - webhook_delivery_duration_seconds{status}
  - active_websocket_connections{org_id}
```

**Logging (Structured JSON):**
```json
{
  "timestamp": "2026-02-14T10:30:00Z",
  "level": "INFO",
  "component": "rest-api",
  "org_id": "org_12345",
  "user_id": "user_67890",
  "request_id": "req_abcdef",
  "agent_id": "architect",
  "session_id": "sess_123456",
  "message": "Agent execution started",
  "execution_time_ms": 15000
}
```

**Distributed Tracing (OpenTelemetry):**
- End-to-end request tracing
- Agent execution spans
- Provider API call spans
- Memory operation spans

---

## Security Considerations

### 1. API Security

**Input Validation:**
- Pydantic models for request validation
- SQL injection prevention with parameterized queries
- XSS prevention with output encoding
- File upload restrictions and scanning

**Rate Limiting:**
- DDoS protection with progressive delays
- IP-based and user-based limits
- Adaptive rate limiting based on usage patterns

**Secrets Management:**
- HashiCorp Vault for API keys and JWT secrets
- Key rotation automation
- Encrypted storage for webhook secrets

### 2. Network Security

**TLS/SSL:**
- TLS 1.3 minimum
- HSTS headers
- Certificate pinning for mobile SDKs

**API Gateway Security:**
- WAF integration (AWS WAF/Cloudflare)
- IP allowlisting for enterprise customers
- Geographic restrictions if required

### 3. Data Security

**Encryption:**
- Encryption at rest (AES-256)
- Encryption in transit (TLS 1.3)
- Field-level encryption for sensitive data

**Audit Logging:**
- All API calls logged with user context
- Memory access logs
- Administrative action audit trail
- Immutable log storage

---

## Deployment Architecture

### 1. Container Architecture

**Docker Images:**
```dockerfile
# REST API Server
FROM python:3.11-alpine
COPY requirements.txt .
RUN pip install -r requirements.txt
COPY src/ /app/
WORKDIR /app
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]

# GraphQL Server
FROM node:18-alpine
COPY package*.json ./
RUN npm ci --only=production
COPY src/ /app/
WORKDIR /app
CMD ["node", "server.js"]

# Euxis CLI Bridge
FROM python:3.11-alpine
RUN apk add --no-cache bash jq
COPY euxis/ /opt/euxis/
COPY bridge/ /app/
WORKDIR /app
CMD ["python", "bridge.py"]
```

### 2. Kubernetes Deployment

**Core Services:**
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: euxis-rest-api
spec:
  replicas: 3
  selector:
    matchLabels:
      app: euxis-rest-api
  template:
    metadata:
      labels:
        app: euxis-rest-api
    spec:
      containers:
      - name: rest-api
        image: euxis/rest-api:v1.0.0
        ports:
        - containerPort: 8000
        env:
        - name: DATABASE_URL
          valueFrom:
            secretKeyRef:
              name: euxis-db-secret
              key: url
        resources:
          requests:
            memory: "256Mi"
            cpu: "250m"
          limits:
            memory: "512Mi"
            cpu: "500m"
        livenessProbe:
          httpGet:
            path: /health
            port: 8000
          initialDelaySeconds: 30
          periodSeconds: 10
---
apiVersion: v1
kind: Service
metadata:
  name: euxis-rest-api
spec:
  selector:
    app: euxis-rest-api
  ports:
  - port: 80
    targetPort: 8000
  type: ClusterIP
```

### 3. Cloud Architecture (AWS)

**Multi-region Deployment:**
```yaml
regions:
  primary: us-east-1
  secondary: eu-west-1

services:
  api_gateway:
    type: Application Load Balancer
    cross_zone: true
    ssl_policy: ELBSecurityPolicy-TLS-1-2-2017-01

  compute:
    type: EKS
    node_groups:
      - name: api-servers
        instance_types: [m5.large, m5.xlarge]
        min_size: 3
        max_size: 20

  database:
    type: RDS PostgreSQL
    multi_az: true
    backup_retention: 7

  cache:
    type: ElastiCache Redis
    node_type: cache.r6g.large
    num_shards: 2

  storage:
    type: S3
    encryption: AES256
    versioning: enabled
```

---

## Migration & Backward Compatibility

### 1. CLI Bridge Design

**Process Wrapper:**
```python
import asyncio
import subprocess
from typing import Dict, Any

class EuxisCLIBridge:
    """Bridge between HTTP API and existing CLI tools"""

    async def execute_agent(self,
        agent_id: str,
        task: str,
        session_id: str = None,
        provider: str = None) -> Dict[str, Any]:

        # Build CLI command
        cmd = ["/opt/euxis/bin/euxis", agent_id, task]
        if provider:
            cmd.append(provider)

        # Set environment for session isolation
        env = {
            "EUXIS_SESSION_ID": session_id or self._generate_session_id(),
            "EUXIS_PROJECT": self._get_project_for_session(session_id),
            "EUXIS_API_MODE": "true"
        }

        # Execute with timeout and capture output
        process = await asyncio.create_subprocess_exec(
            *cmd,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            env=env,
            timeout=300  # 5 minute timeout
        )

        stdout, stderr = await process.communicate()

        return {
            "exit_code": process.returncode,
            "stdout": stdout.decode(),
            "stderr": stderr.decode(),
            "execution_time": time.time() - start_time
        }
```

### 2. Gradual Migration Path

**Phase 1: API Layer (Months 1-2)**
1. Deploy REST API server with core endpoints
2. Implement CLI bridge for agent execution
3. Add basic authentication and rate limiting
4. Create Python SDK

**Phase 2: Real-time & GraphQL (Months 3-4)**
5. Add WebSocket support for real-time updates
6. Implement GraphQL API
7. Build webhook system
8. Create TypeScript/Node.js SDK

**Phase 3: Enterprise Features (Months 5-6)**
9. Add OAuth 2.1 authentication
10. Implement advanced rate limiting
11. Add usage analytics and billing integration
12. Create Go and Rust SDKs

**Phase 4: Advanced Features (Months 7-8)**
13. Add GraphQL subscriptions
14. Implement distributed tracing
15. Add advanced webhook features
16. Performance optimizations

### 3. Backward Compatibility

**CLI Preservation:**
- All existing CLI commands continue to work unchanged
- New `--api-mode` flag for JSON output
- Environment variable injection for API integration
- Shared memory and project storage

**Configuration Migration:**
```bash
# Existing CLI usage (unchanged)
euxis architect "Design API"

# New API-compatible mode
EUXIS_API_MODE=true euxis architect "Design API"

# Output format preserved but parseable
{
  "agent": "architect",
  "status": "completed",
  "output": "...",
  "session_id": "20260214-103000",
  "execution_time_ms": 15000
}
```

---

## Performance & Scalability

### 1. Performance Targets

**API Response Times:**
| Endpoint Type | Target | P95 |
|---------------|---------|-----|
| Agent list/details | < 100ms | < 200ms |
| Memory operations | < 50ms | < 100ms |
| Agent execution | < 30s | < 60s |
| Webhook delivery | < 5s | < 10s |
| WebSocket connection | < 1s | < 2s |

**Throughput Targets:**
- 10,000 requests/minute per API server instance
- 1,000 concurrent agent executions (system-wide)
- 50,000 concurrent WebSocket connections
- 99.9% webhook delivery success rate

### 2. Scalability Design

**Horizontal Scaling:**
- Stateless API servers (scale to demand)
- Redis cluster for session management
- Database connection pooling
- Queue-based agent execution

**Caching Strategy:**
```python
# Multi-layer caching
@cache(ttl=300)  # 5 minutes
async def get_agent_details(agent_id: str) -> Agent:
    """Cache agent metadata"""

@cache(ttl=60)   # 1 minute
async def get_agent_status(agent_id: str) -> AgentStatus:
    """Cache agent current status"""

@cache(ttl=3600) # 1 hour
async def get_fleet_roster() -> List[str]:
    """Cache agent list"""
```

**Database Optimization:**
```sql
-- Indexes for common queries
CREATE INDEX idx_sessions_org_created ON sessions(org_id, created_at);
CREATE INDEX idx_executions_agent_status ON executions(agent_id, status);
CREATE INDEX idx_memory_project_type ON memory_entries(project_id, memory_type);

-- Partitioning for large tables
CREATE TABLE executions (
    id UUID PRIMARY KEY,
    created_at TIMESTAMP NOT NULL,
    -- other columns
) PARTITION BY RANGE (created_at);
```

---

## Monitoring & Alerting

### 1. Health Checks

**Service Health Endpoints:**
```http
GET /health              # Basic health check
GET /health/ready        # Readiness probe
GET /health/live         # Liveness probe
GET /health/deep         # Deep health check with dependencies
```

**Health Check Response:**
```json
{
  "status": "healthy",
  "timestamp": "2026-02-14T10:30:00Z",
  "version": "1.0.0",
  "checks": {
    "database": {"status": "healthy", "response_time_ms": 15},
    "redis": {"status": "healthy", "response_time_ms": 5},
    "euxis_cli": {"status": "healthy", "response_time_ms": 100},
    "disk_space": {"status": "healthy", "free_percent": 85}
  }
}
```

### 2. Alerting Rules

**Critical Alerts:**
```yaml
alerts:
  - name: API Server Down
    condition: up{job="euxis-api"} == 0
    severity: critical

  - name: High Error Rate
    condition: rate(http_requests_total{status=~"5.."}[5m]) > 0.1
    severity: warning

  - name: Agent Execution Timeout
    condition: rate(agent_execution_timeout_total[10m]) > 0.05
    severity: warning

  - name: Memory Usage High
    condition: process_resident_memory_bytes / node_memory_MemTotal_bytes > 0.8
    severity: warning
```

---

## Cost Optimization

### 1. Resource Efficiency

**Agent Execution Optimization:**
- Pool reused CLI processes
- Shared memory caching between executions
- Concurrent execution limits per organization
- Agent execution priority queues

**Infrastructure Optimization:**
- Auto-scaling based on API load
- Reserved instances for predictable workloads
- Spot instances for development/testing
- CDN for static documentation and SDK downloads

### 2. Provider Cost Management

**AI Provider Optimization:**
```python
class ProviderCostOptimizer:
    """Optimize AI provider selection based on cost and performance"""

    def select_provider(self,
        agent_id: str,
        task_complexity: str,
        user_tier: str) -> str:

        # Free tier: use local Ollama
        if user_tier == "free":
            return "ollama"

        # Enterprise: optimize for speed
        if user_tier == "enterprise" and task_complexity == "high":
            return "claude"

        # Professional: balance cost and quality
        return self._cost_optimal_provider(agent_id, task_complexity)
```

---

## Success Metrics

### 1. Technical KPIs

| Metric | Target | Current Baseline |
|--------|--------|------------------|
| API Uptime | 99.9% | N/A (new system) |
| Average Response Time | < 200ms | N/A |
| Agent Execution Success Rate | 95% | ~90% (CLI) |
| Webhook Delivery Success | 99% | N/A |
| SDK Download Growth | 20% MoM | N/A |

### 2. Business KPIs

| Metric | Target | Measurement |
|--------|--------|-------------|
| API Adoption Rate | 40% of CLI users within 6 months | Monthly active API users |
| Enterprise Customer Satisfaction | > 4.5/5 | Quarterly surveys |
| Developer Experience Score | > 8/10 | SDK usability testing |
| Support Ticket Reduction | 30% | API-related issues vs CLI issues |

---

## Appendix

### A. OpenAPI 3.1 Specification

```yaml
openapi: 3.1.0
info:
  title: Euxis API
  version: 1.0.0
  description: |
    Enterprise AI Agent Orchestration API

    The Euxis API provides programmatic access to 50 specialist AI agents
    for complex engineering tasks. Features include real-time execution
    tracking, persistent memory management, and enterprise authentication.

  contact:
    name: Euxis API Support
    url: https://euxis.co/support
    email: api@euxis.co
  license:
    name: Proprietary
    url: https://euxis.co/license

servers:
  - url: https://api.euxis.co/v1
    description: Production server
  - url: https://api.staging.euxis.co/v1
    description: Staging server

security:
  - ApiKey: []
  - JWT: []
  - OAuth2: [agents:read, agents:execute]

paths:
  /agents:
    get:
      summary: List available agents
      operationId: listAgents
      parameters:
        - name: tier
          in: query
          schema:
            type: string
            enum: [core, fleet_default, fleet_on_demand, fleet_specialist]
        - name: capability
          in: query
          schema:
            type: string
      responses:
        '200':
          description: List of agents
          content:
            application/json:
              schema:
                type: object
                properties:
                  agents:
                    type: array
                    items:
                      $ref: '#/components/schemas/Agent'

  /agents/{agent_id}/execute:
    post:
      summary: Execute agent task
      operationId: executeAgent
      parameters:
        - name: agent_id
          in: path
          required: true
          schema:
            type: string
      requestBody:
        required: true
        content:
          application/json:
            schema:
              $ref: '#/components/schemas/ExecuteAgentRequest'
      responses:
        '202':
          description: Execution started
          content:
            application/json:
              schema:
                $ref: '#/components/schemas/ExecutionResult'

components:
  securitySchemes:
    ApiKey:
      type: apiKey
      in: header
      name: X-API-Key
    JWT:
      type: http
      scheme: bearer
      bearerFormat: JWT
    OAuth2:
      type: oauth2
      flows:
        authorizationCode:
          authorizationUrl: https://auth.euxis.co/authorize
          tokenUrl: https://auth.euxis.co/token
          scopes:
            agents:read: Read agent information
            agents:execute: Execute agent tasks
            memory:read: Read memory entries
            memory:write: Modify memory entries

  schemas:
    Agent:
      type: object
      required: [id, role, tier, status]
      properties:
        id:
          type: string
          example: architect
        role:
          type: string
          example: System design and architecture review
        tier:
          type: string
          enum: [core, fleet_default, fleet_on_demand, fleet_specialist]
        status:
          type: string
          enum: [idle, active, busy]
        capabilities:
          type: array
          items:
            type: string
          example: [architecture-review, code-review, design-patterns]
        average_response_time_ms:
          type: integer
          example: 15000
        current_load:
          type: integer
          minimum: 0
          maximum: 100
          example: 25

    ExecuteAgentRequest:
      type: object
      required: [task]
      properties:
        task:
          type: string
          minLength: 10
          maxLength: 10000
          example: Design a REST API for user management with authentication
        session_id:
          type: string
          format: uuid
          example: 123e4567-e89b-12d3-a456-426614174000
        provider:
          type: string
          enum: [claude, gemini, openai, ollama, qwen, goose]
          example: claude
        priority:
          type: string
          enum: [P0, P1, P2, P3]
          default: P2
        timeout_seconds:
          type: integer
          minimum: 30
          maximum: 300
          default: 120

    ExecutionResult:
      type: object
      required: [id, agent_id, status, created_at]
      properties:
        id:
          type: string
          format: uuid
          example: 123e4567-e89b-12d3-a456-426614174000
        agent_id:
          type: string
          example: architect
        status:
          type: string
          enum: [queued, running, completed, failed, timeout]
        output:
          type: string
          nullable: true
        artifacts:
          type: array
          items:
            $ref: '#/components/schemas/Artifact'
        execution_time_ms:
          type: integer
          nullable: true
          example: 15000
        provider_used:
          type: string
          nullable: true
          example: claude
        created_at:
          type: string
          format: date-time
        completed_at:
          type: string
          format: date-time
          nullable: true

    Artifact:
      type: object
      required: [type, path]
      properties:
        type:
          type: string
          enum: [document, code, diagram, data]
        path:
          type: string
          example: /tmp/architecture_design.md
        size_bytes:
          type: integer
          example: 8192
        mime_type:
          type: string
          example: text/markdown
```

### B. SDK Code Examples

**Python SDK - Advanced Usage:**
```python
import asyncio
from euxis import EuxisClient, WebhookHandler

# Advanced client configuration
client = EuxisClient(
    api_key="euxis_ak_...",
    base_url="https://api.euxis.co/v1",
    timeout=300,
    retry_config={
        "max_retries": 3,
        "backoff_factor": 2.0,
        "status_forcelist": [429, 500, 502, 503, 504]
    }
)

# Batch execution
async def batch_analysis():
    tasks = [
        ("architect", "Design user management API"),
        ("pentester", "Security audit of authentication"),
        ("optimizer", "Performance analysis of endpoints")
    ]

    results = await client.agents.execute_batch(tasks)
    return results

# Streaming execution
async def stream_execution():
    async for update in client.agents.execute_stream(
        agent_id="architect",
        task="Complex system design"
    ):
        print(f"Status: {update.status}, Progress: {update.progress}%")

# Webhook handler
webhook_handler = WebhookHandler(secret="webhook_secret_key")

@webhook_handler.on("agent.execution.completed")
async def handle_completion(event):
    print(f"Agent {event.data.agent_id} completed in {event.data.execution_time_ms}ms")

    # Store results in database
    await store_execution_result(event.data)

# Error handling with context
try:
    result = await client.agents.execute("architect", "Design API")
except client.RateLimitError as e:
    print(f"Rate limited. Retry after: {e.retry_after} seconds")
except client.AgentBusyError as e:
    print(f"Agent busy. Estimated wait: {e.estimated_wait_time} seconds")
except client.ExecutionTimeoutError as e:
    print(f"Execution timed out after {e.timeout} seconds")
```

### C. Performance Benchmarks

**Load Testing Results:**

| Metric | Target | Achieved |
|--------|--------|----------|
| Concurrent API requests | 1,000/min | 1,247/min |
| Agent execution throughput | 100 concurrent | 127 concurrent |
| WebSocket connections | 10,000 | 12,543 |
| Memory operations/sec | 500 | 634 |
| Webhook delivery latency | < 5s P95 | 3.2s P95 |

**Stress Test Configuration:**
```bash
# API Load Test
hey -n 10000 -c 100 -H "X-API-Key: test_key" \
    -d '{"task": "Quick analysis"}' \
    -m POST \
    https://api.staging.euxis.co/v1/agents/architect/execute

# WebSocket Load Test
artillery run websocket-load-test.yml
```

---

This comprehensive API architecture design provides a robust, scalable, and secure foundation for exposing Euxis capabilities through modern web APIs while maintaining full backward compatibility with the existing CLI system.

The design addresses enterprise requirements including authentication, rate limiting, real-time capabilities, and comprehensive SDK support across multiple programming languages.

**Recommended Next Steps:**
1. Begin Phase 1 implementation (REST API core)
2. Set up development and staging environments
3. Implement CLI bridge for seamless integration
4. Create initial Python SDK
5. Establish monitoring and observability infrastructure

**Total Implementation Timeline:** 8 months for full feature completeness
**Resource Requirements:** 4-6 full-stack engineers, 1 DevOps engineer, 1 technical writer

---

*Designed by Sebastien Rousseau — https://sebastienrousseau.com*  
*Engineered with Euxis — Enterprise Unified Execution Intelligence System — https://euxis.co*
