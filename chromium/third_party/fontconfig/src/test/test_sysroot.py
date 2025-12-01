# Copyright (C) 2025 fontconfig Authors
# SPDX-License-Identifier: HPND

from fctest import FcTest, FcTestFont
from pathlib import Path
from tempfile import TemporaryDirectory
import hashlib
import os
import pytest
import shutil


@pytest.fixture
def fctest():
    return FcTest()


@pytest.fixture
def fcfont():
    return FcTestFont()


@pytest.mark.skipif(not not os.getenv('EXEEXT'), reason='not working on Win32')
def test_sysroot(fctest, fcfont):
    basedir = TemporaryDirectory(prefix='fontconfig.',
                                 suffix='.sysroot')
    cachedir = Path(basedir.name) / Path(fctest.cachedir.name).relative_to('/')
    cachedir.mkdir(parents=True, exist_ok=True)
    fontdir = Path(basedir.name) / Path(fctest.fontdir.name).relative_to('/')
    fontdir.mkdir(parents=True, exist_ok=True)
    configdir = Path(basedir.name) / Path(fctest._conffile.name).parent.relative_to('/')
    configdir.mkdir(parents=True, exist_ok=True)
    fctest.setup()
    fctest.install_font(fcfont.fonts[0], fontdir)
    shutil.copy2(fctest._conffile.name, configdir / Path(fctest._conffile.name).name)
    for ret, stdout, stderr in fctest.run_cache(['-y', basedir.name]):
        assert ret == 0, stderr
    font_files = [fn.name for fn in fontdir.glob('*.pcf')]
    assert len(font_files) == 1, font_files
    cache_files = [c.name for c in cachedir.glob('*cache*')]
    assert len(cache_files) == 1, cache_files

    md5 = hashlib.md5()
    md5.update(fctest.fontdir.name.encode('utf-8'))
    cache_files_based_on_md5 = [c.name for c in cachedir.glob(f'{md5.hexdigest()}*')]
    assert len(cache_files_based_on_md5) == 1, cache_files_based_on_md5
