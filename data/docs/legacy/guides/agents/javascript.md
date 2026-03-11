# JavaScript & TypeScript Playbook

The Euxis JS/TS playbook mandates strict typing, modern web environments, and absolute performance. Use this playbook to scaffold robust frontend applications or high-throughput Node/Deno/Bun servers.

## Toolchain Integration

Euxis natively understands and manipulates modern JavaScript infrastructure. 

* **TypeScript Compiler (`tsc`):** Enforce strict type checking across the execution graph.
* **ESLint & Prettier:** Standardize styling and catch syntax errors.
* **Vitest & Jest:** Execute robust test runners.
* **Runtime Isolation:** Execute code dynamically via Node, Deno, or Bun environments.

## Execute the Playbook

Generate or refactor JavaScript/TypeScript components from the command line.

```bash
euxis playbook run javascript "Implement a robust debounce utility function with generic type signatures."
```

If the TypeScript compiler throws errors, the agent squad resolves them autonomously before returning the compiled module.
