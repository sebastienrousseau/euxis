euxis-adapters
==============

Platform adapters for external integrations.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The ``euxis-adapters`` package provides adapters for integrating the Euxis
Fleet with external platforms like Slack and Telegram.

Features
--------

- Slack workspace integration
- Telegram bot integration
- Adapter registry for plugin management
- Message routing and formatting

Modules
-------

adapters.registry
^^^^^^^^^^^^^^^^^

Adapter registration and discovery.

.. automodule:: adapters.registry
   :members:
   :undoc-members:
   :show-inheritance:

adapters.slack_adapter
^^^^^^^^^^^^^^^^^^^^^^

Slack integration adapter.

.. automodule:: adapters.slack_adapter
   :members:
   :undoc-members:
   :show-inheritance:

adapters.telegram_adapter
^^^^^^^^^^^^^^^^^^^^^^^^^

Telegram bot adapter.

.. automodule:: adapters.telegram_adapter
   :members:
   :undoc-members:
   :show-inheritance:
