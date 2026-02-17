# euxis-docs

Documentation for the Euxis Fleet framework, built with Sphinx.

## Building Documentation

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

MIT License - see [LICENSE](LICENSE)

---
*Part of the [Euxis Agent Framework](https://euxis.co)*
