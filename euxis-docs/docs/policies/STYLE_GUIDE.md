# Documentation Style Guide

This guide establishes standards for all Euxis documentation. Follow these conventions to ensure consistency, clarity, and accessibility across all documentation.

## Voice and Tone

### Be Direct and Professional

Write in a direct, professional, action-oriented voice. Get to the point quickly and avoid unnecessary preamble.

**Do:**
```
Run the health check to verify your installation:
```

**Do not:**
```
Before we begin, it's important to understand that running health checks is a great way to ensure everything is working properly in your environment.
```

### Use Second Person

Address the reader as "you" when providing instructions. This creates clear, actionable guidance.

**Do:**
```
You can configure the orchestrator by editing the config file.
```

**Do not:**
```
Users can configure the orchestrator by editing the config file.
The developer should edit the config file.
```

### Prefer Active Voice

Use active voice to make instructions clear and direct. Passive voice obscures who performs the action.

**Do:**
```
The orchestrator routes requests to the appropriate agent.
Run `euxis-health` to check system status.
```

**Do not:**
```
Requests are routed to the appropriate agent by the orchestrator.
System status can be checked by running `euxis-health`.
```

### Avoid Marketing Language

Do not use superlatives, marketing speak, or promotional language. State facts without embellishment.

**Do:**
```
Euxis supports parallel agent execution.
```

**Do not:**
```
Euxis provides best-in-class, blazingly fast parallel agent execution.
Experience the power of revolutionary agent orchestration.
```

### Emoji Usage

Do not use emojis in technical documentation. The only exception is user-facing CLI feedback where emojis improve readability (success indicators, warnings).

**Acceptable in CLI output:**
```
[OK] Health check passed
[!] Configuration warning
```

**Not acceptable in documentation:**
```
Run the installer to get started!
```

## Formatting Standards

### Heading Hierarchy

Use a consistent heading structure throughout all documents.

| Level | Usage | Example |
|-------|-------|---------|
| H1 (`#`) | Document title only. One per file. | `# Installation Guide` |
| H2 (`##`) | Major sections | `## Prerequisites` |
| H3 (`###`) | Subsections | `### System Requirements` |
| H4 (`####`) | Nested topics (use sparingly) | `#### Memory Configuration` |

Never skip heading levels. An H3 must follow an H2, not an H1.

### Code Blocks

Always specify the language for syntax highlighting. Use the most specific language identifier available.

**Do:**
````markdown
```bash
euxis-health --verbose
```

```python
def check_status():
    return orchestrator.health()
```

```json
{
  "version": "0.1.0",
  "status": "healthy"
}
```
````

**Do not:**
````markdown
```
euxis-health --verbose
```
````

For shell sessions showing both commands and output, use `console` or `bash` with clear visual separation:

````markdown
```console
$ euxis-health
Status: healthy
Agents: 3 active
Uptime: 2h 34m
```
````

### Inline Code

Use backticks for:
- Command names: `euxis-health`
- File paths: `/etc/euxis/config.yaml`
- Configuration keys: `max_retries`
- Variable names: `EUXIS_HOME`
- Short code references: `return None`

Do not use backticks for:
- Product names (Euxis, not `Euxis`)
- General technical terms (API, CLI, JSON)

### Tables

Use tables for structured reference information. Align columns for readability in source.

```markdown
| Parameter     | Type    | Default | Description                    |
|---------------|---------|---------|--------------------------------|
| `timeout`     | integer | 30      | Request timeout in seconds     |
| `retries`     | integer | 3       | Number of retry attempts       |
| `verbose`     | boolean | false   | Enable detailed logging        |
```

Keep tables simple. If a table exceeds 4-5 columns or requires complex formatting, consider using a definition list or subsections instead.

### Lists

**Use numbered lists for:**
- Sequential steps that must be followed in order
- Ranked items
- Items you will reference by number

```markdown
1. Install dependencies.
2. Configure the environment.
3. Run the initialization script.
```

**Use bullet lists for:**
- Unordered collections of items
- Feature lists
- Requirements where order does not matter

```markdown
- Python 3.10 or higher
- 4 GB available memory
- Network access to the API endpoint
```

**List formatting rules:**
- End each list item with a period if it is a complete sentence
- Use consistent capitalization (sentence case)
- Keep parallel structure across items

## Code Examples

### Show Command and Output

When demonstrating commands, show both the command and its expected output. This helps users verify they are on the right track.

```console
$ euxis-health --json
{
  "status": "healthy",
  "version": "0.1.0",
  "agents": {
    "orchestrator": "running",
    "analyzer": "running",
    "executor": "idle"
  },
  "uptime_seconds": 9240
}
```

### Use Realistic but Anonymized Data

Use plausible example data, but avoid real names, emails, or identifiable information.

**Do:**
```json
{
  "user": "jsmith",
  "email": "jsmith@example.com",
  "project": "acme-backend"
}
```

**Do not:**
```json
{
  "user": "foo",
  "email": "test@test.com",
  "project": "asdf"
}
```

### Include Error Handling

Show what happens when things go wrong. Users need to recognize and handle errors.

```python
from euxis import Orchestrator, ConnectionError

try:
    orch = Orchestrator.connect()
except ConnectionError as e:
    print(f"Failed to connect: {e}")
    # Check that the daemon is running
    # Verify EUXIS_HOST and EUXIS_PORT environment variables
```

```console
$ euxis-health
Error: Could not connect to orchestrator
Hint: Ensure the daemon is running with `euxis-daemon start`
```

### Comment Complex Logic

Add comments to explain non-obvious code. Focus on why, not what.

```python
# Retry with exponential backoff to handle transient network issues
for attempt in range(max_retries):
    try:
        return client.send(request)
    except TimeoutError:
        wait_time = 2 ** attempt  # 1s, 2s, 4s, 8s...
        time.sleep(wait_time)

# All retries exhausted; surface the error to the caller
raise ConnectionError("Failed after {max_retries} attempts")
```

## Terminology

### Product and Component Names

| Term | Correct | Incorrect |
|------|---------|-----------|
| Product name | Euxis | EUXIS, euxis, EuXis |
| Agent names | orchestrator, analyzer, executor | Orchestrator, ANALYZER |
| Command names | `euxis-health`, `euxis-daemon` | euxis-health (without backticks) |

### Capitalization Rules

- **Euxis**: Always capitalize the product name
- **Agent names**: Always lowercase when referring to specific agents (orchestrator, analyzer)
- **Configuration keys**: Always lowercase as they appear in config files
- **Environment variables**: Always uppercase (`EUXIS_HOME`)

### Technical Terms

Use consistent terminology throughout documentation.

| Preferred | Avoid |
|-----------|-------|
| configuration file | config file, conf file |
| command-line interface | CLI (acceptable after first use) |
| environment variable | env var |
| API endpoint | API URL, endpoint URL |

Spell out acronyms on first use:
```
The command-line interface (CLI) provides several subcommands. Use the CLI to...
```

## File Naming

### Documentation Files

- Use lowercase letters
- Separate words with hyphens
- Use descriptive names
- Use `.md` extension for Markdown

**Do:**
```
installation-guide.md
api-reference.md
troubleshooting-network-issues.md
```

**Do not:**
```
InstallationGuide.md
API_Reference.md
troubleshooting.md  (too vague)
```

### Directories

- Use lowercase letters
- Separate words with hyphens
- Use plural names for collections

```
docs/
  guides/
  api-reference/
  examples/
```

## Accessibility

### Alternative Text for Images

Every image must have descriptive alt text that conveys the same information as the image.

```markdown
![System architecture diagram showing the orchestrator connected to three agent processes](./images/architecture.png)
```

For decorative images that add no information, use empty alt text:
```markdown
![](./images/divider.png)
```

### Heading Structure

Maintain proper heading hierarchy to support screen readers and navigation.

- Never skip heading levels
- Use headings to create a logical document outline
- Do not use headings for visual styling

You can validate heading structure by extracting headings from a document. They should form a logical outline:

```
# Installation Guide
## Prerequisites
### System Requirements
### Network Requirements
## Installation Steps
### Download
### Configure
### Verify
```

### Color and Contrast

When creating diagrams or visual content:

- Do not rely on color alone to convey information
- Use labels, patterns, or icons in addition to color
- Ensure sufficient contrast between text and background
- Test with colorblind simulation tools

**Do:**
```
[SUCCESS - green] Operation completed
[FAILED - red] Operation failed
```

**Do not:**
```
[green circle] Operation completed
[red circle] Operation failed
```

### Link Text

Use descriptive link text that makes sense out of context.

**Do:**
```markdown
See the [installation guide](./installation.md) for setup instructions.
```

**Do not:**
```markdown
For setup instructions, [click here](./installation.md).
```

## Versioning

### Version-Specific Documentation

When documenting version-specific features, clearly indicate the version requirement.

```markdown
## Parallel Execution

*Available in Euxis 0.0.6 and later.*

The orchestrator supports parallel agent execution...
```

For configuration options:
```markdown
| Parameter | Type | Default | Since |
|-----------|------|---------|-------|
| `parallel` | boolean | false | 0.0.6 |
```

### Deprecation Notices

Mark deprecated features clearly with a deprecation notice. Include:
- The version when deprecation occurred
- The recommended alternative
- The version when removal is planned (if known)

```markdown
> **Deprecated in 0.1.0**
>
> The `--legacy` flag is deprecated and will be removed in version 0.0.1.
> Use `--compat-mode` instead.
```

For deprecated configuration:
```markdown
### `legacy_mode`

**Deprecated:** Use `compat_mode` instead. Will be removed in 0.1.0.
```

### Changelog References

When documenting changes, reference the changelog for complete details:

```markdown
See the [changelog](./CHANGELOG.md) for a complete list of changes in this version.
```

## Quick Reference

| Element | Convention | Example |
|---------|------------|---------|
| Product name | Capitalized | Euxis |
| Agent names | Lowercase | orchestrator |
| Commands | Backticks | `euxis-health` |
| File paths | Backticks | `/etc/euxis/config.yaml` |
| Config keys | Backticks, lowercase | `max_retries` |
| Env variables | Backticks, uppercase | `EUXIS_HOME` |
| Code blocks | Language specified | ````bash` |
| Headings | H1 title only | `# Title` for doc title |
| Lists | Periods for sentences | `- Install the package.` |
| File names | Lowercase, hyphens | `api-reference.md` |
