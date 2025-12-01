#! /usr/bin/env python3
# Copyright (C) 2024 fontconfig Authors
# SPDX-License-Identifier: HPND

import os
import pytest
import re
import requests
import shutil
import subprocess
from pathlib import Path


def test_issue431(tmp_path):
    builddir = Path(os.environ.get("builddir", Path(__file__).parent.parent))
    roboto_flex_font = (
        builddir
        / "testfonts"
        / "roboto-flex-fonts/fonts/variable/RobotoFlex[GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght].ttf"
    )

    if not roboto_flex_font.exists():
        pytest.skip(f"Font file not found: {roboto_flex_font}")

    result = subprocess.run(
        [
            builddir / "fc-query" / "fc-query",
            "-f",
            "%{family[0]}:%{index}:%{style[0]}:%{postscriptname}\n",
            roboto_flex_font,
        ],
        stdout=subprocess.PIPE,
    )

    for line in result.stdout.decode("utf-8").splitlines():
        family, index, style, psname = line.split(":")
        normstyle = re.sub("[\x04\\(\\)/<>\\[\\]{}\t\f\r\n ]", "", style)
        assert (
            psname.split("-")[-1] == normstyle
        ), f"postscriptname `{psname}' does not contain style name `{normstyle}': index {index}"
