#! /usr/bin/env python3
# Copyright (C) 2025 Google LLC.
# SPDX-License-Identifier: HPND

import os
from pathlib import Path
import pytest
import re
import requests
import subprocess


def builddir():
    return Path(os.environ.get("builddir", Path(__file__).parent.parent))


def list_test_fonts():
    font_files = []
    for root, _, files in os.walk(builddir() / "testfonts"):
        for file in files:
            # Variable .ttc not supported yet.
            if file.endswith(".ttf"):
                font_files.append(os.path.join(root, file))
    return font_files


def run_fc_query(font_file, format_string, with_fontations=False):
    fc_query_path = builddir() / "fc-query" / "fc-query"

    env = os.environ.copy()
    if with_fontations:
        env["FC_FONTATIONS"] = ""

    result = subprocess.run(
        [fc_query_path, "-f", format_string, font_file],
        stdout=subprocess.PIPE,
        env=env,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )

    assert (
        result.returncode == 0
    ), f"fc-query failed for {font_file} with error: {result.stderr}"
    assert result.stdout, f"fc-query produced no output for {font_file}"

    return result


@pytest.mark.parametrize("font_file", list_test_fonts())
def test_fontations_freetype_fcquery_equal(font_file):
    print(f"Testing with: {font_file}")  # Example usage

    supported_format_entitites = [
        "family",
        "familylang",
        "outline",
        "scalable",
        "fontformat",
        "color",
        "fonthashint",
        "foundry",
        "version",
        "weight",
        "width",
        "slant",
        "capability",
    ]
    format_string = ":".join(
        "%{" + entity + "}" for entity in supported_format_entitites
    )
    print(format_string)

    font_path = Path(font_file)

    if not font_path.exists():
        pytest.skip(f"Font file not found: {font_file}")  # Skip if file missing

    result_freetype = run_fc_query(font_file, format_string).stdout.strip().splitlines()
    result_fontations = (
        run_fc_query(font_file, format_string, with_fontations=True)
        .stdout.strip()
        .splitlines()
    )

    assert (
        result_freetype == result_fontations
    ), f"FreeType and Fontations fc-query result must match. Fontations: {result_fontations}, FreeType: {result_freetype}"
