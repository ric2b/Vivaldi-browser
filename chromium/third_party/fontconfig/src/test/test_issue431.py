#! /usr/bin/env python3
# Copyright (C) 2024 fontconfig Authors
# SPDX-License-Identifier: HPND

import pytest
import re
from pathlib import Path
from fctest import FcTest


@pytest.fixture
def fctest():
    return FcTest()


def test_issue431(fctest):
    roboto_flex_font = (
        Path(fctest.builddir)
        / "testfonts"
        / "roboto-flex-fonts/fonts/variable/RobotoFlex[GRAD,XOPQ,XTRA,YOPQ,YTAS,YTDE,YTFI,YTLC,YTUC,opsz,slnt,wdth,wght].ttf"
    )

    if not roboto_flex_font.exists():
        pytest.skip(f"Font file not found: {roboto_flex_font}")

    for ret, stdout, stderr in fctest.run_query(['-f',
                                                 '%{family[0]}:%{index}:%{style[0]}:%{postscriptname}\n',
                                                 roboto_flex_font]):
        assert ret == 0, stderr
        for line in stdout.splitlines():
            family, index, style, psname = line.split(":")
            normstyle = re.sub("[\x04\\(\\)/<>\\[\\]{}\t\f\r\n ]", "", style)
            assert (
                psname.split("-")[-1] == normstyle
            ), f"postscriptname `{psname}' does not contain style name `{normstyle}': index {index}"
