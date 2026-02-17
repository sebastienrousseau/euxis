# Repository Structure Strategy

This document captures the target split for functional repositories to reduce coupling and simplify releases.

## Target Repos

- `euxis-frontend`
- `euxis-backend`
- `euxis-docs`
- `euxis-scripts`
- `euxis-assets`

## Migration Notes

- Keep shared conventions in `.github/` and `config/templates/` and sync across repos.
- Extract one domain at a time to avoid large, breaking migrations.
- Publish versioned release notes for each repo as the split progresses.
