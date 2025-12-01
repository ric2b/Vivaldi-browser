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


def test_basic(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts, '.')
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        out_expected = f.read()
        assert out == out_expected


def test_subdir(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts, 'a')
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        out_expected = f.read()
        assert out == out_expected

def test_subdir_with_cache(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts, 'a')
    for ret, stdout, stderr in fctest.run_cache([str(Path(fctest.fontdir.name) / 'a')]):
        assert ret == 0, stderr
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        out_expected = f.read()
        assert out == out_expected


def test_with_dotfiles(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts, '.')
    for f in Path(fctest.fontdir.name).glob('*.pcf'):
        f.rename(f.parent / ('.' + f.name))
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        out_expected = f.read()
        assert out == out_expected


def test_with_dotdir(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts, '.a')
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        out_expected = f.read()
        assert out == out_expected


def test_with_complicated_dir_structure(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts[0], Path('a') / 'a')
    fctest.install_font(fcfont.fonts[1], Path('b') / 'b')
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        out_expected = f.read()
        assert out == out_expected


def test_subdir_with_out_of_date_cache(fctest, fcfont):
    fctest.setup()
    fctest.install_font([], 'a')
    for ret, stdout, stderr in fctest.run_cache([str(Path(fctest.fontdir.name) / 'a')]):
        assert ret == 0, stderr
    time.sleep(1)
    fctest.install_font(fcfont.fonts, 'a')
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        out_expected = f.read()
        assert out == out_expected


def test_new_file_with_out_of_date_cache(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts[0], '.')
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr
    time.sleep(1)
    fctest.install_font(fcfont.fonts[1], 'a')
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        out_expected = f.read()
        assert out == out_expected


def test_keep_mtime(fctest, fcfont):
    fctest.setup()
    fctest.install_font(fcfont.fonts, '.', 0)
    fontdir = Path(fctest.fontdir.name)
    before = fontdir.stat().st_mtime
    time.sleep(1)
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr
    after = fontdir.stat().st_mtime
    assert before == after, f'mtime {before} was changed to {after}'


def test_multiple_caches(fctest, fcfont):
    extraconffile = NamedTemporaryFile(prefix='fontconfig.',
                                       suffix='.extra.conf',
                                       mode='w',
                                       delete_on_close=False)
    fctest._extra.append(f'<include ignore_missing="yes">{fctest.convert_path(extraconffile.name)}</include>')

    # Set up for generating original caches
    fctest.setup()
    origepoch = epoch = os.getenv('SOURCE_DATE_EPOCH')
    if epoch:
        # epoch 0 has special meaning. increase to avoid epoch 0
        epoch = int(epoch) + 1
    fctest.install_font(fcfont.fonts, '.', epoch)
    if epoch:
        fctest._env['SOURCE_DATE_EPOCH'] = str(epoch)
    for ret, stdout, stderr in fctest.run_cache([fctest.convert_path(fctest.fontdir.name)]):
        assert ret == 0, stderr
    time.sleep(1)

    cache_files1 = [f.stat() for f in fctest.cache_files()]
    assert len(cache_files1) == 1, cache_files1

    # Set up for modified caches
    oldcachedir = fctest.cachedir
    oldconffile = fctest._conffile
    newcachedir = TemporaryDirectory(prefix='fontconfig.',
                                     suffix='.newcachedir')
    newconffile = NamedTemporaryFile(prefix='fontconfig.',
                                     suffix='.new.conf',
                                     mode='w',
                                     delete_on_close=False)
    fctest._cachedir = newcachedir
    fctest._conffile = newconffile
    fctest.setup()

    extraconffile.write(f'''
<fontconfig>
  <match target="scan">
    <test name="file"><string>{fctest.convert_path(fctest.fontdir.name)}/4x6.pcf</string></test>
    <edit name="pixelsize"><int>8</int></edit>
  </match>
</fontconfig>''')
    extraconffile.close()
    if epoch:
        fctest._env['SOURCE_DATE_EPOCH'] = str(epoch + 1)
    for ret, stdout, stderr in fctest.run_cache([fctest.convert_path(fctest.fontdir.name)]):
        assert ret == 0, stderr
    if epoch:
        fctest._env['SOURCE_DATE_EPOCH'] = origepoch
    cache_files2 = [f.stat() for f in fctest.cache_files()]
    assert len(cache_files2) == 1, cache_files2
    # Make sure if 1 and 2 is different
    assert cache_files1 != cache_files2

    ## Set up for mixed caches
    mixedconffile = NamedTemporaryFile(prefix='fontconfig.',
                                       suffix='.mixed.conf',
                                       mode='w',
                                       delete_on_close=False)
    fctest._cachedir = oldcachedir
    fctest._conffile = mixedconffile
    fctest._extra.append(f'<cachedir>{fctest.convert_path(newcachedir.name)}</cachedir>')
    fctest.setup()
    l = []
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['=']
    for ret, stdout, stderr in fctest.run_list(['-', 'family', 'pixelsize']):
        assert ret == 0, stderr
        l += sorted(stdout.splitlines())
    l += ['']
    out = '\n'.join(l)
    with open(Path(fctest.builddir) / 'test' / 'out.expected') as f:
        s = f.read()
        out_expected = s.replace('pixelsize=6', 'pixelsize=8')
        assert out == out_expected

    del oldconffile


@pytest.mark.skipif(not not os.getenv('EXEEXT'), reason='not working on Win32')
def test_xdg_cache_home(fctest, fcfont):
    fctest._env['XDG_CACHE_HOME'] = ''
    old_home = os.getenv('HOME')
    new_home = TemporaryDirectory(prefix='fontconfig.',
                                  suffix='.home')
    fctest._env['HOME'] = new_home.name

    fctest.install_font(fcfont.fonts, '.')

    def custom_config(self):
        return f'<fontconfig><dir>{fctest.fontdir.name}</dir><cachedir prefix="xdg">fontconfig</cachedir></fontconfig>'

    fctest.config = types.MethodType(custom_config, fctest)
    fctest.setup()
    for ret, stdout, stderr in fctest.run_cache([fctest.fontdir.name]):
        assert ret == 0, stderr

    phome = Path(new_home.name)
    assert (phome / '.cache').exists()
    assert (phome / '.cache' / 'fontconfig').exists()
    cache_files = [f.name for f in (phome / '.cache' / 'fontconfig').glob('*cache*')]
    assert len(cache_files) == 1
