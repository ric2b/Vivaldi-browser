#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
from typing import List, Optional, Tuple

import metadata.fields.field_types as field_types
import metadata.fields.util as util
import metadata.validation_result as vr

# List of supported vulnerability ID prefixes.
_VULN_PREFIXES = [
    "CVE",  # Common Vulnerabilities and Exposures.
    "GHSA",  # GitHub Security Advisory.
    "PYSEC",  # Python Security Advisory.
    "OSV",  # Open Source Vulnerability.
    "DSA",  # Debian Security Advisory.
]

_PATTERN_PREFIX = "|".join(_VULN_PREFIXES)
PATTERN_VULN_ID = re.compile(
    rf"({_PATTERN_PREFIX})-[a-zA-Z0-9]{{4}}-[a-zA-Z0-9:-]+")
PATTERN_VULN_ID_WITH_ANCHORS = re.compile(f"^{PATTERN_VULN_ID.pattern}$")


def validate_vuln_ids(vuln_ids: str) -> Tuple[List[str], List[str]]:
    """
    Validates a list of vulnerability identifiers and returns valid and invalid IDs.

    Supports multiple formats:
    - CVE IDs (e.g., CVE-2024-12345)
    - GitHub Security Advisories (e.g., GHSA-1234-5678-90ab)
    - Python Security Advisories (e.g., PYSEC-2024-1234)
    - Open Source Vulnerabilities (e.g., OSV-2024-1234)
    - Debian Security Advisories (e.g., DSA-1234-1)

    Args:
        vuln_ids: List of vulnerability identifiers to validate

    Returns:
        Tuple of (valid_ids, invalid_ids)
    """
    valid_vuln_ids = []
    invalid_vuln_ids = []

    for cve in vuln_ids.split(","):
        cve_stripped = cve.strip()
        if PATTERN_VULN_ID_WITH_ANCHORS.match(cve_stripped):
            valid_vuln_ids.append(cve_stripped)
        else:
            invalid_vuln_ids.append(cve)

    return valid_vuln_ids, invalid_vuln_ids


class MitigatedField(field_types.SingleLineTextField):
    """Field for comma-separated vulnerability IDs."""

    def __init__(self):
        super().__init__(name="Mitigated")

    def validate(self, value: str) -> Optional[vr.ValidationResult]:
        """Checks if the value contains valid CVE IDs."""
        if util.is_empty(value):
            return None
        _, invalid_vuln_ids = validate_vuln_ids(value)

        if invalid_vuln_ids:
            return vr.ValidationWarning(
                reason=f"{self._name} contains invalid vulnerability IDs.",
                additional=[
                    f"Invalid Vulnerability IDs: {util.quoted(invalid_vuln_ids)}",
                    "The following identifiers are supported: " +
                    ", ".join(_VULN_PREFIXES),
                ],
            )

        return None

    def narrow_type(self, value: str) -> Optional[List[str]]:
        if not value:
            return None
        vuln_ids, _ = validate_vuln_ids(value)
        return vuln_ids
