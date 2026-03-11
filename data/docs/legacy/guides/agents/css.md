# CSS & HTML Playbook

Euxis natively understands modern web layout engines, CSS Grid mechanics, and semantic component isolation. Use this playbook to engineer responsive, accessible user interfaces without manually writing boilerplate.

## Toolchain Integration

Euxis operates directly on your existing frontend infrastructure. It automatically detects and adheres to the rules defined by your tooling.

* **TailwindCSS / PostCSS:** Utility-first and modular generation.
* **Stylelint:** Strict CSS syntax and pattern enforcement.
* **HTMLHint:** HTML5 semantic validation and accessibility checking.

## Execute the Playbook

Generate or refactor stylistic components from the command line.

```bash
euxis playbook run css "Create a responsive, accessible navigation bar."
```

Euxis automatically runs the resulting code against Stylelint before returning the artifact.
