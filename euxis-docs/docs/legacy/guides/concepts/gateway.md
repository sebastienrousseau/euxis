# The Euxis Gateway

The Gateway operates as the central nervous system connecting remote clients to your Extism execution sandboxes. Use the Gateway to manage HTTP ingress traffic, validate telemetry logs, and bridge federated host mesh architectures.

## Service Architecture

The `euxis-gateway` package exposes high-throughput asynchronous execution routes utilizing `FastAPI`.

1. **Authentication Interceptors:** All traffic hitting the Gateway immediately passes through dependency-injected cryptographic verifiers. Unsigned agent requests fail to authorize HTTP execution.
2. **WebSockets (Bi-directional Telemetry):** Real-time squad observability streams through dedicated WebSocket connections to power your Terminal User Interfaces and Dashboard analytics.
3. **The Extism Plugin Pool:** The Gateway manages a persistent pool of Extism Plugin references to mitigate subsequent WebAssembly module load times.

## Bootstrapping the Server

Launch the Gateway directly from your terminal using standard Uvicorn commands.

```bash
euxis gateway serve --port 8000 --workers 4
```

### Gateway Configurations

By default, the server only binds to localhost. Use explicit binding attributes mapped through your global `config.toml` file to expose the Gateway onto public subnets or containerized network scopes. 

```toml
[server]
host = "0.0.0.0"
port = 8000
timeout_keep_alive = 5
```

## Securing Your Integrations

All programmatic environments interacting with a remote Euxis Gateway must provide pre-shared cryptographic secrets or valid Vault identities. If you are operating completely locally on your machine, Euxis automatically bypasses the explicit authorization channels, substituting them with active session lockfiles via `tmp`.

Consult the [Authentication and Security](security/gateway-security.md) documentation prior to production rollout.
