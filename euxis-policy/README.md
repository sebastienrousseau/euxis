# Security Policy Defaults

## Purpose

This directory holds top-level, repository-shared security policy defaults used by
Gateway and related packages.

## Why This Is Not `euxis-*`

`euxis-policy/` is a workspace-level policy asset directory, not a standalone package.
Package code remains under `euxis-cpp/euxis-security-cpp/`.

## Current Contents

- `gateway.json`: default gateway approval/allowlist policy seed.

## Multi-Repo Portability

During repo-split, these files should move into the dedicated policy/config repo
or into `euxis-security` release assets with versioned publishing.
