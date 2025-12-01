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


@pytest.fixture
def fcfont():
    return FcTestFont()


@pytest.mark.skipif(not not os.getenv('EXEEXT'), reason='not working on Win32')
@pytest.mark.skipif(not shutil.which('bwrap'), reason='No bwrap installed')
def test_bz106618(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts, '.')
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr
    time.sleep(1)
    cache_stat_before = [f.stat() for f in fctest.cache_files()]
    cache_stat_after = []
    basedir = tempfile.TemporaryDirectory(prefix='fontconfig.',
                                          suffix='.base')
    with fctest.sandboxed(basedir.name) as f:
        # Test if font is visible on sandbox
        for ret, stdout, stderr in f.run_match(['-f', '%{file}\n',
                                                ':foundry=Misc']):
            assert ret == 0, stderr
            out = list(filter(None, stdout.splitlines()))
            assert len(out) == 1, out
            assert re.match(str(Path(f._remapped_fontdir.name) / '4x6.pcf'),
                            out[0])
        testexe = Path(f.builddir) / 'test' / ('test-bz106618' + fctest._exeext)
        if not testexe.exists():
            testexe = Path(f.builddir) / 'test' / ('test_bz106618' + fctest._exeext)
            if not testexe.exists():
                raise RuntimeError('No test case for bz106618')
        flist1 = []
        for ret, stdout, stderr in f.run(Path(f._remapped_builddir.name) / 'test' / testexe.name):
            assert ret == 0, stderr
            flist1 = sorted(stdout.splitlines())
        # convert path to bind-mounted one
        flist2 = sorted(map(lambda x: str(x) if Path(x).parent != Path(f.fontdir.name) else str(Path(f._remapped_fontdir.name) / Path(x).name), Path(f.fontdir.name).glob('*.pcf')))
        assert flist1 == flist2

    # Check if cache files isn't created over bind-mounted dir again
    cache_stat_after = [f.stat() for f in fctest.cache_files()]
    assert len(cache_stat_before) == len(cache_stat_after)
    # ignore st_atime
    cmp_cache_before = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat_before]
    cmp_cache_after = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat_after]
    assert cmp_cache_before == cmp_cache_after


@pytest.mark.skipif(not not os.getenv('EXEEXT'), reason='not working on Win32')
@pytest.mark.skipif(not shutil.which('bwrap'), reason='No bwrap installed')
def test_different_content(fctest, fcfont):
    '''
    Make sure if fontdir where sandbox has own fonts is handled
    differently even if they are same directory name for remapped
    '''
    fctest.setup()
    fctest.install_font(fcfont.fonts[0], '.')
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr
    time.sleep(1)
    cache_stat_before = [f.stat() for f in fctest.cache_files()]
    sbox_fontdir = TemporaryDirectory(prefix='fontconfig.',
                                      suffix='.fontdir')
    sbox_cachedir = TemporaryDirectory(prefix='fontconfig.',
                                       suffix='.cachedir')
    sbox_basedir = TemporaryDirectory(prefix='fontconfig.',
                                      suffix='.basedir')
    fontdir2 = TemporaryDirectory(prefix='fontconfig.',
                                  suffix='.fontdir',
                                  dir=sbox_basedir.name)
    fctest.install_font(fcfont.fonts[1], fontdir2.name)
    fontdir = fctest.fontdir

    def custom_config(self):
        return f'<fontconfig><remap-dir as-path="{fontdir.name}">{sbox_fontdir.name}</remap-dir><dir>{sbox_fontdir.name}</dir><dir salt="salt-to-make-difference">{fontdir.name}</dir><cachedir>{sbox_cachedir.name}</cachedir></fontconfig>'

    fctest.config = types.MethodType(custom_config, fctest)
    bind = {
        # remap to share a host cache
        fctest.fontdir.name: sbox_fontdir.name,
        # sandbox has own font on same directory like host but different fonts
        fontdir2.name: fctest.fontdir.name
    }
    with fctest.sandboxed(sbox_basedir.name, bind=bind) as f:
        # Test if font is visible on sandbox
        for ret, stdout, stderr in f.run_match(['-f', '%{file}\n',
                                                ':foundry=Misc']):
            assert ret == 0, stderr
            out = list(filter(None, stdout.splitlines()))
            assert len(out) == 1, out
            assert re.match(str(Path(sbox_fontdir.name) / '4x6.pcf'),
                            out[0])
        testexe = Path(f.builddir) / 'test' / ('test-bz106618' + fctest._exeext)
        if not testexe.exists():
            testexe = Path(f.builddir) / 'test' / ('test_bz106618' + fctest._exeext)
            if not testexe.exists():
                raise RuntimeError('No test case for bz106618')
        flist1 = []
        for ret, stdout, stderr in f.run(Path(f._remapped_builddir.name) / 'test' / testexe.name):
            assert ret == 0, stderr
            flist1 = sorted(stdout.splitlines())
        # convert path to bind-mounted one
        flist2 = list(map(lambda x: str(x) if Path(x).parent != Path(f.fontdir.name) else str(Path(sbox_fontdir.name) / Path(x).name), Path(f.fontdir.name).glob('*.pcf')))
        flist2 += list(map(lambda x: str(x) if Path(x).parent != Path(fontdir2.name) else str(Path(fontdir.name) / Path(x).name), Path(fontdir2.name).glob('*.pcf')))
        flist2 = sorted(flist2)
        assert len(flist1) == 2, flist1
        assert flist1 == flist2

    # Check if cache files isn't created over bind-mounted dir again
    cache_stat_after = [f.stat() for f in fctest.cache_files()]
    assert len(cache_stat_before) == len(cache_stat_after)
    # ignore st_atime
    cmp_cache_before = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat_before]
    cmp_cache_after = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat_after]
    assert cmp_cache_before == cmp_cache_after


@pytest.mark.skipif(not not os.getenv('EXEEXT'), reason='not working on Win32')
@pytest.mark.skipif(not shutil.which('bwrap'), reason='No bwrap installed')
def test_md5_consistency(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts[0], 'sub')
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr
    time.sleep(1)
    cache_files_before = [f.name for f in fctest.cache_files()]
    cache_stat_before = [f.stat() for f in fctest.cache_files()]
    cachedir2 = TemporaryDirectory(prefix='fontconfig.',
                                   suffix='.cachedir')
    basedir = TemporaryDirectory(prefix='fontconfig.',
                                 suffix='.base')
    orig_cachedir = fctest.cachedir
    fctest._cachedir = cachedir2
    with fctest.sandboxed(basedir.name) as f:
        for ret, stdout, stderr in f.run_cache([f._remapped_fontdir.name]):
            assert ret == 0, stderr
    cache_files_after = [c.name for c in fctest.cache_files()]
    cache_stat_after = [c.stat() for c in fctest.cache_files()]
    # ignore st_atime
    cmp_cache_before = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat_before]
    cmp_cache_after = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat_after]

    # Make sure they are totally different but same filename
    assert cache_files_before == cache_files_after
    assert cmp_cache_before != cmp_cache_after


@pytest.mark.skipif(not not os.getenv('EXEEXT'), reason='not working on Win32')
@pytest.mark.skipif(not shutil.which('bwrap'), reason='No bwrap installed')
def test_gen_testcache(fctest, fcfont):
    testexe = Path(fctest.builddir) / 'test' / ('test-gen-testcache' + fctest._exeext)
    if not testexe.exists():
        testexe = Path(fctest.builddir) / 'test' / ('test_gen_testcache' + fctest._exeext)
        if not testexe.exists():
            raise RuntimeError('No test case for gen-testcache')

    fctest.setup()
    fctest.install_font(fcfont.fonts, '.')
    for ret, stdout, stderr in fctest.run(testexe, [fctest.fontdir.name]):
        assert ret == 0, stderr
        cache_files1 = [f for f in fctest.cache_files()]
        assert len(cache_files1) == 1, cache_files1
        time.sleep(1)
        # Update mtime
        Path(fctest.fontdir.name).touch()
        cache_stat1 = [f.stat() for f in fctest.cache_files()]
        fctest.logger.info(cache_files1)

    time.sleep(1)
    basedir = tempfile.TemporaryDirectory(prefix='fontconfig.',
                                          suffix='.base')
    with fctest.sandboxed(basedir.name) as f:
        for ret, stdout, stderr in f.run_match([]):
            assert ret == 0, stderr
            assert "Fontconfig warning" in stderr, stderr
            f.logger.info(stdout)
            f.logger.info(stderr)

    cache_stat2 = [f.stat() for f in fctest.cache_files()]
    cache_files2 = [f for f in fctest.cache_files()]
    assert len(cache_stat2) == 1, cache_stat2
    assert cache_files1 == cache_files2, cache_files2
    # ignore st_atime
    cmp_cache_before = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat1]
    cmp_cache_after = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat2]
    assert cmp_cache_before == cmp_cache_after


@pytest.mark.skipif(not not os.getenv('EXEEXT'), reason='not working on Win32')
@pytest.mark.skipif(not shutil.which('bwrap'), reason='No bwrap installed')
def test_gen_testcache_no_check(fctest, fcfont):
    testexe = Path(fctest.builddir) / 'test' / ('test-gen-testcache' + fctest._exeext)
    if not testexe.exists():
        testexe = Path(fctest.builddir) / 'test' / ('test_gen_testcache' + fctest._exeext)
        if not testexe.exists():
            raise RuntimeError('No test case for gen-testcache')

    fctest.env['FONTCONFIG_NO_CHECK_CACHE_VERSION'] = '1'
    fctest.setup()
    fctest.install_font(fcfont.fonts, '.')
    for ret, stdout, stderr in fctest.run(testexe, [fctest.fontdir.name]):
        assert ret == 0, stderr
        cache_files1 = [f for f in fctest.cache_files()]
        assert len(cache_files1) == 1, cache_files1
        time.sleep(1)
        # Update mtime
        Path(fctest.fontdir.name).touch()
        cache_stat1 = [f.stat() for f in fctest.cache_files()]
        fctest.logger.info(cache_files1)

    time.sleep(1)
    basedir = tempfile.TemporaryDirectory(prefix='fontconfig.',
                                          suffix='.base')
    with fctest.sandboxed(basedir.name) as f:
        for ret, stdout, stderr in f.run_match([]):
            assert ret == 0, stderr
            assert "Fontconfig warning" not in stderr, stderr
            f.logger.info(stdout)
            f.logger.info(stderr)

    cache_stat2 = [f.stat() for f in fctest.cache_files()]
    cache_files2 = [f for f in fctest.cache_files()]
    assert len(cache_stat2) == 1, cache_stat2
    assert cache_files1 == cache_files2, cache_files2
    # ignore st_atime
    cmp_cache_before = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat1]
    cmp_cache_after = [attrgetter('st_mode', 'st_ino', 'st_dev', 'st_nlink', 'st_uid', 'st_gid', 'st_size', 'st_mtime', 'st_ctime')(st) for st in cache_stat2]
    assert cmp_cache_before != cmp_cache_after
