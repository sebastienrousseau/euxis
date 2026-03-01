# Archive Cleanup Report (v0.0.3)

## Summary
Safe archival of duplicate top-level guides to reduce confusion while preserving history. Redirect stubs remain at original paths to satisfy certification checks and preserve inbound links.

## Archived Items
- `docs/archive/user-guide.md`
  - Source: `docs/user-guide.md`
  - Reason: duplicate of `docs/guides/user-guide.md`
  - Action: moved to archive; added redirect stub at original path

- `docs/archive/fleet-guide.md`
  - Source: `docs/fleet-guide.md`
  - Reason: duplicate of `docs/guides/fleet-guide.md`
  - Action: moved to archive; added redirect stub at original path

## Redirect Stubs
- `docs/user-guide.md` → `docs/guides/user-guide.md`
- `docs/fleet-guide.md` → `docs/guides/fleet-guide.md`

## Link Updates
Updated internal references to point to `docs/guides/*` paths.

## Certification
`euxis-cli/bin/euxis-certify` passed after archival and redirects.
