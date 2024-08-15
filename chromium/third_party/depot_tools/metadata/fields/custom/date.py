#!/usr/bin/env python3
# Copyright 2023 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import datetime
import os
import sys
from typing import Union

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
# The repo's root directory.
_ROOT_DIR = os.path.abspath(os.path.join(_THIS_DIR, "..", "..", ".."))

# Add the repo's root directory for clearer imports.
sys.path.insert(0, _ROOT_DIR)

import metadata.fields.field_types as field_types
import metadata.validation_result as vr

# The preferred date format for the start of date values.
_PREFERRED_PREFIX_FORMAT = "%Y-%m-%d"

# Formats for the start of date values that are recognized as
# alternative date formats.
_RECOGNIZED_PREFIX_FORMATS = (
    "%d-%m-%Y",
    "%m-%d-%Y",
    "%d-%m-%y",
    "%m-%d-%y",
    "%d/%m/%Y",
    "%m/%d/%Y",
    "%d/%m/%y",
    "%m/%d/%y",
    "%d.%m.%Y",
    "%m.%d.%Y",
    "%d.%m.%y",
    "%m.%d.%y",
    "%Y/%m/%d",
    "%Y.%m.%d",
    "%Y%m%d",
)

# Formats recognized as alternative date formats (entire value must
# match).
_RECOGNIZED_DATE_FORMATS = (
    "%d %b %Y",
    "%d %b, %Y",
    "%b %d %Y",
    "%b %d, %Y",
    "%Y %b %d",
    "%d %B %Y",
    "%d %B, %Y",
    "%B %d %Y",
    "%B %d, %Y",
    "%Y %B %d",
    "%a %b %d %H:%M:%S %Y",
    "%a %b %d %H:%M:%S %Y %z",
)


def format_matches(value: str, date_format: str):
    """Returns whether the given value matches the date format."""
    try:
        datetime.datetime.strptime(value, date_format)
    except ValueError:
        return False
    return True


class DateField(field_types.MetadataField):
    """Custom field for the date when the package was updated."""
    def __init__(self):
        super().__init__(name="Date", one_liner=True)

    def validate(self, value: str) -> Union[vr.ValidationResult, None]:
        """Checks the given value is a YYYY-MM-DD date."""
        value = value.strip()
        if not value:
            return vr.ValidationError(
                reason=f"{self._name} is empty.",
                additional=["Provide date in format YYYY-MM-DD."])

        # Check if the first part (to ignore timezone info) uses the
        # preferred format.
        parts = value.split()
        if format_matches(parts[0], _PREFERRED_PREFIX_FORMAT):
            return None

        # Check if the first part (to ignore timezone info) uses a
        # recognized format.
        for prefix_format in _RECOGNIZED_PREFIX_FORMATS:
            if format_matches(parts[0], prefix_format):
                return vr.ValidationWarning(
                    reason=f"{self._name} is not in the preferred format.",
                    additional=[
                        "Use YYYY-MM-DD.", f"Current value is '{value}'."
                    ])

        # Check the entire value for recognized date formats.
        for date_format in _RECOGNIZED_DATE_FORMATS:
            if format_matches(value, date_format):
                return vr.ValidationWarning(
                    reason=f"{self._name} is not in the preferred format.",
                    additional=[
                        "Use YYYY-MM-DD.", f"Current value is '{value}'."
                    ])

        # Return an error as the value's format was not recognized.
        return vr.ValidationError(
            reason=f"{self._name} is invalid.",
            additional=["Use YYYY-MM-DD.", f"Current value is '{value}'."])
