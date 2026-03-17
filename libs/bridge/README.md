# Euxis Bridge C++

The `euxis::bridge` module securely connects the core C++23 orchestrator to external Agent capabilities and execution bounds. It serves as the primary ingress for foreign skills and sandboxed binaries.

## The Skill Importer

The `SkillImporter` ingests OpenClaw-compatible `SKILL.md` configurations.

* **Precondition**: The skill file must pass initial JSON-Schema structure checks.
* **Postcondition**: Synthesizes the logic into a typed `BridgedSkill` definition.

For safety, the module uses static analysis to strip unverified markdown. Extract dependencies strictly, ensuring no malicious shell directives leak into the parser.

## Verification & Admission

The `AdmissionController` enforces hardware-native execution bounds on all imported logic.

* **Type Erasure**: Hiding concrete implementations — Unified interface for disparate skill schemas.
* **RAII**: Resource-bound lifetime management.

```cpp
auto res = admission.evaluate_skill(bridged_skill)
    .and_then([](auto&& validated_skill) { return enqueue_for_execution(validated_skill); })
    .or_else([](auto&& err) { return reject_skill(err); });
```

Utilize C++23 monadic chains to guarantee `BridgedSkill` isolation. The controller invokes the `Platform` abstraction to verify that host-level eBPF or micro-kernel sandboxes are active prior to yielding execution.

## Sandboxed Execution

The `SkillExecutor` runs admitted skills securely.

* **UB**: Undefined Behavior — Avoid unchecked memory bounds.

The executor never grants direct filesystem access. Utilize `std::expected` to capture and surface sandbox violations or runtime panics back to the orchestrator layer without halting the primary thread.