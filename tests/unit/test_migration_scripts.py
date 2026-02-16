"""Comprehensive unit tests for migration scripts.

Tests cover:
- Schema creation and validation
- Data migration from JSON to SQLite
- Backup functionality
- Error handling and validation
- Query views and indexing
- Migration integrity verification
"""

from __future__ import annotations

# Import the migration script (filename uses dashes, so importlib is required)
import importlib.util
import json
import shutil
import sqlite3
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

import pytest

_spec = importlib.util.spec_from_file_location(
    "migrate_registry_to_sqlite",
    Path(__file__).parent.parent.parent / "migrate-registry-to-sqlite.py",
)
migrate_registry_to_sqlite = importlib.util.module_from_spec(_spec)
sys.modules["migrate_registry_to_sqlite"] = migrate_registry_to_sqlite
_spec.loader.exec_module(migrate_registry_to_sqlite)


class TestBackupFunctionality(unittest.TestCase):
    """Test backup creation functionality."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        # Mock the global paths
        self.original_registry_file = migrate_registry_to_sqlite.REGISTRY_FILE
        self.original_backup_dir = migrate_registry_to_sqlite.BACKUP_DIR
        migrate_registry_to_sqlite.REGISTRY_FILE = self.temp_path / "agents/registry.json"
        migrate_registry_to_sqlite.BACKUP_DIR = self.temp_path / "backups"

    def tearDown(self):
        migrate_registry_to_sqlite.REGISTRY_FILE = self.original_registry_file
        migrate_registry_to_sqlite.BACKUP_DIR = self.original_backup_dir
        shutil.rmtree(self.temp_dir)

    def test_create_backup_success(self):
        """Test successful backup creation."""
        # Create test registry file
        registry_data = {"agents": [{"id": "test-agent", "tier": "core"}]}
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        backup_file = migrate_registry_to_sqlite.create_backup()

        assert backup_file.exists()
        assert "registry-" in backup_file.name
        assert backup_file.name.endswith(".json")
        assert migrate_registry_to_sqlite.BACKUP_DIR.exists()

        # Verify backup content
        backup_content = json.loads(backup_file.read_text())
        assert backup_content == registry_data

    def test_create_backup_missing_registry(self):
        """Test backup when registry file doesn't exist."""
        with pytest.raises(SystemExit):
            migrate_registry_to_sqlite.create_backup()

    def test_create_backup_directory_creation(self):
        """Test that backup directory is created if it doesn't exist."""
        registry_data = {"agents": []}
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        # Ensure backup directory doesn't exist
        assert not migrate_registry_to_sqlite.BACKUP_DIR.exists()

        backup_file = migrate_registry_to_sqlite.create_backup()

        assert migrate_registry_to_sqlite.BACKUP_DIR.exists()
        assert backup_file.exists()

    @patch("migrate_registry_to_sqlite.datetime")
    def test_create_backup_timestamp_format(self, mock_datetime):
        """Test backup filename timestamp format."""
        mock_datetime.now.return_value.strftime.return_value = "20260101-120000"

        registry_data = {"agents": []}
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        backup_file = migrate_registry_to_sqlite.create_backup()

        assert "registry-20260101-120000.json" in str(backup_file)


class TestSchemaCreation(unittest.TestCase):
    """Test SQLite schema creation."""

    def setUp(self):
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as temp_db:
            self.db_path = Path(temp_db.name)

    def tearDown(self):
        if self.db_path.exists():
            self.db_path.unlink()

    def test_create_schema_all_tables(self):
        """Test that all required tables are created."""
        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)

            cursor = conn.cursor()
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
            tables = [row[0] for row in cursor.fetchall() if not row[0].startswith("sqlite_")]

            expected_tables = {
                "registry_metadata",
                "agents",
                "tags",
                "capability_tags",
                "agent_tags",
                "agent_capabilities"
            }
            assert set(tables) == expected_tables

    def test_create_schema_table_structures(self):
        """Test specific table structures."""
        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)

            cursor = conn.cursor()

            # Test agents table structure
            cursor.execute("PRAGMA table_info(agents)")
            agents_columns = {row[1]: row[2] for row in cursor.fetchall()}
            expected_agent_columns = {
                "id": "TEXT",
                "path": "TEXT",
                "tier": "TEXT",
                "version": "TEXT",
                "activation": "TEXT",
                "created_at": "TEXT",
                "updated_at": "TEXT"
            }
            assert agents_columns == expected_agent_columns

            # Test foreign key constraints
            cursor.execute("PRAGMA foreign_key_list(agent_tags)")
            fk_constraints = cursor.fetchall()
            assert len(fk_constraints) == 2  # Should reference agents and tags

    def test_create_schema_indexes(self):
        """Test that all indexes are created."""
        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)

            cursor = conn.cursor()
            cursor.execute("SELECT name FROM sqlite_master WHERE type='index'")
            indexes = [row[0] for row in cursor.fetchall() if not row[0].startswith("sqlite_")]

            expected_indexes = {
                "idx_agents_tier",
                "idx_agents_version",
                "idx_agents_activation",
                "idx_agent_tags_agent",
                "idx_agent_tags_tag",
                "idx_agent_capabilities_agent",
                "idx_agent_capabilities_capability",
                "idx_tags_name",
                "idx_capability_tags_name"
            }
            assert set(indexes) == expected_indexes

    def test_create_schema_idempotent(self):
        """Test that schema creation is idempotent."""
        with sqlite3.connect(self.db_path) as conn:
            # Create schema twice
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.create_schema(conn)

            # Should not raise error and tables should exist
            cursor = conn.cursor()
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table'")
            tables = [row[0] for row in cursor.fetchall()]
            assert "agents" in tables


class TestTagInsertion(unittest.TestCase):
    """Test tag insertion helper function."""

    def setUp(self):
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as temp_db:
            self.db_path = Path(temp_db.name)

    def tearDown(self):
        if self.db_path.exists():
            self.db_path.unlink()

    def test_insert_tag_new_tag(self):
        """Test inserting a new tag."""
        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            cursor = conn.cursor()

            tag_id = migrate_registry_to_sqlite.insert_tag(cursor, "test-tag", "tags")
            assert tag_id is not None
            assert isinstance(tag_id, int)

            # Verify tag exists
            cursor.execute("SELECT name FROM tags WHERE id = ?", (tag_id,))
            result = cursor.fetchone()
            assert result[0] == "test-tag"

    def test_insert_tag_existing_tag(self):
        """Test inserting an existing tag returns existing ID."""
        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            cursor = conn.cursor()

            # Insert tag first time
            first_id = migrate_registry_to_sqlite.insert_tag(cursor, "existing-tag", "tags")

            # Insert same tag again
            second_id = migrate_registry_to_sqlite.insert_tag(cursor, "existing-tag", "tags")

            assert first_id == second_id

            # Verify only one tag exists
            cursor.execute("SELECT COUNT(*) FROM tags WHERE name = ?", ("existing-tag",))
            count = cursor.fetchone()[0]
            assert count == 1

    def test_insert_tag_capability_table(self):
        """Test inserting tags into capability_tags table."""
        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            cursor = conn.cursor()

            tag_id = migrate_registry_to_sqlite.insert_tag(cursor, "capability", "capability_tags")
            assert tag_id is not None

            # Verify tag exists in capability_tags
            cursor.execute("SELECT name FROM capability_tags WHERE id = ?", (tag_id,))
            result = cursor.fetchone()
            assert result[0] == "capability"


class TestDataMigration(unittest.TestCase):
    """Test data migration from JSON to SQLite."""

    def setUp(self):
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as temp_db:
            self.db_path = Path(temp_db.name)

    def tearDown(self):
        if self.db_path.exists():
            self.db_path.unlink()

    def test_migrate_data_complete(self):
        """Test complete data migration."""
        registry_data = {
            "schema_version": "2.0",
            "protocol_version": "0.0.8",
            "last_updated": "2026-01-01T12:00:00",
            "capabilities_registry": "capabilities.json",
            "constitution": "constitution.md",
            "agents": [
                {
                    "id": "architect",
                    "path": "config/agents/architect.yaml",
                    "tier": "core",
                    "version": "0.0.8",
                    "activation": "default",
                    "tags": ["design", "planning"],
                    "capability_tags": ["system-design", "architecture"]
                },
                {
                    "id": "debugger",
                    "path": "config/agents/debugger.yaml",
                    "tier": "fleet",
                    "version": "0.0.8",
                    "activation": "on-demand",
                    "tags": ["debug", "fix"],
                    "capability_tags": ["troubleshooting"]
                }
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data)

            cursor = conn.cursor()

            # Check metadata
            cursor.execute("SELECT key, value FROM registry_metadata")
            metadata = dict(cursor.fetchall())
            assert metadata["schema_version"] == "2.0"
            assert metadata["protocol_version"] == "0.0.8"

            # Check agents
            cursor.execute("SELECT COUNT(*) FROM agents")
            agent_count = cursor.fetchone()[0]
            assert agent_count == 2

            cursor.execute("SELECT id, tier, activation FROM agents ORDER BY id")
            agents = cursor.fetchall()
            assert agents[0] == ("architect", "core", "default")
            assert agents[1] == ("debugger", "fleet", "on-demand")

            # Check tags
            cursor.execute("SELECT COUNT(*) FROM tags")
            tag_count = cursor.fetchone()[0]
            assert tag_count == 4  # design, planning, debug, fix

            # Check capability tags
            cursor.execute("SELECT COUNT(*) FROM capability_tags")
            cap_count = cursor.fetchone()[0]
            assert cap_count == 3  # system-design, architecture, troubleshooting

            # Check agent-tag relationships
            cursor.execute("SELECT COUNT(*) FROM agent_tags")
            agent_tag_count = cursor.fetchone()[0]
            assert agent_tag_count == 4  # 2 tags per agent

            # Check agent-capability relationships
            cursor.execute("SELECT COUNT(*) FROM agent_capabilities")
            agent_cap_count = cursor.fetchone()[0]
            assert agent_cap_count == 3  # 2 + 1 capabilities

    def test_migrate_data_minimal(self):
        """Test migration with minimal data."""
        registry_data = {
            "agents": [
                {
                    "id": "minimal-agent",
                    "path": "config/agents/minimal.yaml",
                    "tier": "fleet",
                    "version": "0.0.8"
                }
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data)

            cursor = conn.cursor()

            # Check agent exists
            cursor.execute("SELECT id, activation FROM agents WHERE id = ?", ("minimal-agent",))
            result = cursor.fetchone()
            assert result[0] == "minimal-agent"
            assert result[1] is None  # No activation specified

            # Check no tags/capabilities
            cursor.execute("SELECT COUNT(*) FROM agent_tags WHERE agent_id = ?", ("minimal-agent",))
            assert cursor.fetchone()[0] == 0

    def test_migrate_data_dry_run(self):
        """Test dry run mode doesn't write data."""
        registry_data = {
            "agents": [{"id": "test-agent", "path": "test.yaml", "tier": "fleet", "version": "1.0"}]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data, dry_run=True)

            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM agents")
            agent_count = cursor.fetchone()[0]
            assert agent_count == 0  # No data written in dry run

    def test_migrate_data_empty_agents(self):
        """Test migration with no agents."""
        registry_data = {"agents": []}

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data)

            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM agents")
            agent_count = cursor.fetchone()[0]
            assert agent_count == 0

    def test_migrate_data_duplicate_agents(self):
        """Test migration with duplicate agents (should replace)."""
        registry_data = {
            "agents": [
                {"id": "duplicate", "path": "path1.yaml", "tier": "core", "version": "1.0"},
                {"id": "duplicate", "path": "path2.yaml", "tier": "fleet", "version": "2.0"}
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data)

            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM agents WHERE id = ?", ("duplicate",))
            count = cursor.fetchone()[0]
            assert count == 1

            # Should have the second agent's data
            cursor.execute("SELECT path, tier FROM agents WHERE id = ?", ("duplicate",))
            result = cursor.fetchone()
            assert result == ("path2.yaml", "fleet")


class TestQueryViews(unittest.TestCase):
    """Test query view creation and functionality."""

    def setUp(self):
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as temp_db:
            self.db_path = Path(temp_db.name)

    def tearDown(self):
        if self.db_path.exists():
            self.db_path.unlink()

    def test_create_query_views(self):
        """Test creation of all query views."""
        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.create_query_views(conn)

            cursor = conn.cursor()
            cursor.execute("SELECT name FROM sqlite_master WHERE type='view'")
            views = [row[0] for row in cursor.fetchall()]

            expected_views = {
                "agent_tags_view",
                "agent_capabilities_view",
                "agents_complete"
            }
            assert set(views) == expected_views

    def test_agent_tags_view_functionality(self):
        """Test agent_tags_view with actual data."""
        registry_data = {
            "agents": [
                {
                    "id": "test-agent",
                    "path": "test.yaml",
                    "tier": "core",
                    "version": "1.0",
                    "activation": "default",
                    "tags": ["tag1", "tag2"]
                }
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data)
            migrate_registry_to_sqlite.create_query_views(conn)

            cursor = conn.cursor()
            cursor.execute("SELECT id, tags FROM agent_tags_view WHERE id = ?", ("test-agent",))
            result = cursor.fetchone()

            assert result[0] == "test-agent"
            tags = result[1].split(",") if result[1] else []
            assert set(tags) == {"tag1", "tag2"}

    def test_agent_capabilities_view_functionality(self):
        """Test agent_capabilities_view with actual data."""
        registry_data = {
            "agents": [
                {
                    "id": "capable-agent",
                    "path": "test.yaml",
                    "tier": "fleet",
                    "version": "1.0",
                    "capability_tags": ["cap1", "cap2"]
                }
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data)
            migrate_registry_to_sqlite.create_query_views(conn)

            cursor = conn.cursor()
            cursor.execute(
                "SELECT id, capabilities FROM agent_capabilities_view WHERE id = ?",
                ("capable-agent",),
            )
            result = cursor.fetchone()

            assert result[0] == "capable-agent"
            capabilities = result[1].split(",") if result[1] else []
            assert set(capabilities) == {"cap1", "cap2"}

    def test_agents_complete_view_functionality(self):
        """Test agents_complete view combines all data."""
        registry_data = {
            "agents": [
                {
                    "id": "complete-agent",
                    "path": "complete.yaml",
                    "tier": "core",
                    "version": "2.0",
                    "activation": "specialist",
                    "tags": ["complete"],
                    "capability_tags": ["full-stack"]
                }
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data)
            migrate_registry_to_sqlite.create_query_views(conn)

            cursor = conn.cursor()
            cursor.execute("""
                SELECT id, tier, activation, tags, capability_tags
                FROM agents_complete WHERE id = ?
            """, ("complete-agent",))
            result = cursor.fetchone()

            assert result[0] == "complete-agent"
            assert result[1] == "core"
            assert result[2] == "specialist"
            assert result[3] == "complete"
            assert result[4] == "full-stack"


class TestMigrationVerification(unittest.TestCase):
    """Test migration integrity verification."""

    def setUp(self):
        with tempfile.NamedTemporaryFile(suffix=".db", delete=False) as temp_db:
            self.db_path = Path(temp_db.name)

    def tearDown(self):
        if self.db_path.exists():
            self.db_path.unlink()

    def test_verify_migration_success(self):
        """Test successful migration verification."""
        registry_data = {
            "agents": [
                {"id": "agent1", "path": "a1.yaml", "tier": "core", "version": "1.0"},
                {"id": "agent2", "path": "a2.yaml", "tier": "fleet", "version": "1.0"}
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, registry_data)

            # Verification should pass
            result = migrate_registry_to_sqlite.verify_migration(conn, registry_data)
            assert result is True

    def test_verify_migration_count_mismatch(self):
        """Test verification failure due to count mismatch."""
        original_data = {
            "agents": [
                {"id": "agent1", "path": "a1.yaml", "tier": "core", "version": "1.0"},
                {"id": "agent2", "path": "a2.yaml", "tier": "fleet", "version": "1.0"}
            ]
        }

        partial_data = {
            "agents": [
                {"id": "agent1", "path": "a1.yaml", "tier": "core", "version": "1.0"}
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, partial_data)

            # Verification should fail
            result = migrate_registry_to_sqlite.verify_migration(conn, original_data)
            assert result is False

    def test_verify_migration_missing_agent(self):
        """Test verification failure due to missing agent."""
        original_data = {
            "agents": [
                {"id": "agent1", "path": "a1.yaml", "tier": "core", "version": "1.0"},
                {"id": "missing-agent", "path": "missing.yaml", "tier": "fleet", "version": "1.0"}
            ]
        }

        partial_data = {
            "agents": [
                {"id": "agent1", "path": "a1.yaml", "tier": "core", "version": "1.0"}
            ]
        }

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, partial_data)

            result = migrate_registry_to_sqlite.verify_migration(conn, original_data)
            assert result is False

    def test_verify_migration_empty_data(self):
        """Test verification with empty data."""
        empty_data = {"agents": []}

        with sqlite3.connect(self.db_path) as conn:
            migrate_registry_to_sqlite.create_schema(conn)
            migrate_registry_to_sqlite.migrate_data(conn, empty_data)

            result = migrate_registry_to_sqlite.verify_migration(conn, empty_data)
            assert result is True


class TestMainFunction(unittest.TestCase):
    """Test main migration function."""

    def setUp(self):
        self.temp_dir = tempfile.mkdtemp()
        self.temp_path = Path(self.temp_dir)
        # Mock global paths
        self.original_registry_file = migrate_registry_to_sqlite.REGISTRY_FILE
        self.original_db_file = migrate_registry_to_sqlite.DB_FILE
        self.original_backup_dir = migrate_registry_to_sqlite.BACKUP_DIR
        migrate_registry_to_sqlite.REGISTRY_FILE = self.temp_path / "agents/registry.json"
        migrate_registry_to_sqlite.DB_FILE = self.temp_path / "agents/registry.db"
        migrate_registry_to_sqlite.BACKUP_DIR = self.temp_path / "backups"

    def tearDown(self):
        migrate_registry_to_sqlite.REGISTRY_FILE = self.original_registry_file
        migrate_registry_to_sqlite.DB_FILE = self.original_db_file
        migrate_registry_to_sqlite.BACKUP_DIR = self.original_backup_dir
        shutil.rmtree(self.temp_dir)

    @patch("sys.argv", ["migrate_registry_to_sqlite.py"])
    def test_main_success(self):
        """Test successful main execution."""
        registry_data = {
            "agents": [
                {"id": "test", "path": "test.yaml", "tier": "core", "version": "1.0"}
            ]
        }
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        # Should complete without error
        migrate_registry_to_sqlite.main()

        # Verify database was created
        assert migrate_registry_to_sqlite.DB_FILE.exists()

        # Verify data was migrated
        with sqlite3.connect(migrate_registry_to_sqlite.DB_FILE) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM agents")
            count = cursor.fetchone()[0]
            assert count == 1

    @patch("sys.argv", ["migrate_registry_to_sqlite.py", "--backup"])
    def test_main_with_backup(self):
        """Test main execution with backup flag."""
        registry_data = {"agents": []}
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        migrate_registry_to_sqlite.main()

        # Verify backup was created
        assert migrate_registry_to_sqlite.BACKUP_DIR.exists()
        backup_files = list(migrate_registry_to_sqlite.BACKUP_DIR.glob("registry-*.json"))
        assert len(backup_files) == 1

    @patch("sys.argv", ["migrate_registry_to_sqlite.py", "--dry-run"])
    def test_main_dry_run(self):
        """Test main execution in dry run mode."""
        registry_data = {
            "agents": [
                {"id": "test", "path": "test.yaml", "tier": "core", "version": "1.0"}
            ]
        }
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        migrate_registry_to_sqlite.main()

        # Database should not be created in dry run
        assert not migrate_registry_to_sqlite.DB_FILE.exists()

    @patch("sys.argv", ["migrate_registry_to_sqlite.py"])
    def test_main_missing_registry(self):
        """Test main execution when registry file is missing."""
        with pytest.raises(SystemExit):
            migrate_registry_to_sqlite.main()

    @patch("sys.argv", ["migrate_registry_to_sqlite.py"])
    @patch("builtins.input", return_value="n")
    def test_main_database_exists_cancel(self, mock_input):
        """Test main execution when database exists and user cancels."""
        registry_data = {"agents": []}
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        # Create existing database
        migrate_registry_to_sqlite.DB_FILE.touch()

        with pytest.raises(SystemExit):
            migrate_registry_to_sqlite.main()

    @patch("sys.argv", ["migrate_registry_to_sqlite.py"])
    @patch("builtins.input", return_value="y")
    def test_main_database_exists_overwrite(self, mock_input):
        """Test main execution when database exists and user confirms overwrite."""
        registry_data = {
            "agents": [
                {"id": "test", "path": "test.yaml", "tier": "core", "version": "1.0"}
            ]
        }
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        # Create existing database
        migrate_registry_to_sqlite.DB_FILE.touch()

        migrate_registry_to_sqlite.main()

        # Should complete successfully and database should contain data
        with sqlite3.connect(migrate_registry_to_sqlite.DB_FILE) as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM agents")
            count = cursor.fetchone()[0]
            assert count == 1

    @patch("sys.argv", ["migrate_registry_to_sqlite.py"])
    @patch("migrate_registry_to_sqlite.create_schema")
    def test_main_migration_failure(self, mock_create_schema):
        """Test main execution when migration fails."""
        registry_data = {"agents": []}
        migrate_registry_to_sqlite.REGISTRY_FILE.write_text(json.dumps(registry_data))

        mock_create_schema.side_effect = Exception("Database error")

        with pytest.raises(SystemExit):
            migrate_registry_to_sqlite.main()

        # Database should be cleaned up on failure
        assert not migrate_registry_to_sqlite.DB_FILE.exists()


if __name__ == "__main__":
    unittest.main()
