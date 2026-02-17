euxis-crypto-lib
================

Cryptographic services library for the Euxis Fleet.

.. contents:: Table of Contents
   :local:
   :depth: 2

Overview
--------

The ``euxis-crypto-lib`` package provides encryption, decryption, key management,
and cryptographic utilities using AES-256-GCM.

Features
--------

- AES-256-GCM authenticated encryption
- Secure key generation and derivation (PBKDF2)
- Result type pattern for error handling
- Comprehensive exception hierarchy

Modules
-------

crypto_lib
^^^^^^^^^^

Main package exports.

.. automodule:: crypto_lib
   :members:
   :undoc-members:
   :show-inheritance:

crypto_lib.core
^^^^^^^^^^^^^^^

Core encryption/decryption functions.

.. automodule:: crypto_lib.core
   :members:
   :undoc-members:
   :show-inheritance:

crypto_lib.exceptions
^^^^^^^^^^^^^^^^^^^^^

Exception classes and error handling.

.. automodule:: crypto_lib.exceptions
   :members:
   :undoc-members:
   :show-inheritance:

crypto_lib.key_management
^^^^^^^^^^^^^^^^^^^^^^^^^

Key generation and validation.

.. automodule:: crypto_lib.key_management
   :members:
   :undoc-members:
   :show-inheritance:
