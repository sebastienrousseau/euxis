# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Euxis Core - Shared core logic and shell libraries."""

from euxis_core.encrypted_memory import EncryptedMemoryEntry, EncryptedMemoryStore

__version__ = "v0.0.3"
__all__ = ["EncryptedMemoryEntry", "EncryptedMemoryStore", "__version__"]
