# Configuration file for the Sphinx documentation builder.
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import sys
from datetime import datetime

# Add package paths for autodoc
# Each package has src/<module_name>/ structure
repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

package_paths = [
    "euxis-core/src",
    "euxis-cli/src",
    "euxis-cli/bin",  # For euxis_core.py
    "euxis-metrics/src",
    "euxis-metrics/src/metrics",  # For aggregators, verification submodules
    "euxis-crypto-lib/src",
    "euxis-tui/src",
    "euxis-gateway/src",
    "euxis-adapters/src",
    "euxis-security/src",
]

for pkg_path in package_paths:
    full_path = os.path.join(repo_root, pkg_path)
    if os.path.exists(full_path):
        sys.path.insert(0, full_path)

# -- Project information -----------------------------------------------------

project = "Euxis"
copyright = f"{datetime.now().year}, Euxis Fleet"
author = "Euxis Fleet"
version = "0.0.1"
release = "0.1.0"

# -- General configuration ---------------------------------------------------

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.viewcode",
    "sphinx.ext.napoleon",
    "sphinx.ext.todo",
    "myst_parser",
]

# MyST parser settings (for Markdown support)
myst_enable_extensions = [
    "colon_fence",
    "deflist",
    "tasklist",
    "fieldlist",
    "attrs_inline",
]
myst_heading_anchors = 3
myst_highlight_code_blocks = False
myst_suppress_warnings = ["xref_missing"]

# Suppress warnings for missing cross-references in markdown
myst_url_schemes = ["http", "https", "mailto"]
suppress_warnings = [
    "myst.xref_missing",
    "toc.not_included",
    "autodoc.import_object",
    "autodoc",
]

# Source file suffixes
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}

# The master document
master_doc = "index"

# Patterns to exclude
exclude_patterns = [
    "_build",
    "Thumbs.db",
    ".DS_Store",
    "**/.git",
    "mkdocs.yml",
    "**/README.md",
    "**/*.orig",
]

# Templates path
templates_path = ["_templates"]

# -- Options for HTML output -------------------------------------------------

html_theme = "furo"
html_title = "Euxis Documentation"
html_static_path = ["_static"]

# Theme options
html_theme_options = {
    "sidebar_hide_name": False,
    "navigation_with_keys": True,
}

# -- Options for autodoc -----------------------------------------------------

autodoc_default_options = {
    "members": True,
    "member-order": "bysource",
    "special-members": "__init__",
    "undoc-members": True,
    "exclude-members": "__weakref__",
}

autodoc_typehints = "description"
autodoc_class_signature = "separated"

# Mock imports for modules that may not be available
autodoc_mock_imports = [
    "chromadb",
    "sentence_transformers",
    "textual",
    "rich",
    "httpx",
    "fastapi",
    "uvicorn",
    "slack_sdk",
    "telegram",
    "cryptography",
    "shared",
    "shared.crypto_bridge",
]

# -- Options for autosummary -------------------------------------------------

autosummary_generate = True

# -- Options for Napoleon (Google/NumPy docstrings) --------------------------

napoleon_google_docstring = True
napoleon_numpy_docstring = True
napoleon_include_init_with_doc = True
napoleon_include_private_with_doc = False

# -- Options for todo extension ----------------------------------------------

todo_include_todos = True
