# Copyright (C) 2025 fontconfig Authors
# SPDX-License-Identifier: HPND

from contextlib import contextmanager
from itertools import chain
from pathlib import Path, PureWindowsPath
from tempfile import TemporaryDirectory, NamedTemporaryFile
from typing import Iterator, Self
import logging
import os
import re
import shutil
import subprocess
import sys


logging.basicConfig(level=logging.DEBUG)

class FcTest:

    def __init__(self):
        self._with_fontations = False
        self.logger = logging.getLogger()
        self._env = os.environ.copy()
        self._fontdir = TemporaryDirectory(prefix='fontconfig.',
                                           suffix='.host_fontdir')
        self._cachedir = TemporaryDirectory(prefix='fontconfig.',
                                            suffix='.host_cachedir')
        self._conffile = NamedTemporaryFile(prefix='fontconfig.',
                                            suffix='.host.conf',
                                            mode='w',
                                            delete_on_close=False)
        self._builddir = self._env.get('builddir', str(Path(__file__).parents[2] / 'build'))
        self._srcdir = self._env.get('srcdir', '.')
        self._exeext = self._env.get('EXEEXT',
                                     '.exe' if sys.platform == 'win32' else '')
        self._drive = PureWindowsPath(self._env.get('SystemDrive', '')).drive
        self._exewrapper = ''
        if self._exeext and sys.platform != 'win32':
            self._exewrapper = shutil.which('wine')
            if not self._exewrapper:
                raise RuntimeError('No runner available')
            self._drive = 'z:'
            cc = self._env.get('CC', 'cc')
            res = subprocess.run([cc, '-print-sysroot'], capture_output=True)
            sysroot = res.stdout.decode('utf-8').rstrip()
            if res.returncode != 0 or not sysroot:
                raise RuntimeError('Unable to get sysroot')
            sysroot = Path(sysroot) / 'mingw' / 'bin'
            self._env['WINEPATH'] = ';'.join(
                [
                    self.convert_path(self._builddir),
                    self.convert_path(sysroot)
                ])
        self._bwrap = shutil.which('bwrap')
        def bin_path(bin):
            fn = bin + self._exeext
            return self.convert_path(Path(self.builddir) / bin / fn)
        self._fccache = bin_path('fc-cache')
        self._fccat = bin_path('fc-cat')
        self._fclist = bin_path('fc-list')
        self._fcmatch = bin_path('fc-match')
        self._fcpattern = bin_path('fc-pattern')
        self._fcquery = bin_path('fc-query')
        self._fcscan = bin_path('fc-scan')
        self._fcvalidate = bin_path('fc-validate')
        self._extra = []
        self.__conf_templ = '''
        <fontconfig>
          {extra}
          <dir>{fontdir}</dir>
          <cachedir>{cachedir}</cachedir>
        </fontconfig>
        '''
        self._sandboxed = False

    def __del__(self):
        del self._conffile
        del self._fontdir
        del self._cachedir

    @property
    def builddir(self):
        return self._builddir

    @property
    def srcdir(self):
        return self._srcdir

    @property
    def fontdir(self):
        return self._fontdir

    @property
    def cachedir(self):
        return self._cachedir

    @property
    def extra(self):
        return '\n'.join(self._extra)

    @property
    def remapdir(self):
        return [x for x in self._extra if re.search(r'\b<remap-dir\b', x)]

    @property
    def with_fontations(self):
        return self._with_fontations

    @with_fontations.setter
    def with_fontations(self, v: bool) -> None:
        self._with_fontations = v

    @remapdir.setter
    def remapdir(self, v: str) -> None:
        self._extra = [x for x in self._extra if not re.search(r'\b<remap-dir\b', x)]
        self._extra += [f'<remap-dir as-path="{self.fontdir.name}">{v}</remap-dir>']

    @property
    def env(self):
        return self._env

    def config(self) -> str:
        return self.__conf_templ.format(fontdir=self.convert_path(self.fontdir.name),
                                        cachedir=self.convert_path(self.cachedir.name),
                                        extra=self.extra)

    def setup(self):
        if self._sandboxed:
            self.logger.info(self.config())
            self._remapped_conffile.write(self.config())
            self._remapped_conffile.close()
            conf = self._remapped_conffile.name
            try:
                fn = Path(self._remapped_conffile.name).relative_to(Path(self._builddir).resolve())
                conf = str(Path(self._remapped_builddir.name) / fn)
            except ValueError:
                pass
        else:
            self._conffile.write(self.config())
            self._conffile.close()
            conf = self._conffile.name

        self._env['FONTCONFIG_FILE'] = self.convert_path(conf)

    def install_font(self, files, dest, time=None):
        if not isinstance(files, list):
            files = [files]
        if not time:
            time = self._env.get('SOURCE_DATE_EPOCH', None)

        for f in files:
            fn = Path(f).name
            d = Path(dest)
            if d.is_absolute():
                dpath = d
            else:
                dpath = Path(self.fontdir.name) / dest
            dname = dpath / fn
            os.makedirs(str(dpath), exist_ok=True)
            shutil.copy2(f, dname)
            if time:
                os.utime(str(dname), (time, time))

        if time:
            os.utime(self.fontdir.name, (time, time))

    @contextmanager
    def sandboxed(self, remapped_basedir, bind=None) -> Self:
        if not self._bwrap:
            raise RuntimeError('No bwrap installed')
        self._remapped_fontdir = TemporaryDirectory(prefix='fontconfig.',
                                                    suffix='.fontdir',
                                                    dir=remapped_basedir,
                                                    delete=False)
        self._remapped_cachedir = TemporaryDirectory(prefix='fontconfig.',
                                                     suffix='.cachedir',
                                                     dir=remapped_basedir,
                                                     delete=False)
        self._remapped_builddir = TemporaryDirectory(prefix='fontconfig.',
                                                     suffix='.build',
                                                     dir=remapped_basedir,
                                                     delete=False)
        self._remapped_conffile = NamedTemporaryFile(prefix='fontconfig.',
                                                     suffix='.conf',
                                                     dir=Path(self._builddir) / 'test',
                                                     mode='w',
                                                     delete_on_close=False)
        self._basedir = remapped_basedir
        self.remapdir = self._remapped_fontdir.name
        self._orig_cachedir = self.cachedir
        self._cachedir = self._remapped_cachedir
        self._sandboxed = True
        dummy = TemporaryDirectory(prefix='fontconfig.')
        # Set same mtime to dummy directory to avoid updating cache
        # because of mtime
        st = Path(self.fontdir.name).stat()
        os.utime(str(dummy.name), (st.st_mtime, st.st_mtime))
        # Set dummy dir as <dir>
        orig_fontdir = self.fontdir
        self._fontdir = dummy
        self.setup()
        self._fontdir = orig_fontdir
        base_bind = {
                self._orig_cachedir.name: self._remapped_cachedir.name,
                self._builddir: self._remapped_builddir.name,
        }
        if not bind:
            bind = base_bind | {
                self.fontdir.name: self._remapped_fontdir.name,
            }
        else:
            bind = base_bind | bind
        b = [('--bind', x, y) for x, y in bind.items()]
        self.__bind = list(chain.from_iterable(i for i in b))
        try:
            yield self
        finally:
            self._cachedir = self._orig_cachedir
            del self._remapped_conffile
            self._sandboxed = False
            self._remapped_builddir = None
            self._remapped_cachedir = None
            self._remapped_conffile = None
            self._remapped_fontdir = None
            self._orig_cachedir = None
            self.remapdir = None
            self._basedir = None
            self.__bind = None
            self._env['FONTCONFIG_FILE'] = self._conffile.name

    def run(self, binary, args=[], debug=False) -> Iterator[[int, str, str]]:
        cmd = []
        if self._exewrapper:
            cmd += [self._exewrapper]
        cmd += [str(binary)]
        cmd += args
        if self._sandboxed:
            boxed = [self._bwrap, '--ro-bind', '/', '/',
                     '--dev-bind', '/dev', '/dev',
                     '--proc', '/proc',
                     # Use fresh tmpfs to avoid unexpected references
                     '--tmpfs', '/tmp',
                     '--setenv', 'FONTCONFIG_FILE', self._env['FONTCONFIG_FILE']]
            boxed += self.__bind
            if debug:
                boxed += ['--setenv', 'FC_DEBUG', str(debug)]
            if self.with_fontations:
                boxed += ['--setenv', 'FC_FONTATIONS', '1']
            boxed += cmd
            self.logger.info(boxed)
            res = subprocess.run(boxed, capture_output=True,
                                 env=self._env)
        else:
            origdebug = self._env.get('FC_DEBUG')
            if debug:
                self._env['FC_DEBUG'] = str(debug)
            origfontations = self._env.get('FC_FONTATIONS')
            if self.with_fontations:
                self._env['FC_FONTATIONS'] = '1'
            self.logger.info(cmd)
            res = subprocess.run(cmd, capture_output=True,
                                 env=self._env)
            if debug:
                if origdebug:
                    self._env['FC_DEBUG'] = origdebug
                else:
                    del self._env['FC_DEBUG']
            if self.with_fontations:
                if origfontations:
                    self._env['FC_FONTATIONS'] = origfontations
                else:
                    del self._env['FC_FONTATIONS']
        yield res.returncode, res.stdout.decode('utf-8'), res.stderr.decode('utf-8')

    def run_cache(self, args, debug=False) -> Iterator[[int, str, str]]:
        return self.run(self._fccache, args, debug)

    def run_cat(self, args, debug=False) -> Iterator[[int, str, str]]:
        return self.run(self._fccat, args, debug)

    def run_list(self, args, debug=False) -> Iterator[[int, str, str]]:
        return self.run(self._fclist, args, debug)

    def run_match(self, args, debug=False) -> Iterator[[int, str, str]]:
        return self.run(self._fcmatch, args, debug)

    def run_pattern(self, args, debug=False) -> Iterator[[int, str, str]]:
        return self.run(self._fcpattern, args, debug)

    def run_query(self, args, debug=False) -> Iterator[[int, str, str]]:
        return self.run(self._fcquery, args, debug)

    def run_scan(self, args, debug=False) -> Iterator[[int, str, str]]:
        return self.run(self._fcscan, args, debug)

    def run_validate(self, args, debug=False) -> Iterator[[int, str, str]]:
        return self.run(self._fcvalidate, args, debug)

    def cache_files(self) -> Iterator[Path]:
        for c in Path(self.cachedir.name).glob('*cache*'):
            yield c

    def convert_path(self, path) -> str:
        winpath = PureWindowsPath(path)
        if not winpath.drive and self._drive:
            return str(PureWindowsPath(self._drive) / '/' / winpath).replace('\\', '/')
        return path


class FcTestFont:

    def __init__(self, srcdir='.'):
        self._fonts = []
        p = Path(srcdir)
        if (p / 'test').exists():
            p = p / 'test'
        if not (p / '4x6.pcf').exists():
            raise RuntimeError('No 4x6.pcf available.')
        else:
            self._fonts.append(p / '4x6.pcf')
        if not (p / '8x16.pcf').exists():
            raise RuntimeError('No 8x16.pcf available.')
        else:
            self._fonts.append(p / '8x16.pcf')

    @property
    def fonts(self):
        return self._fonts


class FcExternalTestFont:

    def __init__(self):
        fctest = FcTest()
        self._fonts = [str(fn) for fn in (Path(fctest.builddir) / "testfonts").glob('**/*.ttf')]

    @property
    def fonts(self):
        return self._fonts


if __name__ == '__main__':
    f = FcTest()
    print(f.fontdir.name)
    print(f.cachedir.name)
    print(f._conffile.name)
    print(f.config())
    f.setup()
    f = FcExternalTestFont()
    print(f.fonts)
