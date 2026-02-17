euxis-tui
=========

Terminal User Interface for the Euxis Fleet.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The ``euxis-tui`` package provides a rich terminal interface built with
the Textual framework for monitoring and controlling the fleet.

Features
--------

- Real-time fleet monitoring
- Agent status dashboards
- Playbook execution
- Cortex memory visualization

Architecture
------------

The TUI is built using the Textual framework with a modular screen-based
architecture:

- **App**: Main application entry point (``tui.app.EuxisApp``)
- **Screens**: Individual views (dashboard, fleet, cortex, etc.)
- **Widgets**: Reusable UI components (agent cards, sparklines, etc.)
- **Services**: Backend services and dependency injection

Modules
-------

tui.core.services
^^^^^^^^^^^^^^^^^

Service classes and dependency injection.

.. automodule:: tui.core.services
   :members:
   :undoc-members:
   :show-inheritance:

tui.core.config
^^^^^^^^^^^^^^^

Configuration management.

.. automodule:: tui.core.config
   :members:
   :undoc-members:
   :show-inheritance:

Usage
-----

Start the TUI with::

    euxis-tui

Or programmatically::

    from tui.app import EuxisApp
    app = EuxisApp()
    app.run()
