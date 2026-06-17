euxis-cli
=========

Command-line interface tools for the Euxis Fleet framework.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The ``euxis-cli`` package provides all CLI commands for interacting with the
Euxis Fleet system.

Main Entry Point
----------------

The ``euxis`` command is the primary entry point::

    euxis <command> [args...]
    euxis <agent> <task> [provider]

Commands
--------

Fleet Operations
^^^^^^^^^^^^^^^^

- ``euxis dispatch <manifest>`` - Deploy fleet from architect manifest
- ``euxis verify`` - Run pre-commit quality gate
- ``euxis health`` - System health check

Squad & Playbook
^^^^^^^^^^^^^^^^

- ``euxis squad <cmd>`` - Squad management
- ``euxis playbook <cmd>`` - Playbook execution
- ``euxis combo <cmd>`` - Sequential agent chains

Memory & Bus
^^^^^^^^^^^^

- ``euxis cortex <cmd>`` - Vector memory operations
- ``euxis bus <cmd>`` - Message bus operations
- ``euxis graph <cmd>`` - Knowledge graph operations

Modules
-------

cli (dispatch engine)
^^^^^^^^^^^^^^^^^^^^^

Core dispatch engine for agent coordination.

.. automodule:: cli
   :members:
   :undoc-members:
   :show-inheritance:
   :noindex:
