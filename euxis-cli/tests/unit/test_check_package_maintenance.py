# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (c) 2024-2026 Sebastien Rousseau ᛫ https://sebastienrousseau.com

"""Comprehensive unit tests for cli/bin/check_package_maintenance.py.

Tests cover: get_package_info, parse_requirements_file,
check_maintenance_status with various package states.
"""

from __future__ import annotations

# Import the module under test
import sys
import tempfile
import unittest
from datetime import UTC, datetime, timedelta
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "bin"))
from check_package_maintenance import (
    check_maintenance_status,
    get_package_info,
    parse_requirements_file,
)


class TestGetPackageInfo(unittest.TestCase):
    """Tests for get_package_info function."""

    @patch("check_package_maintenance.requests.get")
    def test_successful_request(self, mock_get):
        """Test successful PyPI API request."""
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "info": {"name": "requests", "version": "2.28.0"},
            "releases": {"2.28.0": [{"upload_time": "2023-01-01T00:00:00"}]}
        }
        mock_get.return_value = mock_response

        result = get_package_info("requests", "2.28.0")

        assert result is not None
        assert "info" in result
        assert "releases" in result

    @patch("check_package_maintenance.requests.get")
    def test_failed_request_404(self, mock_get):
        """Test failed PyPI API request (404)."""
        mock_response = Mock()
        mock_response.status_code = 404
        mock_get.return_value = mock_response

        result = get_package_info("nonexistent-package", "1.0.0")
        assert result is None

    @patch("check_package_maintenance.requests.get")
    def test_request_exception(self, mock_get):
        """Test PyPI API request exception."""
        mock_get.side_effect = Exception("Network error")

        result = get_package_info("requests", "2.28.0")
        assert result is None

    @patch("check_package_maintenance.requests.get")
    def test_timeout_handling(self, mock_get):
        """Test request timeout is handled."""
        import requests
        mock_get.side_effect = requests.exceptions.Timeout("Timeout")

        result = get_package_info("requests", "2.28.0")
        assert result is None

    @patch("check_package_maintenance.requests.get")
    def test_package_name_cleaned(self, mock_get):
        """Test package name is cleaned before request."""
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {"info": {}, "releases": {}}
        mock_get.return_value = mock_response

        get_package_info("  requests  ", "2.28.0")

        # Verify the URL was called with cleaned name
        call_args = mock_get.call_args[0][0]
        assert "requests" in call_args
        assert "  " not in call_args


class TestParseRequirementsFile(unittest.TestCase):
    """Tests for parse_requirements_file function."""

    def test_parse_valid_requirements(self):
        """Test parsing valid requirements file."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("requests==2.28.0\n")
            f.write("flask==2.0.1\n")
            f.write("django==4.0.0\n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 3
            assert ("requests", "2.28.0") in result
            assert ("flask", "2.0.1") in result
            assert ("django", "4.0.0") in result

    def test_parse_with_comments(self):
        """Test parsing requirements with comments."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("# Production dependencies\n")
            f.write("requests==2.28.0\n")
            f.write("# Development only\n")
            f.write("pytest==7.0.0\n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 2
            # Comments should be excluded
            assert not any("#" in name for name, _ in result)

    def test_parse_with_empty_lines(self):
        """Test parsing requirements with empty lines."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("requests==2.28.0\n")
            f.write("\n")
            f.write("\n")
            f.write("flask==2.0.1\n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 2

    def test_parse_missing_file(self):
        """Test parsing missing file returns empty list."""
        result = parse_requirements_file("/nonexistent/requirements.txt")
        assert result == []

    def test_parse_skips_non_pinned(self):
        """Test lines without == are skipped."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("requests>=2.28.0\n")  # Not pinned
            f.write("flask~=2.0.1\n")  # Not pinned
            f.write("django==4.0.0\n")  # Pinned
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 1
            assert ("django", "4.0.0") in result

    def test_parse_strips_whitespace(self):
        """Test whitespace is stripped."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("  requests  ==  2.28.0  \n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 1


class TestCheckMaintenanceStatus(unittest.TestCase):
    """Tests for check_maintenance_status function."""

    def _make_release_data(self, days_ago):
        """Create release data for a package updated N days ago."""
        # Use naive datetime to match implementation's datetime.now()
        upload_time = datetime.now(UTC) - timedelta(days=days_ago)
        return {
            "1.0.0": [{"upload_time": upload_time.strftime("%Y-%m-%dT%H:%M:%S")}]
        }

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_maintained_package(self, mock_parse, mock_get_info):
        """Test package updated recently is marked maintained."""
        mock_parse.return_value = [("requests", "2.28.0")]
        mock_get_info.return_value = {
            "releases": self._make_release_data(30)  # 30 days ago
        }

        results = check_maintenance_status()

        assert len(results["maintained"]) == 1
        assert len(results["stale"]) == 0
        assert len(results["unmaintained"]) == 0

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_stale_package(self, mock_parse, mock_get_info):
        """Test package updated 6-12 months ago is marked stale."""
        mock_parse.return_value = [("old-package", "1.0.0")]
        mock_get_info.return_value = {
            "releases": self._make_release_data(200)  # 200 days ago
        }

        results = check_maintenance_status()

        assert len(results["maintained"]) == 0
        assert len(results["stale"]) == 1
        assert len(results["unmaintained"]) == 0

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_unmaintained_package(self, mock_parse, mock_get_info):
        """Test package not updated in over a year is marked unmaintained."""
        mock_parse.return_value = [("ancient-package", "v0.0.3")]
        mock_get_info.return_value = {
            "releases": self._make_release_data(400)  # 400 days ago
        }

        results = check_maintenance_status()

        assert len(results["maintained"]) == 0
        assert len(results["stale"]) == 0
        assert len(results["unmaintained"]) == 1

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_package_info_error(self, mock_parse, mock_get_info):
        """Test package info fetch error is recorded."""
        mock_parse.return_value = [("broken-package", "1.0.0")]
        mock_get_info.return_value = None

        results = check_maintenance_status()

        assert len(results["errors"]) == 1
        assert results["errors"][0]["name"] == "broken-package"

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_no_releases(self, mock_parse, mock_get_info):
        """Test package with no releases is marked unmaintained."""
        mock_parse.return_value = [("empty-package", "1.0.0")]
        mock_get_info.return_value = {
            "releases": {}
        }

        results = check_maintenance_status()

        assert len(results["unmaintained"]) == 1

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_empty_release_data(self, mock_parse, mock_get_info):
        """Test package with empty release data."""
        mock_parse.return_value = [("weird-package", "1.0.0")]
        mock_get_info.return_value = {
            "releases": {"1.0.0": []}  # No upload data
        }

        results = check_maintenance_status()

        # Should be marked unmaintained or in errors
        assert len(results["unmaintained"]) == 1 or len(results["errors"]) == 1

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_multiple_packages(self, mock_parse, mock_get_info):
        """Test multiple packages with different statuses."""
        mock_parse.return_value = [
            ("fresh", "1.0.0"),
            ("stale", "1.0.0"),
            ("old", "1.0.0")
        ]

        def get_info_side_effect(name, version):
            if name == "fresh":
                return {"releases": self._make_release_data(10)}
            if name == "stale":
                return {"releases": self._make_release_data(200)}
            return {"releases": self._make_release_data(400)}

        mock_get_info.side_effect = get_info_side_effect

        results = check_maintenance_status()

        assert len(results["maintained"]) == 1
        assert len(results["stale"]) == 1
        assert len(results["unmaintained"]) == 1

    @patch("check_package_maintenance.parse_requirements_file")
    def test_empty_requirements(self, mock_parse):
        """Test empty requirements file."""
        mock_parse.return_value = []

        results = check_maintenance_status()

        assert results["maintained"] == []
        assert results["stale"] == []
        assert results["unmaintained"] == []
        assert results["errors"] == []


class TestResultStructure(unittest.TestCase):
    """Tests for result dictionary structure."""

    @patch("check_package_maintenance.parse_requirements_file")
    def test_result_has_all_keys(self, mock_parse):
        """Test result contains all expected keys."""
        mock_parse.return_value = []

        results = check_maintenance_status()

        assert "maintained" in results
        assert "stale" in results
        assert "unmaintained" in results
        assert "errors" in results

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_result_entry_structure(self, mock_parse, mock_get_info):
        """Test result entries have expected fields."""
        mock_parse.return_value = [("requests", "2.28.0")]
        upload_time = datetime.now(UTC) - timedelta(days=30)
        mock_get_info.return_value = {
            "releases": {"2.28.0": [{"upload_time": upload_time.strftime("%Y-%m-%dT%H:%M:%S")}]}
        }

        results = check_maintenance_status()

        entry = results["maintained"][0]
        assert "name" in entry
        assert "version" in entry
        assert "latest_version" in entry
        assert "last_release" in entry
        assert "days_since_update" in entry


class TestEdgeCases(unittest.TestCase):
    """Edge case tests for check_package_maintenance."""

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_release_without_upload_time(self, mock_parse, mock_get_info):
        """Test release without upload_time field."""
        mock_parse.return_value = [("weird", "1.0.0")]
        mock_get_info.return_value = {
            "releases": {"1.0.0": [{"filename": "weird-1.0.0.tar.gz"}]}  # No upload_time
        }

        results = check_maintenance_status()

        # Should handle gracefully
        assert len(results["unmaintained"]) == 1 or len(results["errors"]) == 1

    @patch("check_package_maintenance.get_package_info")
    @patch("check_package_maintenance.parse_requirements_file")
    def test_multiple_release_versions(self, mock_parse, mock_get_info):
        """Test package with multiple release versions."""
        mock_parse.return_value = [("multi", "2.0.0")]

        now = datetime.now(UTC)
        mock_get_info.return_value = {
            "releases": {
                "1.0.0": [
                    {
                        "upload_time": (now - timedelta(days=400)).strftime(
                            "%Y-%m-%dT%H:%M:%S"
                        )
                    }
                ],
                "2.0.0": [
                    {
                        "upload_time": (now - timedelta(days=30)).strftime(
                            "%Y-%m-%dT%H:%M:%S"
                        )
                    }
                ],
            }
        }

        results = check_maintenance_status()

        # Should use most recent release
        assert len(results["maintained"]) == 1


if __name__ == "__main__":
    unittest.main()
