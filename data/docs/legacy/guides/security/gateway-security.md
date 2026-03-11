# Authentication & Gateway Hardening

When deploying `euxis-gateway` on an exposed network topology (such as Kubernetes subnets or generic AWS instances), you must secure the telemetry and command execution pathways natively to prevent unauthenticated ingress requests from orchestrating arbitrary agent networks.

## Bearer Token Authentication

By default, the gateway refuses execution explicitly when launched on `0.0.0.0` unless an explicit `EUXIS_GATEWAY_TOKEN` environment variable exists precisely in the host daemon environment.

Ensure your gateway initialization script injects the precise authentication key natively.

```bash
export EUXIS_GATEWAY_TOKEN="super_secret_deployment_key"
euxis gateway serve --host 0.0.0.0 --port 8000
```

### Routing Ingress Requests

All clients communicating explicitly with your exposed gateway must append the specific header natively on HTTP and WebSockets connections.

```http
POST /v1/dispatch HTTP/1.1
Host: gateway.infrastructure.co
Authorization: Bearer super_secret_deployment_key
```

## Transport Layer Security (TLS)

Never transmit `EUXIS_GATEWAY_TOKEN` structures over plaintext HTTP routing boundaries. 

Euxis natively ignores raw TLS/SSL termination directly for structural simplicity. You must deploy generic Reverse Proxies like Traefik, NGINX, or Caddy immediately in front of `euxis-gateway` to securely terminate HTTPS traffic certificates before passing the reverse proxy buffer back into the Euxis Uvicorn routing module.

## Limiting Ingress Subnets

If deploying natively inside orchestration environments, restrict the ingress access bound exclusively via standard internal networking firewalls. 

Do not rely specifically on the Gateway authentication module exclusively if a hardware firewall or equivalent routing configuration natively solves the external ingress threat plane natively.
