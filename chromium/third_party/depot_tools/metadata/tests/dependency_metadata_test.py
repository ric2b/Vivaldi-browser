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
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public Domain")
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
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public domain")
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
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public domain")
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
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public domain")
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
        versioning info."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test metadata missing versioning info")
        dependency.add_entry(known_fields.URL.get_name(),
                             "https://www.example.com")
        dependency.add_entry(known_fields.VERSION.get_name(), "N/A")
        dependency.add_entry(known_fields.REVISION.get_name(), "unknown")
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public Domain")
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

    def test_required_field(self):
        """Check that a validation error is returned for a missing field."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public Domain")
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
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public domain")
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
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public domain")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(),
                             "MISSING-LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 1)
        self.assertTrue(isinstance(results[0], vr.ValidationError))
        self.assertEqual(results[0].get_reason(), "License File is invalid.")

    def test_multiple_validation_issues(self):
        """Check all validation issues are returned."""
        dependency = dm.DependencyMetadata()
        dependency.add_entry(known_fields.NAME.get_name(),
                             "Test multiple errors")
        # Leave URL field unspecified.
        dependency.add_entry(known_fields.VERSION.get_name(), "1.0.0")
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public domain")
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
        dependency.add_entry(known_fields.LICENSE.get_name(), "Public Domain")
        dependency.add_entry(known_fields.LICENSE_FILE.get_name(), "LICENSE")
        dependency.add_entry(known_fields.SECURITY_CRITICAL.get_name(), "no")
        dependency.add_entry(known_fields.SHIPPED.get_name(), "no")

        results = dependency.validate(
            source_file_dir=os.path.join(_THIS_DIR, "data"),
            repo_root_dir=_THIS_DIR,
        )
        self.assertEqual(len(results), 0)


if __name__ == "__main__":
    unittest.main()
