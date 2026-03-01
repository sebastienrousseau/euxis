# Common Errors

When engineering complex federated topologies or generating custom Extism agents natively, you inevitably confront architectural execution faults. 

Review the primary Gateway and CLI initialization exceptions and their resolution matrices.

## 1. Extism Error: SandboxViolationError

**Symptom:** The execution halts instantly and throws `SandboxViolationError` inside the CLI output trace or the TUI logger.
**Diagnosis:** The Wasm binary explicitly attempted to invoke an OS-level integration without authorization mapped inside the explicit WASI Capability Manifest.
**Resolution:** Re-read your `manifest.json` specifically. You must inject the exact URL target inside the `network` array, or specifically expose the local target directory entirely within the explicit `filesystem` container mapping structures.

## 2. API Key Instantiation Fault

**Symptom:** `ValueError: Missing Explicit Provider Initialization`.
**Diagnosis:** The default native language model architecture relies exclusively on Anthropic, OpenAI, or Google frameworks. The system natively could not locate the environment variables.
**Resolution:** Euxis does not store native secrets explicitly inside global configuration loops. Ensure that your shell (`~/.bashrc`, `~/.zshrc`) exports the definitive key identically. 

```bash
export OPENAI_API_KEY="sk-..."
```
*You must reload your terminal environment explicitly (`source ~/.bashrc`) and restart the Gateway initialization sequence completely.*

## 3. WebAssembly Memory Exhaustion

**Symptom:** `ExtismTrap: out of bounds memory access` or `OOMError`.
**Diagnosis:** The specific plugin required excessive linear memory arrays natively beyond the default Extism 10MB memory envelope mapping architecture.
**Resolution:** Pass the generic CLI overriding index flag explicitly to scale up WebAssembly constraints automatically. 

```bash
euxis playbook run --memory 50mb "Refactor the massive monolithic server integration schema"
```

## 4. Port Conflict during Initialization

**Symptom:** `OSError: [Errno 98] Address already in use`.
**Diagnosis:** You attempted to spin-up `euxis gateway serve` natively while an extraneous process identically occupied Port 8000 natively.
**Resolution:** Override the Uvicorn daemon binding target specifically natively or terminate the orphaned background process natively utilizing generic commands (`fuser -k 8000/tcp`).
