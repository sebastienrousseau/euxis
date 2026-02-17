euxis-gateway
=============

HTTP/WebSocket gateway server for the Euxis Fleet.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The ``euxis-gateway`` package provides HTTP and WebSocket endpoints for
external integrations with the Euxis Fleet system.

Features
--------

- RESTful API endpoints
- WebSocket real-time updates
- Session import/export
- Web chat interface

Modules
-------

gateway.server
^^^^^^^^^^^^^^

Main server implementation.

.. automodule:: gateway.server
   :members:
   :undoc-members:
   :show-inheritance:

gateway.session_export
^^^^^^^^^^^^^^^^^^^^^^

Session export functionality.

.. automodule:: gateway.session_export
   :members:
   :undoc-members:
   :show-inheritance:

gateway.session_import
^^^^^^^^^^^^^^^^^^^^^^

Session import functionality.

.. automodule:: gateway.session_import
   :members:
   :undoc-members:
   :show-inheritance:
