euxis-core
==========

Core libraries for the Euxis Fleet framework.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The ``euxis-core`` package provides the foundational libraries for agent
coordination, provider routing, session management, and template processing.

Shell Libraries
---------------

The core shell libraries are located in ``euxis-bin/lib/``:

- ``agents.sh`` - Agent registry and fleet management
- ``common.sh`` - Shared utilities and logging
- ``providers.sh`` - Provider routing and execution
- ``session.sh`` - Session lifecycle management
- ``template.sh`` - Prompt template processing
- ``prompt.sh`` - Prompt construction and caching
- ``memory.sh`` - Memory and context management
- ``registry_sql.sh`` - SQLite-based registry operations
- ``validation.sh`` - Input validation and sanitization
- ``cli.sh`` - CLI utilities and argument parsing
- ``dispatch.sh`` - Agent dispatch coordination

Configuration
-------------

Agent definitions are stored in ``euxis-data/agents/``:

- ``registry.json`` - Agent registry with metadata
- ``registry.db`` - SQLite database for fast lookups
- ``squads.json`` - Squad composition definitions
- ``prompts/`` - Agent prompt templates

Python Modules
--------------

.. note::
   The euxis-core package primarily consists of shell scripts.
   Python modules are used for testing and utilities.
