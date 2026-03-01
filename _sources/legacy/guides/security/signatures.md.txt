# Cryptographic Signatures

To fundamentally mitigate supply-chain injections and malicious node infiltration over your autonomous mesh, Euxis executes rigorous cryptographic verification natively on every WebAssembly module before memory initialization. 

## The Zero-Trust Verification Loop

When you download a new playbook or compile a custom agent, it generates two files: 
1. The `.wasm` execution binary.
2. The `.sig` cryptographic signature file associated precisely with the binary.

The Euxis gateway intercepts the deployment mapping and intrinsically queries the local `registry.json`. 

If the SHA256 checksum natively fails the cryptographic verification mapping established in the source hash, the gateway throws `SandboxViolationError` and isolates the payload instantly.

## Generating Native Signatures

When constructing native Custom Agents utilizing `euxis agent build`, the CLI automatically invokes your private Elliptic Curve (ECDSA) key natively stored in `~/.euxis/keys/private.pem` to generate a verifiable header attached to exactly that specific compilation matrix.

## Verifying Extraneous Agents

You can specifically enforce manual WebAssembly binary diagnostics locally.

```bash
euxis verify ./agent-payload.wasm
```

The CLI analyzes the Wasm binary constraints, calculates the hash array, and queries the primary Euxis trusted ledger for the verification bounds. If the agent does not possess cryptographic validation, the Gateway explicitly rejects its initialization natively.
