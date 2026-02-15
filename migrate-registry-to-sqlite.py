#!/usr/bin/env python3
"""Migrate Euxis registry.json into registry.db (SQLite).

This script is intentionally lightweight and self-contained to support
CLI usage and unit tests in tests/unit/test_migration_scripts.py.
"""

from __future__ import annotations

import argparse
import json
import sqlite3
import sys
from datetime import UTC, datetime
from pathlib import Path

REGISTRY_FILE = Path.home() / ".euxis" / "registry.json"
DB_FILE = Path.home() / ".euxis" / "registry.db"
BACKUP_DIR = Path.home() / ".euxis" / "backups"


def _fail(msg: str) -> None:
    """Exit with an error message."""
    sys.stderr.write(f"❌ {msg}\n")
    raise SystemExit(1)


def create_backup() -> Path:
    """Write a timestamped backup of the registry JSON."""
    if not REGISTRY_FILE.exists():
        _fail(f"Registry file not found: {REGISTRY_FILE}")

    BACKUP_DIR.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(tz=UTC).strftime("%Y%m%d-%H%M%S")
    backup_file = BACKUP_DIR / f"registry-{stamp}.json"
    backup_file.write_text(REGISTRY_FILE.read_text())
    return backup_file


def create_schema(conn: sqlite3.Connection) -> None:
    """Create required registry tables and indexes."""
    cursor = conn.cursor()
    cursor.execute("PRAGMA foreign_keys = ON")

    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS registry_metadata (
            key TEXT PRIMARY KEY,
            value TEXT
        )
        """
    )
    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS agents (
            id TEXT PRIMARY KEY,
            path TEXT,
            tier TEXT,
            version TEXT,
            activation TEXT,
            created_at TEXT,
            updated_at TEXT
        )
        """
    )
    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE
        )
        """
    )
    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS capability_tags (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT UNIQUE
        )
        """
    )
    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS agent_tags (
            agent_id TEXT,
            tag_id INTEGER,
            PRIMARY KEY (agent_id, tag_id),
            FOREIGN KEY (agent_id) REFERENCES agents(id) ON DELETE CASCADE,
            FOREIGN KEY (tag_id) REFERENCES tags(id) ON DELETE CASCADE
        )
        """
    )
    cursor.execute(
        """
        CREATE TABLE IF NOT EXISTS agent_capabilities (
            agent_id TEXT,
            capability_id INTEGER,
            PRIMARY KEY (agent_id, capability_id),
            FOREIGN KEY (agent_id) REFERENCES agents(id) ON DELETE CASCADE,
            FOREIGN KEY (capability_id) REFERENCES capability_tags(id) ON DELETE CASCADE
        )
        """
    )

    # Indexes
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_agents_tier ON agents(tier)")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_agents_version ON agents(version)")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_agents_activation ON agents(activation)")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_agent_tags_agent ON agent_tags(agent_id)")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_agent_tags_tag ON agent_tags(tag_id)")
    cursor.execute(
        "CREATE INDEX IF NOT EXISTS idx_agent_capabilities_agent ON agent_capabilities(agent_id)"
    )
    cursor.execute(
        "CREATE INDEX IF NOT EXISTS idx_agent_capabilities_capability "
        "ON agent_capabilities(capability_id)"
    )
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_tags_name ON tags(name)")
    cursor.execute("CREATE INDEX IF NOT EXISTS idx_capability_tags_name ON capability_tags(name)")

    conn.commit()


def insert_tag(cursor: sqlite3.Cursor, name: str, table: str) -> int:
    """Insert a tag into a known table and return its ID."""
    if table == "tags":
        select_sql = "SELECT id FROM tags WHERE name = ?"
        insert_sql = "INSERT INTO tags (name) VALUES (?)"
    elif table == "capability_tags":
        select_sql = "SELECT id FROM capability_tags WHERE name = ?"
        insert_sql = "INSERT INTO capability_tags (name) VALUES (?)"
    else:
        _fail(f"Unknown tag table: {table}")

    cursor.execute(select_sql, (name,))
    row = cursor.fetchone()
    if row:
        return int(row[0])
    cursor.execute(insert_sql, (name,))
    return int(cursor.lastrowid)


def migrate_data(conn: sqlite3.Connection, registry_data: dict, dry_run: bool = False) -> None:
    """Load registry data into the SQLite database."""
    if dry_run:
        return

    cursor = conn.cursor()

    # Metadata
    for key in (
        "schema_version",
        "protocol_version",
        "last_updated",
        "capabilities_registry",
        "constitution",
    ):
        if key in registry_data:
            cursor.execute(
                "INSERT OR REPLACE INTO registry_metadata (key, value) VALUES (?, ?)",
                (key, str(registry_data.get(key))),
            )

    agents = registry_data.get("agents", []) or []
    for agent in agents:
        cursor.execute(
            """
            INSERT OR REPLACE INTO agents (
                id,
                path,
                tier,
                version,
                activation,
                created_at,
                updated_at
            )
            VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (
                agent.get("id"),
                agent.get("path"),
                agent.get("tier"),
                agent.get("version"),
                agent.get("activation"),
                agent.get("created_at"),
                agent.get("updated_at"),
            ),
        )

        for tag in agent.get("tags", []) or []:
            tag_id = insert_tag(cursor, tag, "tags")
            cursor.execute(
                "INSERT OR IGNORE INTO agent_tags (agent_id, tag_id) VALUES (?, ?)",
                (agent.get("id"), tag_id),
            )

        for cap in agent.get("capability_tags", []) or []:
            cap_id = insert_tag(cursor, cap, "capability_tags")
            cursor.execute(
                "INSERT OR IGNORE INTO agent_capabilities (agent_id, capability_id) VALUES (?, ?)",
                (agent.get("id"), cap_id),
            )

    conn.commit()


def create_query_views(conn: sqlite3.Connection) -> None:
    """Create read-friendly views for tags and capabilities."""
    cursor = conn.cursor()

    cursor.execute(
        """
        CREATE VIEW IF NOT EXISTS agent_tags_view AS
        SELECT a.id AS id,
               GROUP_CONCAT(t.name, ',') AS tags
        FROM agents a
        LEFT JOIN agent_tags at ON at.agent_id = a.id
        LEFT JOIN tags t ON t.id = at.tag_id
        GROUP BY a.id
        """
    )

    cursor.execute(
        """
        CREATE VIEW IF NOT EXISTS agent_capabilities_view AS
        SELECT a.id AS id,
               GROUP_CONCAT(c.name, ',') AS capabilities
        FROM agents a
        LEFT JOIN agent_capabilities ac ON ac.agent_id = a.id
        LEFT JOIN capability_tags c ON c.id = ac.capability_id
        GROUP BY a.id
        """
    )

    cursor.execute(
        """
        CREATE VIEW IF NOT EXISTS agents_complete AS
        SELECT a.id,
               a.path,
               a.tier,
               a.version,
               a.activation,
               atv.tags AS tags,
               acv.capabilities AS capability_tags
        FROM agents a
        LEFT JOIN agent_tags_view atv ON atv.id = a.id
        LEFT JOIN agent_capabilities_view acv ON acv.id = a.id
        """
    )

    conn.commit()


def verify_migration(conn: sqlite3.Connection, registry_data: dict) -> bool:
    """Confirm the database matches registry entries."""
    agents = registry_data.get("agents", []) or []
    expected_ids = {a.get("id") for a in agents}

    cursor = conn.cursor()
    cursor.execute("SELECT COUNT(*) FROM agents")
    db_count = cursor.fetchone()[0]
    if db_count != len(expected_ids):
        return False

    cursor.execute("SELECT id FROM agents")
    db_ids = {row[0] for row in cursor.fetchall()}
    return expected_ids.issubset(db_ids)


def _parse_args(argv: list[str]) -> argparse.Namespace:
    """Parse CLI arguments."""
    parser = argparse.ArgumentParser(description="Migrate Euxis registry JSON to SQLite.")
    parser.add_argument(
        "--backup",
        action="store_true",
        help="Create backup of registry.json before migrating.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Validate migration without writing a database.",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite existing database without prompt.",
    )
    return parser.parse_args(argv)


def main() -> None:
    """Run the migration workflow."""
    args = _parse_args(sys.argv[1:])

    if not REGISTRY_FILE.exists():
        _fail(f"Registry file not found: {REGISTRY_FILE}")

    if args.backup:
        create_backup()

    if args.dry_run:
        with sqlite3.connect(":memory:") as conn:
            create_schema(conn)
            registry_data = json.loads(REGISTRY_FILE.read_text())
            migrate_data(conn, registry_data, dry_run=True)
            create_query_views(conn)
            verify_migration(conn, registry_data)
        return

    if DB_FILE.exists() and not args.force:
        confirm = input(f"Database already exists at {DB_FILE}. Overwrite? [y/N] ").strip().lower()
        if confirm != "y":
            _fail("Migration cancelled by user.")

    try:
        if DB_FILE.exists():
            DB_FILE.unlink()
        DB_FILE.parent.mkdir(parents=True, exist_ok=True)
        registry_data = json.loads(REGISTRY_FILE.read_text())
        with sqlite3.connect(DB_FILE) as conn:
            create_schema(conn)
            migrate_data(conn, registry_data)
            create_query_views(conn)
            if not verify_migration(conn, registry_data):
                _fail("Migration verification failed.")
    except (OSError, sqlite3.Error, json.JSONDecodeError, ValueError) as exc:  # pragma: no cover
        if DB_FILE.exists():
            DB_FILE.unlink()
        _fail(f"Migration failed: {exc}")


if __name__ == "__main__":
    main()
