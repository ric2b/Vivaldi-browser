#!/usr/bin/env python3
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script (intended to be invoked by autoninja or autoninja.bat) detects
whether a build is accelerated using a service like goma. If so, it runs with a
large -j value, and otherwise it chooses a small one. This auto-adjustment
makes using remote build acceleration simpler and safer, and avoids errors that
can cause slow goma builds or swap-storms on unaccelerated builds.

autoninja tries to detect relevant build settings such as use_remoteexec, and it
does handle import statements, but it can't handle conditional setting of build
settings.
"""

import multiprocessing
import os
import platform
import re
import shlex
import subprocess
import sys

import autosiso
import ninja
import ninja_reclient
import siso

if sys.platform in ['darwin', 'linux']:
    import resource

SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))

# See [1] and [2] for the painful details of this next section, which handles
# escaping command lines so that they can be copied and pasted into a cmd
# window.
#
# pylint: disable=line-too-long
# [1] https://learn.microsoft.com/en-us/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way # noqa
# [2] https://web.archive.org/web/20150815000000*/https://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/set.mspx # noqa
_UNSAFE_FOR_CMD = set('^<>&|()%')
_ALL_META_CHARS = _UNSAFE_FOR_CMD.union(set('"'))


def _quote_for_cmd(arg):
    # First, escape the arg so that CommandLineToArgvW will parse it properly.
    if arg == '' or ' ' in arg or '"' in arg:
        quote_re = re.compile(r'(\\*)"')
        arg = '"%s"' % (quote_re.sub(lambda mo: 2 * mo.group(1) + '\\"', arg))

    # Then check to see if the arg contains any metacharacters other than
    # double quotes; if it does, quote everything (including the double
    # quotes) for safety.
    if any(a in _UNSAFE_FOR_CMD for a in arg):
        arg = ''.join('^' + a if a in _ALL_META_CHARS else a for a in arg)
    return arg


def _print_cmd(cmd):
    shell_quoter = shlex.quote
    if sys.platform.startswith('win'):
        shell_quoter = _quote_for_cmd
    print(*[shell_quoter(arg) for arg in cmd], file=sys.stderr)


def _gn_lines(output_dir, path):
    """
    Generator function that returns args.gn lines one at a time, following
    import directives as needed.
    """
    import_re = re.compile(r'\s*import\("(.*)"\)')
    with open(path, encoding='utf-8') as f:
        for line in f:
            match = import_re.match(line)
            if match:
                raw_import_path = match.groups()[0]
                if raw_import_path[:2] == "//":
                    import_path = os.path.normpath(
                        os.path.join(output_dir, '..', '..',
                                     raw_import_path[2:]))
                else:
                    import_path = os.path.normpath(
                        os.path.join(os.path.dirname(path), raw_import_path))
                for import_line in _gn_lines(output_dir, import_path):
                    yield import_line
            else:
                yield line


def main(args):
    # The -t tools are incompatible with -j
    t_specified = False
    j_specified = False
    offline = False
    output_dir = '.'
    input_args = args
    summarize_build = os.environ.get('NINJA_SUMMARIZE_BUILD') == '1'
    # On Windows the autoninja.bat script passes along the arguments enclosed in
    # double quotes. This prevents multiple levels of parsing of the special '^'
    # characters needed when compiling a single file but means that this script
    # gets called with a single argument containing all of the actual arguments,
    # separated by spaces. When this case is detected we need to do argument
    # splitting ourselves. This means that arguments containing actual spaces
    # are not supported by autoninja, but that is not a real limitation.
    if (sys.platform.startswith('win') and len(args) == 2
            and input_args[1].count(' ') > 0):
        input_args = args[:1] + args[1].split()

    # Ninja uses getopt_long, which allow to intermix non-option arguments.
    # To leave non supported parameters untouched, we do not use getopt.
    for index, arg in enumerate(input_args[1:]):
        if arg.startswith('-j'):
            j_specified = True
        if arg.startswith('-t'):
            t_specified = True
        if arg == '-C':
            # + 1 to get the next argument and +1 because we trimmed off
            # input_args[0]
            output_dir = input_args[index + 2]
        elif arg.startswith('-C'):
            # Support -Cout/Default
            output_dir = arg[2:]
        elif arg in ('-o', '--offline'):
            offline = True
        elif arg == '-h':
            print('autoninja: Use -o/--offline to temporary disable goma.',
                  file=sys.stderr)
            print(file=sys.stderr)

    use_goma = False
    use_remoteexec = False
    use_rbe = False
    use_siso = False

    # Attempt to auto-detect remote build acceleration. We support gn-based
    # builds, where we look for args.gn in the build tree, and cmake-based
    # builds where we look for rules.ninja.
    if os.path.exists(os.path.join(output_dir, 'args.gn')):
        for line in _gn_lines(output_dir, os.path.join(output_dir, 'args.gn')):
            # use_goma, use_remoteexec, or use_rbe will activate build
            # acceleration.
            #
            # This test can match multi-argument lines. Examples of this
            # are: is_debug=false use_goma=true is_official_build=false
            # use_goma=false# use_goma=true This comment is ignored
            #
            # Anything after a comment is not consider a valid argument.
            line_without_comment = line.split('#')[0]
            if re.search(r'(^|\s)(use_goma)\s*=\s*true($|\s)',
                         line_without_comment):
                use_goma = True
                continue
            if re.search(r'(^|\s)(use_remoteexec)\s*=\s*true($|\s)',
                         line_without_comment):
                use_remoteexec = True
                continue
            if re.search(r'(^|\s)(use_rbe)\s*=\s*true($|\s)',
                         line_without_comment):
                use_rbe = True
                continue
            if re.search(r'(^|\s)(use_siso)\s*=\s*true($|\s)',
                         line_without_comment):
                use_siso = True
                continue

        siso_marker = os.path.join(output_dir, '.siso_deps')
        if use_siso:
            # autosiso generates a .ninja_log file so the mere existence of a
            # .ninja_log file doesn't imply that a ninja build was done. However
            # if there is a .ninja_log but no .siso_deps then that implies a
            # ninja build.
            ninja_marker = os.path.join(output_dir, '.ninja_log')
            if os.path.exists(ninja_marker) and not os.path.exists(siso_marker):
                print('Run gn clean before switching from ninja to siso in %s' %
                      output_dir,
                      file=sys.stderr)
                return 1
            if use_goma:
                print('Siso does not support Goma.', file=sys.stderr)
                print('Do not use use_siso=true and use_goma=true',
                      file=sys.stderr)
                return 1
            if use_remoteexec:
                return autosiso.main(['autosiso'] + input_args[1:])
            return siso.main(['siso', 'ninja'] + input_args[1:])

        if os.path.exists(siso_marker):
            print('Run gn clean before switching from siso to ninja in %s' %
                  output_dir,
                  file=sys.stderr)
            return 1

    else:
        for relative_path in [
                '',  # GN keeps them in the root of output_dir
                'CMakeFiles'
        ]:
            path = os.path.join(output_dir, relative_path, 'rules.ninja')
            if os.path.exists(path):
                with open(path, encoding='utf-8') as file_handle:
                    for line in file_handle:
                        if re.match(r'^\s*command\s*=\s*\S+gomacc', line):
                            use_goma = True
                            break

    # Strip -o/--offline so ninja doesn't see them.
    input_args = [arg for arg in input_args if arg not in ('-o', '--offline')]

    # If GOMA_DISABLED is set to "true", "t", "yes", "y", or "1"
    # (case-insensitive) then gomacc will use the local compiler instead of
    # doing a goma compile. This is convenient if you want to briefly disable
    # goma. It avoids having to rebuild the world when transitioning between
    # goma/non-goma builds. However, it is not as fast as doing a "normal"
    # non-goma build because an extra process is created for each compile step.
    # Checking this environment variable ensures that autoninja uses an
    # appropriate -j value in this situation.
    goma_disabled_env = os.environ.get('GOMA_DISABLED', '0').lower()
    if offline or goma_disabled_env in ['true', 't', 'yes', 'y', '1']:
        use_goma = False

    if use_goma:
        gomacc_file = 'gomacc.exe' if sys.platform.startswith(
            'win') else 'gomacc'
        goma_dir = os.environ.get('GOMA_DIR',
                                  os.path.join(SCRIPT_DIR, '.cipd_bin'))
        gomacc_path = os.path.join(goma_dir, gomacc_file)
        # Don't invoke gomacc if it doesn't exist.
        if os.path.exists(gomacc_path):
            # Check to make sure that goma is running. If not, don't start the
            # build.
            status = subprocess.call([gomacc_path, 'port'],
                                     stdout=subprocess.PIPE,
                                     stderr=subprocess.PIPE,
                                     shell=False)
            if status == 1:
                print(
                    'Goma is not running. Use "goma_ctl ensure_start" to start '
                    'it.',
                    file=sys.stderr)
                if sys.platform.startswith('win'):
                    # Set an exit code of 1 in the batch file.
                    print('cmd "/c exit 1"')
                else:
                    # Set an exit code of 1 by executing 'false' in the bash
                    # script.
                    print('false')
                sys.exit(1)

    # A large build (with or without goma) tends to hog all system resources.
    # Depending on the operating system, we might have mechanisms available
    # to run at a lower priority, which improves this situation.
    if os.environ.get('NINJA_BUILD_IN_BACKGROUND') == '1':
        if sys.platform in ['darwin', 'linux']:
            # nice-level 10 is usually considered a good default for background
            # tasks. The niceness is inherited by child processes, so we can
            # just set it here for us and it'll apply to the build tool we
            # spawn later.
            os.nice(10)

    # Tell goma or reclient to do local compiles.
    if offline:
        os.environ['RBE_remote_disabled'] = '1'
        os.environ['GOMA_DISABLED'] = '1'

    # On macOS and most Linux distributions, the default limit of open file
    # descriptors is too low (256 and 1024, respectively).
    # This causes a large j value to result in 'Too many open files' errors.
    # Check whether the limit can be raised to a large enough value. If yes,
    # use `ulimit -n .... &&` as a prefix to increase the limit when running
    # ninja.
    if sys.platform in ['darwin', 'linux']:
        # Increase the number of allowed open file descriptors to the maximum.
        fileno_limit, hard_limit = resource.getrlimit(resource.RLIMIT_NOFILE)
        if fileno_limit < hard_limit:
            try:
                resource.setrlimit(resource.RLIMIT_NOFILE,
                                   (hard_limit, hard_limit))
            except Exception:
                pass
            fileno_limit, hard_limit = resource.getrlimit(
                resource.RLIMIT_NOFILE)

    # Call ninja.py so that it can find ninja binary installed by DEPS or one in
    # PATH.
    ninja_path = os.path.join(SCRIPT_DIR, 'ninja.py')
    # If using remoteexec, use ninja_reclient.py which wraps ninja.py with
    # starting and stopping reproxy.
    if use_remoteexec:
        ninja_path = os.path.join(SCRIPT_DIR, 'ninja_reclient.py')

    args = [sys.executable, ninja_path] + input_args[1:]

    num_cores = multiprocessing.cpu_count()
    if not j_specified and not t_specified:
        if not offline and (use_goma or use_remoteexec or use_rbe):
            args.append('-j')
            default_core_multiplier = 80
            if platform.machine() in ('x86_64', 'AMD64'):
                # Assume simultaneous multithreading and therefore half as many
                # cores as logical processors.
                num_cores //= 2

            core_multiplier = int(
                os.environ.get('NINJA_CORE_MULTIPLIER',
                               default_core_multiplier))
            j_value = num_cores * core_multiplier

            core_limit = int(os.environ.get('NINJA_CORE_LIMIT', j_value))
            j_value = min(j_value, core_limit)

            # On Windows, a -j higher than 1000 doesn't improve build times.
            # On macOS, ninja is limited to at most FD_SETSIZE (1024) open file
            # descriptors.
            if sys.platform in ['darwin', 'win32']:
                j_value = min(j_value, 1000)

            # Use a j value that reliably works with the open file descriptors
            # limit.
            if sys.platform in ['darwin', 'linux']:
                j_value = min(j_value, int(fileno_limit * 0.8))

            args.append('%d' % j_value)
        else:
            j_value = num_cores
            # Ninja defaults to |num_cores + 2|
            j_value += int(os.environ.get('NINJA_CORE_ADDITION', '2'))
            args.append('-j')
            args.append('%d' % j_value)

    if summarize_build:
        # Enable statistics collection in Ninja.
        args += ['-d', 'stats']
        # Print the command-line to reassure the user that the right settings
        # are being used.
        _print_cmd(args)

    if use_remoteexec:
        return ninja_reclient.main(args[1:])
    return ninja.main(args[1:])


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv))
    except KeyboardInterrupt:
        sys.exit(1)
