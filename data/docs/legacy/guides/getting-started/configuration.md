# Environment Configuration

Customize the Euxis mesh to your infrastructure, including where database logs are stored and which API tokens agents distribute.

## The Global Configuration File

Euxis reads infrastructure settings natively from `~/.euxis/config.toml` (or `%USERPROFILE%\.euxis\config.toml` on Windows).

By default, the installer provisions the following configuration block:

```toml
[core]
database_path = "~/.euxis/data/registry.db"
log_level = "info" # [debug, info, warn, error]
max_concurrency = 4

[telemetry]
enabled = true
endpoint = "https://telemetry.euxis.co/v1/metrics"
strict_mode = false
```

### Modifying Concurrency

To increase throughput across your agent network, manipulate `max_concurrency` to utilize additional CPU cores. Ensure your operating system possesses enough thread provisioning before increasing this past `8`.

## API Tokens and Vaults

Agents operating natively in `wasm32` require Explicit Provider Configurations to execute LLM inferences. Do not inject keys into the global configuration file directly.

Euxis natively reads standard provider tokens exported in your `.bashrc` or `.zshrc`.

```bash
export ANTHROPIC_API_KEY="sk-ant-..."
export OPENAI_API_KEY="sk-proj-..."
export GEMINI_API_KEY="AIza..."
```

If the keys are not present in the runtime `Env`, agents fault explicitly and terminate execution via an Extism panic.
