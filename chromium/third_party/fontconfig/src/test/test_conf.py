# Copyright (C) 2025 fontconfig Authors
# SPDX-License-Identifier: HPND

from fctest import FcTest, FcTestFont
from operator import attrgetter
from pathlib import Path
from tempfile import TemporaryDirectory
import os
import pytest
import re
import shutil
import tempfile
import time
import types


@pytest.fixture
def fctest():
    return FcTest()


def dict_conf_with_json():
    srcdir = os.getenv('srcdir', Path(__file__).parent.parent)
    ret = {}
    for fn in (Path(srcdir) / 'conf.d').glob('*.conf'):
        json = Path(srcdir) / 'test' / ('test-' + fn.stem + '.json')
        if json.exists():
            ret[str(fn)] = str(json)

    return ret


def list_json():
    srcdir = os.getenv('srcdir', Path(__file__).parent.parent)
    pairs = list(dict_conf_with_json().values())
    FcTest().logger.info(pairs)
    ret = []
    for fn in (Path(srcdir) / 'test').glob('test-*.json'):
        if str(fn) not in pairs:
            ret.append(str(fn))

    return ret


@pytest.mark.parametrize(
    'conf, json',
    [(k, v) for k, v in dict_conf_with_json().items()],
    ids=lambda x: Path(x).name)
def test_pair_of_conf_and_json(fctest, conf, json):
    testexe = Path(fctest.builddir) / 'test' / ('test-conf' + fctest._exeext)
    if not testexe.exists():
        testexe = Path(fctest.builddir) / 'test' / ('test_conf' + fctest._exeext)
        if not testexe.exists():
            pytest.skip('No test executable. maybe missing json-c dependency?')

    for ret, stdout, stderr in fctest.run(testexe, [conf, json]):
        assert ret == 0, f'stdout:\n{stdout}\nstderr:\n{stderr}'
        fctest.logger.info(stdout)


@pytest.mark.parametrize('json', list_json(), ids=lambda x: Path(x).name)
def test_json(fctest, json):
    testexe = Path(fctest.builddir) / 'test' / ('test-conf' + fctest._exeext)
    if not testexe.exists():
        testexe = Path(fctest.builddir) / 'test' / ('test_conf' + fctest._exeext)
        if not testexe.exists():
            pytest.skip('No test executable. maybe missing json-c dependency?')
    harmlessconf = str(Path(fctest.srcdir) / 'conf.d' / '10-autohint.conf')

    for ret, stdout, stderr in fctest.run(testexe, [harmlessconf, json]):
        assert ret == 0, f'stdout:\n{stdout}\nstderr:\n{stderr}'
        fctest.logger.info(stdout)
