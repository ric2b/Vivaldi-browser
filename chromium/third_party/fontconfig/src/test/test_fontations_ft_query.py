#! /usr/bin/env python3
# Copyright (C) 2025 Google LLC.
# SPDX-License-Identifier: HPND

from fctest import FcTest, FcExternalTestFont
from pathlib import Path
import pytest


@pytest.fixture
def fctest():
    return FcTest()


@pytest.mark.parametrize("font_file", FcExternalTestFont().fonts)
def test_fontations_freetype_fcquery_equal(fctest, font_file):
    fctest.logger.info(f'Testing with: {font_file}')

    font_path = Path(font_file)

    if not font_path.exists():
        pytest.skip(f"Font file not found: {font_file}")  # Skip if file missing

    for ret, stdout, stderr in fctest.run_query([font_file]):
        assert ret == 0, stderr
        result_freetype = stdout.strip().splitlines()
    fctest.with_fontations = True
    for ret, stdout, stderr in fctest.run_query([font_file]):
        assert ret == 0, stderr
        result_fontations = stdout.strip().splitlines()

    assert (
        result_freetype == result_fontations
    ), f"FreeType and Fontations fc-query result must match. Fontations: {result_fontations}, FreeType: {result_freetype}"
