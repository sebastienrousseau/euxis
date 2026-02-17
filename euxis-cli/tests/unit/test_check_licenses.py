# SPDX-License-Identifier: MIT
# Copyright (c) 2024-2026 Euxis Contributors

"""Comprehensive unit tests for cli/bin/check_licenses.py.

Tests cover: COPYLEFT_LICENSES, LICENSE_ALIASES, get_package_license,
classify_license, parse_requirements_file, check_licenses.
"""

from __future__ import annotations

# Import the module under test
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "bin"))
from check_licenses import (
    COPYLEFT_LICENSES,
    LICENSE_ALIASES,
    classify_license,
    get_package_license,
    parse_requirements_file,
)


class TestCopyleftLicenses(unittest.TestCase):
    """Tests for COPYLEFT_LICENSES constant."""

    def test_copyleft_licenses_is_set(self):
        """Test COPYLEFT_LICENSES is a set."""
        assert isinstance(COPYLEFT_LICENSES, set)

    def test_copyleft_licenses_not_empty(self):
        """Test COPYLEFT_LICENSES has entries."""
        assert len(COPYLEFT_LICENSES) > 0

    def test_gpl_in_copyleft(self):
        """Test GPL is in copyleft licenses."""
        assert "GPL" in COPYLEFT_LICENSES

    def test_agpl_in_copyleft(self):
        """Test AGPL is in copyleft licenses."""
        assert "AGPL" in COPYLEFT_LICENSES or "AGPL-3.0" in COPYLEFT_LICENSES

    def test_lgpl_in_copyleft(self):
        """Test LGPL is in copyleft licenses."""
        assert "LGPL" in COPYLEFT_LICENSES


class TestLicenseAliases(unittest.TestCase):
    """Tests for LICENSE_ALIASES constant."""

    def test_license_aliases_is_dict(self):
        """Test LICENSE_ALIASES is a dictionary."""
        assert isinstance(LICENSE_ALIASES, dict)

    def test_mit_alias(self):
        """Test MIT has an alias."""
        assert "MIT" in LICENSE_ALIASES

    def test_bsd_alias(self):
        """Test BSD has an alias."""
        assert "BSD" in LICENSE_ALIASES

    def test_apache_alias(self):
        """Test Apache has an alias."""
        assert "Apache" in LICENSE_ALIASES or "Apache-2.0" in LICENSE_ALIASES


class TestClassifyLicense(unittest.TestCase):
    """Tests for classify_license function."""

    def test_classify_permissive_mit(self):
        """Test MIT is classified as permissive."""
        info = {"license": "MIT", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "PERMISSIVE"

    def test_classify_permissive_bsd(self):
        """Test BSD is classified as permissive."""
        info = {"license": "BSD-3-Clause", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "PERMISSIVE"

    def test_classify_permissive_apache(self):
        """Test Apache is classified as permissive."""
        info = {"license": "Apache-2.0", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "PERMISSIVE"

    def test_classify_permissive_isc(self):
        """Test ISC is classified as permissive."""
        info = {"license": "ISC", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "PERMISSIVE"

    def test_classify_copyleft_gpl(self):
        """Test GPL is classified as copyleft."""
        info = {"license": "GPL-3.0", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "COPYLEFT"

    def test_classify_copyleft_agpl(self):
        """Test AGPL is classified as copyleft."""
        info = {"license": "AGPL-3.0", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "COPYLEFT"

    def test_classify_copyleft_lgpl(self):
        """Test LGPL is classified as copyleft."""
        info = {"license": "LGPL-3.0", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "COPYLEFT"

    def test_classify_unknown_empty(self):
        """Test empty license is classified as unknown."""
        info = {"license": "", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "UNKNOWN"

    def test_classify_other(self):
        """Test unrecognized license is classified as other."""
        info = {"license": "CustomProprietaryLicense", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "OTHER"

    def test_classify_from_classifiers(self):
        """Test classification from classifiers."""
        info = {
            "license": "",
            "license_expression": "",
            "classifiers": ["License :: OSI Approved :: MIT License"]
        }
        result = classify_license(info)
        assert result == "PERMISSIVE"

    def test_classify_from_license_expression(self):
        """Test classification from license_expression."""
        info = {"license": "", "license_expression": "MIT", "classifiers": []}
        result = classify_license(info)
        assert result == "PERMISSIVE"

    def test_classify_case_insensitive(self):
        """Test classification is case insensitive."""
        info = {"license": "mit", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "PERMISSIVE"

    def test_classify_public_domain(self):
        """Test public domain is classified as permissive."""
        info = {"license": "Public Domain", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "PERMISSIVE"

    def test_classify_unlicense(self):
        """Test Unlicense is classified as permissive."""
        info = {"license": "Unlicense", "license_expression": "", "classifiers": []}
        result = classify_license(info)
        assert result == "PERMISSIVE"


class TestParseRequirementsFile(unittest.TestCase):
    """Tests for parse_requirements_file function."""

    def test_parse_valid_requirements(self):
        """Test parsing valid requirements file."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("requests==2.28.0\n")
            f.write("flask==2.0.1\n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 2
            assert ("requests", "2.28.0") in result
            assert ("flask", "2.0.1") in result

    def test_parse_with_comments(self):
        """Test parsing requirements with comments."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("# This is a comment\n")
            f.write("requests==2.28.0\n")
            f.write("# Another comment\n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 1
            assert ("requests", "2.28.0") in result

    def test_parse_with_empty_lines(self):
        """Test parsing requirements with empty lines."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("requests==2.28.0\n")
            f.write("\n")
            f.write("flask==2.0.1\n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 2

    def test_parse_missing_file(self):
        """Test parsing missing file returns empty list."""
        result = parse_requirements_file("/nonexistent/path/requirements.txt")
        assert result == []

    def test_parse_no_version_specifier(self):
        """Test lines without == are skipped."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("requests>=2.28.0\n")
            f.write("flask==2.0.1\n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 1
            assert ("flask", "2.0.1") in result

    def test_parse_whitespace_handling(self):
        """Test whitespace is handled correctly."""
        with tempfile.NamedTemporaryFile(mode="w", suffix=".txt", delete=False) as f:
            f.write("  requests == 2.28.0  \n")
            f.flush()

            result = parse_requirements_file(f.name)
            assert len(result) == 1
            # Whitespace should be stripped
            name, version = result[0]
            assert name.strip() == "requests"


class TestGetPackageLicense(unittest.TestCase):
    """Tests for get_package_license function."""

    @patch("check_licenses.requests.get")
    def test_successful_request(self, mock_get):
        """Test successful PyPI API request."""
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "info": {
                "license": "MIT",
                "license_expression": "MIT",
                "classifiers": ["License :: OSI Approved :: MIT License"],
                "author": "Test Author",
                "home_page": "https://example.com",
                "project_url": ""
            }
        }
        mock_get.return_value = mock_response

        result = get_package_license("requests", "2.28.0")

        assert result is not None
        assert result["name"] == "requests"
        assert result["version"] == "2.28.0"
        assert result["license"] == "MIT"

    @patch("check_licenses.requests.get")
    def test_failed_request(self, mock_get):
        """Test failed PyPI API request."""
        mock_response = Mock()
        mock_response.status_code = 404
        mock_get.return_value = mock_response

        result = get_package_license("nonexistent-package", "1.0.0")
        assert result is None

    @patch("check_licenses.requests.get")
    def test_request_timeout(self, mock_get):
        """Test PyPI API request timeout."""
        mock_get.side_effect = Exception("Timeout")

        result = get_package_license("requests", "2.28.0")
        assert result is None

    @patch("check_licenses.requests.get")
    def test_extracts_classifiers(self, mock_get):
        """Test extraction of license classifiers."""
        mock_response = Mock()
        mock_response.status_code = 200
        mock_response.json.return_value = {
            "info": {
                "license": "",
                "license_expression": "",
                "classifiers": [
                    "License :: OSI Approved :: MIT License",
                    "Programming Language :: Python :: 3"
                ],
                "author": "",
                "home_page": "",
                "project_url": ""
            }
        }
        mock_get.return_value = mock_response

        result = get_package_license("test", "1.0.0")

        # Should only include License classifiers
        assert len(result["classifiers"]) == 1
        assert "MIT License" in result["classifiers"][0]


class TestCheckLicensesIntegration(unittest.TestCase):
    """Integration tests for check_licenses workflow."""

    @patch("check_licenses.get_package_license")
    @patch("check_licenses.parse_requirements_file")
    def test_check_licenses_workflow(self, mock_parse, mock_get_license):
        """Test full check_licenses workflow."""
        from check_licenses import check_licenses

        mock_parse.return_value = [
            ("requests", "2.28.0"),
            ("flask", "2.0.1")
        ]

        mock_get_license.side_effect = [
            {
                "license": "MIT",
                "license_expression": "",
                "classifiers": [],
                "name": "requests",
                "version": "2.28.0",
            },
            {
                "license": "BSD",
                "license_expression": "",
                "classifiers": [],
                "name": "flask",
                "version": "2.0.1",
            },
        ]

        with patch("check_licenses.time.sleep"):  # Skip rate limiting
            results = check_licenses()

        assert "permissive" in results
        assert "copyleft" in results
        assert "errors" in results

    @patch("check_licenses.parse_requirements_file")
    def test_check_licenses_empty_requirements(self, mock_parse):
        """Test check_licenses with empty requirements."""
        from check_licenses import check_licenses

        mock_parse.return_value = []

        results = check_licenses()

        assert results["permissive"] == []
        assert results["copyleft"] == []
        assert results["errors"] == []


if __name__ == "__main__":
    unittest.main()
