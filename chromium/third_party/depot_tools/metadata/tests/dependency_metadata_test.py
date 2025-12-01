#!/usr/bin/env vpython3
# Copyright (c) 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys
import unittest

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
# The repo's root directory.
_ROOT_DIR = os.path.abspath(os.path.join(_THIS_DIR, "..", ".."))

# Add the repo's root directory for clearer imports.
sys.path.insert(0, _ROOT_DIR)

import metadata.dependency_metadata as dm
import metadata.fields.known as known_fields
import metadata.validation_result as vr


class DependencyValidationTest(unittest.TestCase):
    def test_repeated_field(self):
        """Check that a validation error is returned for a repeated
        field.
        """
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test repeated field")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")
        dependency.add_entry(known_fields.NAME.get_name(), "again")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertTrue(isinstance(results[0], vr.ValidationError))
        self.assertEqual(results[0].get_reason(), "There is a repeated field.")

    def test_only_alias_field(self):
        """Check that an alias field can be used for a main field."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test alias field used")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        # Use Shipped in Chromium instead of Shipped.
        dependency.add_entry(known_fields.SHIPPED_IN_CHROMIUM.get_name(), "no")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 0)

    def test_alias_overwrite_invalid_field(self):
        """Check that the value for an overwritten field (from an alias
        field) is still validated.
        """
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test alias field overwrite")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.SHIPPED_IN_CHROMIUM.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "test")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].get_reason(), "Shipped is invalid.")

    def test_alias_invalid_field_attributed(self):
        """Check that an invalid value from an alias field is attributed
        to that alias field.
        """
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test alias field error attributed")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.SHIPPED_IN_CHROMIUM.get_name(),
                             "test")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "yes")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertEqual(results[0].get_reason(),
                         "Shipped in Chromium is invalid.")

    def test_versioning_field(self):
        """Check that a validation error is returned for insufficient
        versioning info. No Date/Revision and Version is N/A."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test metadata missing versioning info")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "N/A")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertTrue(isinstance(results[0], vr.ValidationError))
        self.assertEqual(results[0].get_reason(),
                         "Versioning fields are insufficient.")

    def test_versioning_with_invalid_revision(self):
        """Check that a validation error is returned for insufficient
        versioning info."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test metadata missing versioning info")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "N/A")
        dependency.add_entry(known_fields.REVISION.get_name(), "N/A")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 2)
        self.assertTrue(isinstance(results[0], vr.ValidationError))
        self.assertTrue(isinstance(results[1], vr.ValidationError))
        self.assertEqual(results[0].get_reason(),
                         "Revision is not a valid hexadecimal revision.")
        self.assertEqual(results[1].get_reason(),
                         "Versioning fields are insufficient.")

    def test_invalid_revision(self):
        """Check invalid revision formats return validation errors."""

        # Test invalid revision format (non-hexadecimal).
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(), "Invalid Revision")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.REVISION.get_name(),
                             "invalid_revision")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertTrue(isinstance(results[0], vr.ValidationError))
        self.assertEqual(
            results[0].get_reason(),
            "Revision is not a valid hexadecimal revision.",
        )

    def test_valid_revision(self):
        """Check valid revision formats return no validation issues."""

        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(), "Valid Revision")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        # No errors for no revision.
        self.assertEqual(len(results), 0)

        dependency.add_entry(known_fields.REVISION.get_name(),
                             "abcdef1")  # Valid.
        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        # No errors for valid revision.
        self.assertEqual(len(results), 0)

    def test_valid_revision_in_deps(self):
        """Check "Revision: DEPS" is acceptable."""

        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(), "Dependency")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "N/A")
        dependency.add_entry(known_fields.REVISION.get_name(), "DEPS")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        # No errors for no revision.
        self.assertEqual(len(results), 0)

    def test_required_field(self):
        """Check that a validation error is returned for a missing field."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.NAME.get_name(), "Test missing field")
        # Leave URL field unspecified.

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertTrue(isinstance(results[0], vr.ValidationError))
        self.assertEqual(results[0].get_reason(),
                         "Required field 'URL' is missing.")

    def test_invalid_field(self):
        """Check field validation issues are returned."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.NAME.get_name(), "Test invalid field")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "test")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertTrue(isinstance(results[0], vr.ValidationError))
        self.assertEqual(results[0].get_reason(),
                         "Security Critical is invalid.")

    def test_invalid_license_file_path(self):
        """Check license file path validation issues are returned."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test license file path")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(),
                             "MISSING-LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertTrue(isinstance(results[0], vr.ValidationWarning))
        self.assertEqual(results[0].get_reason(), "License File is invalid.")

    def test_multiple_validation_issues(self):
        """Check all validation issues are returned."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test multiple errors")
        # Leave URL field unspecified.
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(),
                             "MISSING-LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "test")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")
        dependency.add_entry(known_fields.NAME.get_name(), "again")

        # Check 4 validation results are returned, for:
        #   - missing field;
        #   - invalid license file path;
        #   - invalid yes/no field value; and
        #   - repeated field entry.
        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 4)

    def test_valid_metadata(self):
        """Check valid metadata returns no validation issues."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test valid metadata")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 0)

    def test_dep_is_canonical_skips_versioning_requirement(self):
        """
        Check versioning information isn't required for dependencies where
        Chromium is the canonical repository.
        """
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test valid metadata")
        dependency.add_entry(known_fields.URL.get_name(),
                             "This is the canonical repository")
        dependency.add_entry(known_fields.VERSION.get_name(), "N/A")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "yes")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "yes")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 0)

    def test_all_licenses_allowlisted(self):
        """Test that a single allowlisted license returns True."""
        dependency = dm.DependencyMetadata()
        self.assertTrue(dependency.all_licenses_allowlisted("MIT", False))
        self.assertTrue(dependency.all_licenses_allowlisted("MIT, GPL-2.0", False))
        self.assertTrue(dependency.all_licenses_allowlisted("MIT, Apache-2.0", False))
        self.assertFalse(dependency.all_licenses_allowlisted("InvalidLicense", False))
        self.assertFalse(dependency.all_licenses_allowlisted("MIT, InvalidLicense", False))
        self.assertFalse(dependency.all_licenses_allowlisted("", False))

        # "MPL-2.0" is a reciprocal license, i.e. only allowed in open source projects.
        self.assertTrue(dependency.all_licenses_allowlisted("MPL-2.0", True))
        self.assertFalse(dependency.all_licenses_allowlisted("MPL-2.0", False))

        # Restricted licenses are treated the same as other license types, until
        # the exception and enforcement is resourced.
        self.assertTrue(dependency.all_licenses_allowlisted("GPL-2.0", False))
        self.assertTrue(dependency.all_licenses_allowlisted("GPL-2.0", True))
        self.assertFalse(dependency.all_licenses_allowlisted("MPL-2.0, GPL-2.0", False))


    def test_only_open_source_licenses(self):
        """Test that only open source licenses are returned."""
        dependency = dm.DependencyMetadata()
        self.assertEqual(dependency.only_open_source_licenses(""), [])
        self.assertEqual(dependency.only_open_source_licenses("MIT"), [])
        self.assertEqual(dependency.only_open_source_licenses("GPL-2.0"), [])
        self.assertEqual(dependency.only_open_source_licenses("MPL-2.0"), ["MPL-2.0"])
        result = dependency.only_open_source_licenses("MIT, MPL-2.0")
        self.assertEqual(result, ["MPL-2.0"])
        result = dependency.only_open_source_licenses("MPL-2.0, APSL-2.0")
        self.assertEqual(set(result), {"MPL-2.0", "APSL-2.0"})
        # Test with mix of invalid and valid licenses
        result = dependency.only_open_source_licenses("InvalidLicense, MPL-2.0")
        self.assertEqual(result, ["MPL-2.0"])

    def test_mitigated_validation(self):
        """Tests validation of Mitigated field and corresponding CVE descriptions."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(), "Test Dependency")
        dependency.add_entry(known_fields.URL.get_name(), "http://example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "yes")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "yes")

        # Add description for one CVE and an extra one.
        dependency.add_entry("CVE-2024-1234", "Fixed in this version")
        dependency.add_entry("CVE-2024-9999", "This shouldn't be here")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        # Check that a warning is returned when only CVE descriptions are
        # present.
        self.assertEqual(len(results), 1)
        self.assertTrue(isinstance(results[0], vr.ValidationWarning))
        self.assertEqual(results[0].get_reason(),
                         "Found descriptions for unlisted vulnerability IDs")
        self.assertIn("CVE-2024-1234",results[0].get_additional()[0])
        self.assertIn("CVE-2024-9999",results[0].get_additional()[0])

        # Add Mitigated field with two CVEs.
        dependency.add_entry(known_fields.MITIGATED.get_name(),
                             "CVE-2024-1234, CVE-2024-5678")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )

        # Should get two warnings:
        # 1. Missing description for CVE-2024-5678
        # 2. Extra description for CVE-2024-9999
        self.assertEqual(len(results), 2)
        self.assertTrue(isinstance(results[0], vr.ValidationWarning))
        self.assertEqual(results[0].get_reason(),
                         "Missing descriptions for vulnerability IDs")
        self.assertIn("CVE-2024-5678",results[0].get_additional()[0])
        self.assertTrue(isinstance(results[1], vr.ValidationWarning))
        self.assertEqual(results[1].get_reason(),
                         "Found descriptions for unlisted vulnerability IDs")
        self.assertIn("CVE-2024-9999",results[1].get_additional()[0])


    def test_vuln_scan_sufficiency(self):
        """Tests the vuln_scan_sufficiency property."""
        # Test case: sufficient:CPE.
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.CPE_PREFIX.get_name(),
                             "cpe:/a:vendor:product")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "sufficient:CPE")

        # Test case: sufficient:URL and Revision.
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(), "https://example.com")
        dependency.add_entry(known_fields.REVISION.get_name(), "abcdef123456")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "sufficient:URL and Revision")

        # Test case: sufficient:URL and Revision[DEPS], given 'Revision:DEPS'.
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(), "https://example.com")
        dependency.add_entry(known_fields.REVISION.get_name(), "DEPS")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "sufficient:URL and Revision[DEPS]")

        # Test case: sufficient:URL and Version.
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(), "https://example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.2.3")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "sufficient:URL and Version")

        # Test case: ignore:Static (because of update mechanism).
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.UPDATE_MECHANISM.get_name(), "Static")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "ignore:Static")

        # Test case: ignore:Canonical (only URL).
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(), "This is the canonical public repository")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "ignore:Canonical")

        # Test case: ignore:Internal (only URL).
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(), "Google internal")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "ignore:Internal")

        # Test case: ignore:Internal takes precedence over ignore:Static.
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(), "Google Internal.")
        dependency.add_entry(known_fields.UPDATE_MECHANISM.get_name(), "Static.HardFork")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "ignore:Internal")

        # Test case: insufficient (bad bug link).
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.UPDATE_MECHANISM.get_name(), "Manual (bad_bug_link)")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "insufficient")

        # Test case: ignore:Static (because not shipped).
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "insufficient")

        # Test case: insufficient (no relevant fields, shipped defaults to None).
        dependency = dm.DependencyMetadata()
        self.assertEqual(dependency.vuln_scan_sufficiency,
                          "insufficient")

        # Test case: insufficient (only URL).
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.URL.get_name(), "https://example.com")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "insufficient")

        # Test case: CPE takes precedence over URL/Revision.
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.CPE_PREFIX.get_name(),
                             "cpe:/a:vendor:product")
        dependency.add_entry(known_fields.URL.get_name(), "https://example.com")
        dependency.add_entry(known_fields.REVISION.get_name(), "abcdef123456")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "sufficient:CPE")

        # Test case: URL/Revision takes precedence over static update mechanism.
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.UPDATE_MECHANISM.get_name(), "Static")
        dependency.add_entry(known_fields.URL.get_name(), "https://example.com")
        dependency.add_entry(known_fields.REVISION.get_name(), "abcdef123456")
        self.assertEqual(dependency.vuln_scan_sufficiency,
                         "sufficient:URL and Revision")


def test_update_mechanism_validation(self):
    """Tests the validation logic for the Update Mechanism field."""
    # A list of test cases with the value to test and the expected error, if any.
    test_cases = {
        # --- Valid Cases ---
        "Valid Autoroll": ("Autoroll", None),
        "Valid Manual": ("Manual (crbug.com/123)", None),
        "Valid Static": ("Static (crbug.com/456)", None),
        "Valid Static.HardFork": ("Static.HardFork (crbug.com/789)", None),
        "Valid with extra whitespace": ("  Manual (crbug.com/123)  ", None),

        # --- Invalid Cases ---
        "Invalid format":
        ("Invalid Value", "Invalid format for Update Mechanism field."),
        "Unknown mechanism":
        ("Custom (crbug.com/123)", "Invalid mechanism 'Custom'."),
        "Autoroll with bug link": ("Autoroll (crbug.com/123)",
                                   "A bug link is not allowed for 'Autoroll'."),
        "Manual without bug link":
        ("Manual", "A bug link is required for 'Manual'."),
        "Static without bug link": ("Static",
                                    "A bug link is required for 'Static'."),
        "Static.HardFork without bug link":
        ("Static.HardFork", "A bug link is required for 'Static.HardFork'."),
    }

    for name, (value, expected_error) in test_cases.items():
        with self.subTest(msg=name):
            dependency = dm.DependencyMetadata()
            # Populate with other valid, required fields to isolate the test
            dependency.add_entry(known_fields.NAME.get_name(), f"Test {name}")
            dependency.add_entry(known_fields.URL.get_name(),
                                 "https://www.example.com")
            dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
            dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
            dependency.add_entry(known_fields.LICENSE_FILE.get_name(),
                                 "LICENSE")
            dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(),
                                 "no")
            dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

            # Add the Update Mechanism field to test
            dependency.add_entry("Update Mechanism", value)

            results = dependency.validate(
                source_file_dir=os.path.join(_THIS_DIR, "data"),
                repo_root_dir=_THIS_DIR,
            )

            if expected_error is None:
                self.assertEqual(len(results), 0,
                                 f"Expected no errors for value: '{value}'")
            else:
                self.assertEqual(len(results), 1,
                                 f"Expected one error for value: '{value}'")
                self.assertTrue(isinstance(results[0], vr.ValidationError))
                self.assertEqual(results[0].get_reason(), expected_error)

    # Test case for a missing Update Mechanism field, assuming it's required.
    with self.subTest(msg="Missing field"):
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test Missing Update Mechanism")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE.get_name(), "MIT")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")
        # The "Update Mechanism" field is omitted.

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )

        # Assuming 'Update Mechanism' is now a required field in your known_fields config.
        # If so, the validation framework should catch its absence.
        error_found = any(
            "Required field 'Update Mechanism' is missing." in r.get_reason()
            for r in results)
        self.assertTrue(
            error_found,
            "Expected an error for missing Update Mechanism field.")

if __name__ == "__main__":
    unittest.main()
