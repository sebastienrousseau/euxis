# Capability Manifests

Euxis Agents execute strictly within locked Extism WebAssembly sandboxes. By default, Wasm modules cannot access your local file system, network interfaces, or environment variables. This prevents rogue code execution and protects your machine.

To permit an agent to perform actual work (like analyzing code or contacting an external API), you define an explicit JSON **Capability Manifest**.

## Standard Anatomy

Manifests declaratively map allowed capabilities to the virtual environments exposed inside the Extism memory envelope. A standard manifest is composed of an identifier, network array, and filesystem mapping dictionary.

```json
{
  "agent_id": "auditor_v2.0",
  "capabilities": {
    "network": [
      "https://api.github.com",
      "https://telemetry.euxis.co"
    ],
    "filesystem": {
      "/host/projects/src": "/var/lib/euxis/projects/src"
    },
    "env": [
      "ANTHROPIC_API_KEY",
      "OPENAI_API_KEY"
    ]
  }
}
```

### The `network` Array

This string array defines exact URL origins. If an agent attempts to route a `curl` equivalent or `reqwest` HTTP call to an unlisted origin, the WebAssembly module instantly panics and the gateway enforces connection termination.

### The `filesystem` Map

WebAssembly utilizes WASI (WebAssembly System Interfaces) to interact with disks.

The `filesystem` object maps physical host paths to virtual container paths.

* **Key:** The actual path on your physical machine or container.
* **Value:** The virtualized path explicitly exposed to the Wasm agent.

Never map your user `~` or `/` directory directly. Constrain agent execution specifically to your repository `src/` or `data/` endpoints.

### The `env` Constraints

Provide a string array of explicit Environment Variable keys. Extism exclusively injects these keys into the execution context. Do NOT define your raw tokens inside the manifest; simply provide the keys referencing your global host shell setup.
