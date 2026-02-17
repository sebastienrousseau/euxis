# Extraction Checklist (cli)

- [x] Verify `pyproject.toml` has correct name/version/deps
- [ ] Confirm `src/` package imports are relative and public
- [ ] Ensure `tests/` pass with `pytest -q` from module root
- [ ] Replace any hard-coded repo paths with module-local paths
- [x] Document public API in `README.md` and `API_SUMMARY.md`
- [x] Add CI workflow from `ci/module-ci.yml`
- [ ] Ensure shared utilities are imported from `packages/shared` only
- [ ] Validate `MODULE_BOUNDARIES.md` compliance
- [ ] Add release notes or migration guide if extracted