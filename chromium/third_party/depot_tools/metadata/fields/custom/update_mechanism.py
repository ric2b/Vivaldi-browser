#!/usr/bin/env python3
# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
from typing import Optional, Tuple

import metadata.fields.field_types as field_types
import metadata.fields.util as util
import metadata.validation_result as vr

# The regex for validating the structure of the Update Mechanism field.
UPDATE_MECHANISM_REGEX = re.compile(
    r"""
    ^           # Start of the string.

                # Group 1: Primary Mechanism (e.g., "Autoroll", "Manual", "Static").
                # It matches one or more characters that are not a dot, whitespace,
    ([^.\s(]+)  # or an opening parenthesis.

                # Group 2 (optional) Secondary Mechanism: preceded by a dot (e.g., ".HardFork").
    (?:         # Start of a non-capturing group for the optional part.
      \.        # .
      ([^\s(]+) # Capture the secondary mechanism.
    )?          # Indicates 'secondary' is optional.

                # Group 3 (optional): A bug link in parentheses (e.g., "(https://crbug.com/12345)").
    (?:         # Use a non-capturing group to catch the parentheses and whitespace.
      \s*       # Optional whitespace.
      \(        # (
      ([^)]+)   # Capture the content inside the parentheses.
      \)        # )
    )?          # Indicates 'bug_link' is optional.

    $           # End of the string.
    """, re.VERBOSE)


# Regex for validating the format of the bug link and capturing the bug ID.
BUG_LINK_REGEX = re.compile(r"^https://crbug\.com/(\d+)$")

# A set of the fully-qualified, allowed mechanism values.
ALLOWED_MECHANISMS = {
    "Autoroll",
    "Manual",
    "Static",
    "Static.HardFork",
}


def parse_update_mechanism(
        value: str) -> Tuple[str, Optional[str], Optional[str]]:
    """
    Parses the Update Mechanism field value using a regular expression.
    Values are expected to be in the form Primary.Secondary (bug link)

    Args:
        value: The string value of the Update Mechanism field.

    Returns:
        A tuple (primary, secondary, bug_link).
        If the structure is invalid, all elements of the tuple are None.
    """
    match = UPDATE_MECHANISM_REGEX.match(value.strip())
    if not match:
        return None, None, None
    return match.groups()


class UpdateMechanismField(field_types.SingleLineTextField):
    """
    Field for 'Update Mechanism: <Value>'.
    The format is Primary[.SubsetSpecifier] [(crbug.com/BUG_ID)].
    """

    def __init__(self):
        super().__init__(name="Update Mechanism")

    def validate(self, value: str) -> Optional[vr.ValidationResult]:
        """
        Checks if the value is a valid Update Mechanism entry, including the
        logic for when a bug link is required or disallowed.
        """
        if util.is_empty(value):
            return vr.ValidationError(
                reason=f"{self._name} field cannot be empty.",
                additional=[
                    f"Must be one of {util.quoted(sorted(ALLOWED_MECHANISMS))}.",
                    "Example: 'Autoroll' or 'Manual (https://crbug.com/12345)'"
                ])

        primary, secondary, bug_link = parse_update_mechanism(value)
        # First, check if the value matches the general format.
        if primary is None:
            return vr.ValidationError(
                reason=f"Invalid format for {self._name} field.",
                additional=[
                    "Expected format: Mechanism[.SubMechanism] [(bug)]",
                    f"Allowed mechanisms: {util.quoted(sorted(ALLOWED_MECHANISMS))}.",
                    "Example: 'Static.HardFork (https://crbug.com/12345)'",
                ])

        mechanism = primary
        if secondary:
            mechanism += f".{secondary}"
        # Second, check if the mechanism is a known, allowed value.
        if mechanism not in ALLOWED_MECHANISMS:
            return vr.ValidationError(
                reason=f"{self._name} has invalid mechanism '{mechanism}'.",
                additional=[
                    f"Must be one of {util.quoted(sorted(ALLOWED_MECHANISMS))}.",
                ])

        # If it's not Autorolled, it SHOULD have a bug link.
        # Only warn for Static, for now.
        elif primary == "Static" and bug_link is None:
            return vr.ValidationWarning(
                reason="{self._name} has no link to autoroll exception.",
                additional=[
                    "Please add a link if an exception bug has been filed.",
                    f"Example: '{mechanism} (https://crbug.com/12345)'"
                ])

        # Autoroll must not have a bug link.
        if primary == "Autoroll" and bug_link:
            return vr.ValidationError(
                reason="Autoroll does not permit an autoroll exception.",
                additional=[
                    f"Please remove the unnecessary bug link {bug_link}.",
                    "If this bug is still relevant then maybe Autoroll isn't the right choice",
                    "You could move it to the description.",
                ])

        # Validate the bug link format if present.
        if bug_link:
            bug_num = bug_link.split("/")[-1]
            canonical_bug_link = f"https://crbug.com/{bug_num}"
            if not BUG_LINK_REGEX.match(bug_link):
                # If it ends in a '/number', then provide a copy pastable correct value.
                if bug_num.isdigit():
                    return vr.ValidationError(
                        reason=f"{self._name} bug link should be `({canonical_bug_link})`.",
                        additional=[
                            f"{bug_link} is not a valid crbug link."
                        ])
                # Does not match the expected crbug link format at all.
                return vr.ValidationError(
                    reason=f"{self._name} has invalid bug link format '{bug_link}'.",
                    additional=[
                        "Bug links must be of the form (https://crbug.com/12345).",
                        f"Example: '{mechanism} (https://crbug.com/12345)'",
                    ])

        return None

    def narrow_type(self,
                    value: str) -> Tuple[str, Optional[str], Optional[str]]:
        """
        Parses the field value into its components if it is valid.

        Returns:
            A tuple of (primary, secondary, bug_link) if valid, otherwise None, None, None.
        """
        validation = self.validate(value)
        if validation and validation.is_fatal():
            # It cannot be narrowed if there is a fatal error.
            return None, None, None

        primary, secondary, bug_link = parse_update_mechanism(value)

        # If a bug link is present, parse it into canonical format.
        if bug_link:
            bug_num = bug_link.split("/")[-1]
            bug_link = f"https://crbug.com/{bug_num}"

        return primary, secondary, bug_link
