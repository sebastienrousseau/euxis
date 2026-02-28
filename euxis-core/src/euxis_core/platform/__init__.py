# SPDX-License-Identifier: AGPL-3.0-or-later

"""Platform-specific adapters for core ports."""

from .adapters import LocalFilesystemAdapter, RuntimePlatformAdapter

__all__ = ["LocalFilesystemAdapter", "RuntimePlatformAdapter"]
