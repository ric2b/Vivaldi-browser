# Copyright (C) 2025 fontconfig Authors
# SPDX-License-Identifier: HPND

from fctest import FcTest, FcTestFont
from pathlib import Path
from tempfile import TemporaryDirectory, NamedTemporaryFile
import os
import pytest
import time
import types


@pytest.fixture
def fctest():
    return FcTest()


@pytest.fixture
def fcfont():
    return FcTestFont()


@pytest.mark.skipif(not not os.getenv('EXEEXT'), reason='not working on Win32')
def test_crbug1004254(fctest, fcfont):
    builddir = Path(fctest.builddir)
    def custom_config(self):
        return f'''
<fontconfig>
  <dir>{builddir.resolve()}/testfonts</dir>
  <cachedir>{fctest.cachedir.name}</cachedir>
</fontconfig>'''

    fctest.config = types.MethodType(custom_config, fctest)
    fctest.setup()

    testexe = Path(fctest.builddir) / 'test' / ('test-crbug1004254' + fctest._exeext)
    if not testexe.exists():
        testexe = Path(fctest.builddir) / 'test' / ('test_crbug1004254' + fctest._exeext)
        if not testexe.exists():
            raise RuntimeError('No test case for crbug1004254')

    for ret, stdout, stderr in fctest.run(testexe):
        assert ret == 0, stderr
        fctest.logger.info(stdout)
        fctest.logger.info(stderr)
        fctest.logger.info(fctest.config())
