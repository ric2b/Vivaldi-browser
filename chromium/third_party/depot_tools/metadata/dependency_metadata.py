#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from collections import defaultdict
import os
import sys
from typing import Dict, List, Set, Tuple

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
# The repo's root directory.
_ROOT_DIR = os.path.abspath(os.path.join(_THIS_DIR, ".."))

# Add the repo's root directory for clearer imports.
sys.path.insert(0, _ROOT_DIR)

import metadata.fields.field_types as field_types
import metadata.fields.custom.license as license_util
import metadata.fields.custom.version as version_util
import metadata.fields.known as known_fields
import metadata.fields.util as util
import metadata.validation_result as vr


class DependencyMetadata:
    """The metadata for a single dependency."""

    # Fields that are always required.
    _MANDATORY_FIELDS = {
        known_fields.NAME,
        known_fields.URL,
        known_fields.VERSION,
        known_fields.LICENSE,
        known_fields.SECURITY_CRITICAL,
        known_fields.SHIPPED,
    }

    # Aliases for fields, where:
    #     * key is the alias field; and
    #     * value is the main field to which it should be mapped.
    # Note: if both the alias and main fields are specified in metadata,
    #       the value from the alias field will be used.
    _FIELD_ALIASES = {
        known_fields.SHIPPED_IN_CHROMIUM: known_fields.SHIPPED,
    }

    def __init__(self):
        # The record of all entries added, including repeated fields.
        self._entries: List[Tuple[str, str]] = []

        # The current value of each field.
        self._metadata: Dict[field_types.MetadataField, str] = {}

        # The record of how many times a field entry was added.
        self._occurrences: Dict[field_types.MetadataField,
                                int] = defaultdict(int)

    def add_entry(self, field_name: str, field_value: str):
        value = field_value.strip()
        self._entries.append((field_name, value))

        field = known_fields.get_field(field_name)
        if field:
            self._metadata[field] = value
            self._occurrences[field] += 1

    def has_entries(self) -> bool:
        return len(self._entries) > 0

    def get_entries(self) -> List[Tuple[str, str]]:
        return list(self._entries)

    def _assess_required_fields(self) -> Set[field_types.MetadataField]:
        """Returns the set of required fields, based on the current
        metadata.
        """
        required = set(self._MANDATORY_FIELDS)

        # Assume the dependency is shipped if not specified.
        shipped_value = self._metadata.get(known_fields.SHIPPED)
        is_shipped = (shipped_value is None
                      or util.infer_as_boolean(shipped_value, default=True))

        if is_shipped:
            # A license file is required if the dependency is shipped.
            required.add(known_fields.LICENSE_FILE)

            # License compatibility with Android must be set if the
            # package is shipped and the license is not in the
            # allowlist.
            has_allowlisted = False
            license_value = self._metadata.get(known_fields.LICENSE)
            if license_value:
                licenses = license_util.process_license_value(
                    license_value,
                    atomic_delimiter=known_fields.LICENSE.VALUE_DELIMITER)
                for _, allowed in licenses:
                    if allowed:
                        has_allowlisted = True
                        break

            if not has_allowlisted:
                required.add(known_fields.LICENSE_ANDROID_COMPATIBLE)

        return required

    def validate(self, source_file_dir: str,
                 repo_root_dir: str) -> List[vr.ValidationResult]:
        """Validates all the metadata.

        Args:
            source_file_dir: the directory of the file that the metadata
                             is from.
            repo_root_dir: the repository's root directory.

        Returns: the metadata's validation results.
        """
        results = []

        # Check for duplicate fields.
        repeated_field_info = [
            f"{field.get_name()} ({count})"
            for field, count in self._occurrences.items() if count > 1
        ]
        if repeated_field_info:
            repeated = ", ".join(repeated_field_info)
            error = vr.ValidationError(reason="There is a repeated field.",
                                       additional=[
                                           f"Repeated fields: {repeated}",
                                       ])
            results.append(error)

        # Process alias fields.
        sources = {}
        for alias_field, main_field in self._FIELD_ALIASES.items():
            if alias_field in self._metadata:
                # Validate the value that was present for the main field
                # before overwriting it with the alias field value.
                if main_field in self._metadata:
                    main_value = self._metadata.get(main_field)
                    field_result = main_field.validate(main_value)
                    if field_result:
                        field_result.set_tag(tag="field",
                                             value=main_field.get_name())
                        results.append(field_result)

                self._metadata[main_field] = self._metadata[alias_field]
                sources[main_field] = alias_field
                self._metadata.pop(alias_field)

        # Validate values for all present fields.
        for field, value in self._metadata.items():
            source_field = sources.get(field) or field
            field_result = source_field.validate(value)
            if field_result:
                field_result.set_tag(tag="field", value=source_field.get_name())
                results.append(field_result)

        # Check required fields are present.
        required_fields = self._assess_required_fields()
        for field in required_fields:
            if field not in self._metadata:
                field_name = field.get_name()
                error = vr.ValidationError(
                    reason=f"Required field '{field_name}' is missing.")
                results.append(error)

        # At least one of the fields Version, Date or Revision must be
        # provided.
        version_value = self._metadata.get(known_fields.VERSION)
        date_value = self._metadata.get(known_fields.DATE)
        revision_value = self._metadata.get(known_fields.REVISION)
        if ((not version_value or version_util.is_unknown(version_value))
                and (not date_value or util.is_unknown(date_value))
                and (not revision_value or util.is_unknown(revision_value))):
            versioning_fields = [
                known_fields.VERSION, known_fields.DATE, known_fields.REVISION
            ]
            names = util.quoted(
                [field.get_name() for field in versioning_fields])
            error = vr.ValidationError(
                reason="Versioning fields are insufficient.",
                additional=[f"Provide at least one of [{names}]."],
            )
            results.append(error)

        # Check existence of the license file(s) on disk.
        license_file_value = self._metadata.get(known_fields.LICENSE_FILE)
        if license_file_value is not None:
            result = known_fields.LICENSE_FILE.validate_on_disk(
                value=license_file_value,
                source_file_dir=source_file_dir,
                repo_root_dir=repo_root_dir,
            )
            if result:
                result.set_tag(tag="field",
                               value=known_fields.LICENSE_FILE.get_name())
                results.append(result)

        return results
