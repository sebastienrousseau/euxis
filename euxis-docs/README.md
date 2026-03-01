# euxis-docs

Documentation for the Euxis Fleet framework, built with Sphinx.

## Verification

Run the strict package gate:

```bash
bash ../euxis-ops/quality/run_docs_tests_stable.sh
```

Run tests directly from package root:

```bash
pytest -q -c pyproject.toml tests
```

## Building Documentation

## Publishing to GitHub Pages

Docs are built by CI and published to the `gh-pages` branch on pushes to `main` via `.github/workflows/docs-gh-pages.yml`.

To publish locally (manual):

```bash
make -C euxis-docs html
```

Then push `_build/html` to `gh-pages` if needed.


Docs are published to the `gh-pages` branch by CI on push to `main`.


### Prerequisites

```bash
pip install -e ".[dev]"
```

### Build HTML

```bash
make html
```

The output will be in `_build/html/`.

### Live Preview

```bash
make livehtml
```

This starts a live-reload server for development.

## Structure

- `docs/` - Source documentation (Markdown and reStructuredText)
- `docs/api/` - Auto-generated API reference
- `docs/adr/` - Architecture Decision Records
- `docs/guides/` - User and developer guides
- `docs/reference/` - Reference documentation
- `_build/html/` - Built HTML output

## API Documentation

API documentation is auto-generated from docstrings using Sphinx autodoc.
Each package has its own API page in `docs/api/`.

## License

AGPL-3.0-or-later - see [LICENSE](LICENSE)

---
*Part of the [Euxis Agent Framework](https://euxis.co)*
