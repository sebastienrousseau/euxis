# Euxis Plugin System

The Euxis Plugin System allows you to create and register custom agents to extend the core 53-agent ecosystem. This allows teams to create bespoke agents with specialized playbooks tailored to their specific domains.

## Creating a Plugin Manifest

A custom agent requires a JSON manifest. The manifest defines the agent's identity, description, capabilities, and the prompt path. 

Create a file named `my-agent.json` with the following structure:

```json
{
  "agent_id": "my-agent",
  "name": "My Custom Agent",
  "description": "Performs specialized tasks in my domain.",
  "tier": 2,
  "default_provider": "claude",
  "capabilities": ["code_generation", "analysis"],
  "prompt_file": "/absolute/path/to/my-agent-prompt.txt",
  "author": "My Team",
  "version": "1.0.0"
}
```

The `prompt_file` must point to an existing text file containing the system instructions for the agent.

## Registering a Plugin

Once the manifest and prompt file are ready, you can register the agent using the Euxis CLI:

```bash
euxis agent register /path/to/my-agent.json
```

This commands reads the manifest, copies the metadata into the plugin directory (`~/.euxis/data/config/plugins/`), and activates the agent in your local installation.

## Managing Plugins

To see all registered third-party plugins:

```bash
euxis agent list
```

To unregister a plugin that is no longer needed:

```bash
euxis agent unregister my-agent
```

## Using Your Custom Agent

After registration, your custom agent becomes a first-class citizen in the Euxis ecosystem. You can dispatch it exactly like any core agent:

```bash
euxis my-agent "Analyze this domain specific task"
euxis squad deploy my-squad "Use my-agent to fix this issue"
```
