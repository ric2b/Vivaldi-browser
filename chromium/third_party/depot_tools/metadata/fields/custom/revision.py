#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import sys
from typing import Optional

_THIS_DIR = os.path.abspath(os.path.dirname(__file__))
# The repo's root directory.
_ROOT_DIR = os.path.abspath(os.path.join(_THIS_DIR, "..", "..", ".."))

# Add the repo's root directory for clearer imports.
sys.path.insert(0, _ROOT_DIR)

import metadata.fields.field_types as field_types
import metadata.fields.custom.version as version_field
import metadata.fields.util as util
import metadata.validation_result as vr


class RevisionField(field_types.SingleLineTextField):
    """Custom field for the revision."""

    def __init__(self):
        super().__init__(name="Revision")

    def narrow_type(self, value: str) -> Optional[str]:
        value = super().narrow_type(value)
        if not value:
            return None

        if version_field.version_is_unknown(value):
            return None

        if util.is_known_invalid_value(value):
            return None

        return value
