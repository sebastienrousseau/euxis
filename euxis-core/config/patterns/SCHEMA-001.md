# SCHEMA-001: Data Schema Evolution

## Pattern
Ensure backward-compatible, validated, and safely migrated data schemas across all storage and API layers.

## Severity
HIGH

## Detection Rules
1. **Breaking field removal:** Removing or renaming a field in a schema consumed by other services without a deprecation period — mark fields deprecated first, remove after consumers migrate
2. **Type changes without migration:** Changing a field's type (string→int, array→object) without a migration script — every type change requires a data migration
3. **Missing schema validation:** Accepting unvalidated data at ingestion points — all data entry points must validate against a schema (JSON Schema, protobuf, Avro)
4. **Non-nullable additions:** Adding required (non-nullable) fields to existing schemas — new fields must be nullable or have defaults to avoid breaking existing producers
5. **No migration rollback:** Schema migrations that cannot be reversed — every migration must have an explicit rollback path or be proven forward-only safe

## Validation
- [ ] Schema changes go through compatibility check (backward, forward, or full)
- [ ] All data ingestion points validate against a declared schema
- [ ] New required fields have default values or are nullable
- [ ] Migration scripts include rollback procedure
- [ ] Schema registry or version catalog tracks all active schema versions

## Remediation
- Use a schema registry (Confluent, Apicurio) for event-driven systems
- Apply the expand-contract pattern: add new field → migrate data → remove old field
- Generate migration scripts automatically from schema diffs where possible
- Test migrations against production-like data volumes before deploying
- Version schemas independently from application code (schema-first design)
