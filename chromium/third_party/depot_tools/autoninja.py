#!/usr/bin/env python3
# Copyright (c) 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
This script (intended to be invoked by autoninja or autoninja.bat) detects
whether a build is accelerated using a service like RBE. If so, it runs with a
large -j value, and otherwise it chooses a small one. This auto-adjustment
makes using remote build acceleration simpler and safer, and avoids errors that
can cause slow RBE builds, or swap-storms on unaccelerated builds.

autoninja tries to detect relevant build settings such as use_remoteexec, and it
does handle import statements, but it can't handle conditional setting of build
settings.
"""

import importlib.util
import multiprocessing
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import time
import uuid
import warnings

import android_build_server_helper
import build_telemetry
import gclient_paths
import gclient_utils
import gn_helper
import ninja
import ninjalog_uploader
import reclient_helper
import siso


_SISO_SUGGESTION = """You're still using Ninja.
Please run 'gn clean {output_dir}' when convenient to
upgrade this output directory to Siso (Chromium’s Ninja replacement).
If you run into any issues by switching to Siso, please file a bug via
go/siso-bug and switch back temporarily by setting the GN arg
'use_siso = false'"""

_SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
_NINJALOG_UPLOADER = os.path.join(_SCRIPT_DIR, "ninjalog_uploader.py")

# See [1] and [2] for the painful details of this next section, which handles
# escaping command lines so that they can be copied and pasted into a cmd
# window.
#
# pylint: disable=line-too-long
# [1] https://learn.microsoft.com/en-us/archive/blogs/twistylittlepassagesallalike/everyone-quotes-command-line-arguments-the-wrong-way # noqa
# [2] https://web.archive.org/web/20150815000000*/https://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/set.mspx # noqa
_UNSAFE_FOR_CMD = set("^<>&|()%")
_ALL_META_CHARS = _UNSAFE_FOR_CMD.union(set('"'))

_HELP_MESSAGE = """\
autoninja:
  -o/--offline  temporary disable remote execution
"""


def _import_from_path(module_name, file_path):
    try:
        spec = importlib.util.spec_from_file_location(module_name, file_path)
        module = importlib.util.module_from_spec(spec)
        sys.modules[module_name] = module
        spec.loader.exec_module(module)
    except:
        raise ImportError(
            'Could not import module "{}" from "{}"'.format(
                module_name, file_path),
            name=module_name,
            path=file_path,
        )
    return module


def _is_google_corp_machine():
    """This assumes that corp machine has gcert binary in known location."""
    return shutil.which("gcert") is not None


def _has_internal_checkout():
    """Check if internal is checked out."""
    root_dir = gclient_paths.GetPrimarySolutionPath()
    if not root_dir:
        return False
    return os.path.exists(os.path.join(root_dir, "internal", ".git"))


def _reclient_rbe_project():
    """Returns RBE project used by reclient."""
    instance = os.environ.get('RBE_instance')
    if instance:
        m = re.match(instance, r'projects/([^/]*)/instances/.*')
        if m:
            return m[1]
    reproxy_cfg_path = reclient_helper.find_reclient_cfg()
    if not reproxy_cfg_path:
        return ""
    with open(reproxy_cfg_path) as f:
        for line in f:
            m = re.match(r'instance\s*=\s*projects/([^/]*)/instances/.*', line)
            if m:
                return m[1]
    return ""


def _siso_rbe_project():
    """Returns RBE project used by siso."""
    siso_project = os.environ.get('SISO_PROJECT')
    if siso_project:
        return siso_project
    root_dir = gclient_paths.GetPrimarySolutionPath()
    if not root_dir:
        return ""
    sisoenv_path = os.path.join(root_dir, 'build/config/siso/.sisoenv')
    if not os.path.exists(sisoenv_path):
        return ""
    with open(sisoenv_path) as f:
        for line in f:
            m = re.match(r'SISO_PROJECT=\s*(\S*)\s*', line)
            if m:
                return m[1]
    return ""


def _quote_for_cmd(arg):
    # First, escape the arg so that CommandLineToArgvW will parse it properly.
    if arg == "" or " " in arg or '"' in arg:
        quote_re = re.compile(r'(\\*)"')
        arg = '"%s"' % (quote_re.sub(lambda mo: 2 * mo.group(1) + '\\"', arg))

    # Then check to see if the arg contains any metacharacters other than
    # double quotes; if it does, quote everything (including the double
    # quotes) for safety.
    if any(a in _UNSAFE_FOR_CMD for a in arg):
        arg = "".join("^" + a if a in _ALL_META_CHARS else a for a in arg)
    return arg


def _print_cmd(cmd):
    shell_quoter = shlex.quote
    if sys.platform.startswith("win"):
        shell_quoter = _quote_for_cmd
    print(*[shell_quoter(arg) for arg in cmd], file=sys.stderr)


def _get_remoteexec_defaults():
    root_dir = gclient_paths.GetPrimarySolutionPath()
    if not root_dir:
        return None
    default_file = os.path.join(root_dir,
                                "build/toolchain/remoteexec_defaults.gni")
    values = {
        "use_reclient_on_siso": True,
        "use_reclient_on_ninja": True,
    }
    if not os.path.exists(default_file):
        return values
    pattern = re.compile(r"(^|\s*)([^=\s]*)\s*=\s*(\S*)\s*$")
    with open(default_file, encoding="utf-8") as f:
        for line in f:
            line = line.split("#")[0]
            m = pattern.match(line)
            if not m:
                continue
            values[m.group(2)] = m.group(3) == "true"
    return values


# `use_siso` value is used to determine whether siso or ninja is used,
# and used to determine default value of `use_reclient`, so
# this logic should match with //build/toolchain/siso.gni
def _get_use_siso_default(output_dir):
    """Returns use_siso default value."""
    root_dir = gclient_paths.GetPrimarySolutionPath()
    if not root_dir:
        return False

    # autoninja requires .sisoenv to set Siso env vars such as SISO_PROJECT
    # and SISO_REAPI_INSTANCE.
    sisoenv_path = os.path.join(root_dir, "build/config/siso/.sisoenv")
    if not os.path.exists(sisoenv_path):
        return False

    # Check the project wide default in `.gn`.
    dot_gn = os.path.join(root_dir, ".gn")
    if os.path.exists(dot_gn):
        with open(dot_gn) as f:
            dot_gn_lines = f.readlines()
        p = re.compile(r"(^|\s*)(use_siso)\s*=\s*(true)\s*$")
        if any(p.match(l) for l in dot_gn_lines):
            return True

    # Checking `build_with_chromium` var in //build/config/gclient_args.gni
    # to enable Siso on Chromium.
    # TODO: crbug.com/409223168 - Remove this condition after setting use_siso
    # in Chromium's .gn + some buffer.
    gclient_args_gni = os.path.join(root_dir, "build/config/gclient_args.gni")
    if os.path.exists(gclient_args_gni):
        with open(gclient_args_gni) as f:
            if "build_with_chromium = true" in f.read():
                return True

    # This output directory is already using Siso.
    if os.path.exists(os.path.join(output_dir, ".siso_deps")):
        return True

    return False


# Convert Ninja's -j flag to Siso's -local_jobs, -remote_jobs.
def _convert_ninja_j_to_siso_flags(j_value, use_remoteexec, args):
    local_jobs = None
    remote_jobs = None
    if use_remoteexec:
        remote_jobs = j_value
    else:
        num_cpus = multiprocessing.cpu_count()
        if int(j_value) <= num_cpus:
            local_jobs = j_value
        else:
            print(
                "WARNING: Ignoring -j %s because it is larger than "
                "num_cpus=%d. Use -local_jobs=%s instead if it's intentional." %
                (j_value, num_cpus, j_value),
                file=sys.stderr,
            )
    # replace -j with -local_jobs,-remote_jobs.
    return_args = []
    j_value_index = None
    for i in range(len(args)):
        arg = args[i]
        if arg.startswith('-j'):
            if arg == '-j':
                j_value_index = i + 1
            if local_jobs:
                return_args.extend(['-local_jobs=' + local_jobs])
            if remote_jobs:
                return_args.extend(['-remote_jobs=' + remote_jobs])
            continue
        if i == j_value_index:
            continue
        return_args.append(arg)
    return return_args


def _main_inner(input_args, build_id):
    # if user doesn't set PYTHONPYCACHEPREFIX and PYTHONDONTWRITEBYTECODE
    # set PYTHONDONTWRITEBYTECODE=1 not to create many *.pyc in workspace
    # and keep workspace clean.
    if not os.environ.get("PYTHONPYCACHEPREFIX"):
        os.environ.setdefault("PYTHONDONTWRITEBYTECODE", "1")
    # Workaround for reproxy timing out on startup due to the Google Cloud
    # Go SDK making a call to user.Current(), which can be slow on Googler
    # machines due to go.dev/issue/68312. This can be removed once Go 1.24
    # has been released, and reproxy + other tools have been rebuilt with
    # that.
    if _is_google_corp_machine():
        os.environ.setdefault("GOOGLE_API_USE_CLIENT_CERTIFICATE", "false")
    # The -t tools are incompatible with -j
    t_specified = False
    j_value = None
    offline = False
    output_dir = "."
    summarize_build = os.environ.get("NINJA_SUMMARIZE_BUILD") == "1"
    project = None

    # Ninja uses getopt_long, which allow to intermix non-option arguments.
    # To leave non supported parameters untouched, we do not use getopt.
    for index, arg in enumerate(input_args[1:]):
        if arg.startswith("-j"):
            if arg == "-j":
                j_value = input_args[index + 2]
            else:
                j_value = arg[2:]
        if arg.startswith("-t"):
            t_specified = True
        if arg == "-C":
            # + 1 to get the next argument and +1 because we trimmed off
            # input_args[0]
            output_dir = input_args[index + 2]
        elif arg.startswith("-C"):
            # Support -Cout/Default
            output_dir = arg[2:]
        elif arg in ("-o", "--offline"):
            offline = True
        elif arg in ("--project", "-project"):
            project = input_args[index + 2]
        elif arg.startswith("--project="):
            project = arg[len("--project="):]
        elif arg.startswith("-project="):
            project = arg[len("-project="):]
        elif arg in ("-h", "--help"):
            print(_HELP_MESSAGE, file=sys.stderr)

    is_android = False
    use_remoteexec = False
    use_reclient = None
    use_siso = None
    use_android_build_server = None

    # Attempt to auto-detect remote build acceleration. We support gn-based
    # builds, where we look for args.gn in the build tree, and cmake-based
    # builds where we look for rules.ninja.
    if gn_helper.exists(output_dir):
        for k, v in gn_helper.args(output_dir):
            # use_remoteexec will activate build acceleration.
            #
            # This test can match multi-argument lines. Examples of this
            # are: is_debug=false use_remoteexec=true is_official_build=false
            # use_remoteexec=false# use_remoteexec=true This comment is ignored
            #
            # Anything after a comment is not consider a valid argument.
            if k == "use_remoteexec" and v == "true":
                use_remoteexec = True
                continue
            if k == "use_remoteexec" and v == "false":
                use_remoteexec = False
                continue
            if k == "use_siso" and v == "true":
                use_siso = True
                continue
            if k == "use_siso" and v == "false":
                use_siso = False
                continue
            if k == "use_reclient" and v == "true":
                use_reclient = True
                continue
            if k == "use_reclient" and v == "false":
                use_reclient = False
                continue
            if k == "target_os" and v == '"android"':
                is_android = True
                continue
            if k == "android_static_analysis" and v != '"build_server"':
                use_android_build_server = False
                continue

        if use_siso is None:
            use_siso = _get_use_siso_default(output_dir)

            # If use_siso is True by default, but the output directory is still using
            # Ninja, print the suggestion message.
            is_ninja_used = os.path.exists(
                os.path.join(output_dir, ".ninja_deps"))
            if use_siso and is_ninja_used:
                print(_SISO_SUGGESTION.format(output_dir=output_dir),
                      file=sys.stderr)
                use_siso = False

        if use_reclient is None and use_remoteexec:
            if values := _get_remoteexec_defaults():
                if use_siso:
                    use_reclient = values["use_reclient_on_siso"]
                else:
                    use_reclient = values["use_reclient_on_ninja"]

    # Use the server for target_os="android" (where it is relevant), unless it
    # is disabled via GN arg.
    if use_android_build_server is None and is_android:
        use_android_build_server = True

    if use_remoteexec:
        if use_reclient:
            project = _reclient_rbe_project()
        elif use_siso and project is None:
            # siso runs locally if empty project is given
            # even if use_remoteexec=true is set.
            project = _siso_rbe_project()

        if _has_internal_checkout():
            # user may login on non-@google.com account on corp,
            # but need to use @google.com and rbe-chrome-untrusted
            # with src-internal.
            if project == 'rbe-chromium-untrusted':
                print(
                    "You can't use rbe-chromium-untrusted for "
                    "src-internal.\n"
                    "Please use rbe-chrome-untrusted and @google.com "
                    "account instead to build chrome.\n",
                    file=sys.stderr,
                )
                return 1
        elif not _is_google_corp_machine():
            # only @google.com is allowed to use rbe-chrome-untrusted
            # and use @google.com on non-corp machine is not allowed
            # by corp security policy.
            if project == 'rbe-chrome-untrusted':
                print(
                    "You can't use rbe-chrome-untrusted on non-corp "
                    "machine.\n"
                    "Plase use rbe-chromium-untrusted and non-@google.com "
                    "account instead to build chromium.",
                    file=sys.stderr,
                )
                return 1

    # If --offline is set, then reclient will use the local compiler
    # instead of doing a remote compile. This is convenient if you want
    # to briefly disable remote compile. It avoids having to rebuild the
    # world when transitioning between RBE/non-RBE builds. However, it is
    # not as fast as doing a "normal" non-RBE build because an extra
    # process is created for each compile step.
    if offline and use_reclient:
        # Tell reclient to do local compiles.
        os.environ["RBE_remote_disabled"] = "1"

    if gclient_utils.IsEnvCog():
        if not use_remoteexec or use_reclient or not use_siso:
            print(
                "WARNING: You're not using Siso's built-in remote "
                "execution. The build will be slow.\n"
                "You should set the following in args.gn to get better "
                "performance:\n"
                "  use_remoteexec=true\n"
                "  use_reclient=false\n"
                "  use_siso=true\n",
                file=sys.stderr,
            )

    # use_siso may not be set when running `autoninja -help` without `-C`.
    if use_siso is None:
        use_siso = _get_use_siso_default(output_dir)

    siso_marker = os.path.join(output_dir, ".siso_deps")
    if use_siso:
        # siso generates a .ninja_log file so the mere existence of a
        # .ninja_log file doesn't imply that a ninja build was done. However
        # if there is a .ninja_log but no .siso_deps then that implies a
        # ninja build.
        ninja_marker = os.path.join(output_dir, ".ninja_log")
        if os.path.exists(ninja_marker) and not os.path.exists(siso_marker):
            print(
                "Run gn clean before switching from ninja to siso in %s" %
                output_dir,
                file=sys.stderr,
            )
            return 1

        if j_value:
            input_args = _convert_ninja_j_to_siso_flags(j_value, use_remoteexec,
                                                        input_args)

        # Build ID consistently used in other tools. e.g. Reclient, ninjalog.
        os.environ.setdefault("SISO_BUILD_ID", build_id)
        with android_build_server_helper.build_server_context(
                build_id,
                output_dir,
                use_android_build_server=use_android_build_server):

            def run_siso(args):
                if summarize_build:
                    # Print the command-line to reassure the user that the right
                    # settings are being used.
                    _print_cmd(args)
                return siso.main(args)
            if use_remoteexec:
                if use_reclient and not t_specified:
                    # TODO: crbug.com/379584977 - Remove siso/reclient
                    # integration.
                    return reclient_helper.run_siso([
                        'siso',
                        'ninja',
                        # Do not authenticate when using Reproxy.
                        '-project=',
                        '-reapi_instance=',
                        '-reapi_address=',
                    ] + input_args[1:])
                return run_siso(["siso", "ninja"] + input_args[1:])
            return run_siso(["siso", "ninja", "--offline"] + input_args[1:])

    if os.path.exists(siso_marker):
        print(
            "Run gn clean before switching from siso to ninja in %s" %
            output_dir,
            file=sys.stderr,
        )
        return 1

    # Strip -o/--offline so ninja doesn't see them.
    input_args = [arg for arg in input_args if arg not in ("-o", "--offline")]

    # A large build (with or without RBE) tends to hog all system resources.
    # Depending on the operating system, we might have mechanisms available
    # to run at a lower priority, which improves this situation.
    if os.environ.get("NINJA_BUILD_IN_BACKGROUND") == "1":
        if sys.platform in ["darwin", "linux"]:
            # nice-level 10 is usually considered a good default for background
            # tasks. The niceness is inherited by child processes, so we can
            # just set it here for us and it'll apply to the build tool we
            # spawn later.
            os.nice(10)

    # On macOS and most Linux distributions, the default limit of open file
    # descriptors is too low (256 and 1024, respectively).
    # This causes a large j value to result in 'Too many open files' errors.
    # Check whether the limit can be raised to a large enough value. If yes,
    # use `ulimit -n .... &&` as a prefix to increase the limit when running
    # ninja.
    if sys.platform in ["darwin", "linux"]:
        import resource
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

    ninja_args = ['ninja']
    num_cores = multiprocessing.cpu_count()
    if not j_value and not t_specified:
        if not offline and use_remoteexec:
            ninja_args.append("-j")
            default_core_multiplier = 80
            if platform.machine() in ("x86_64", "AMD64"):
                # Assume simultaneous multithreading and therefore half as many
                # cores as logical processors.
                num_cores //= 2

            core_multiplier = int(
                os.environ.get("NINJA_CORE_MULTIPLIER",
                               default_core_multiplier))
            j_value = num_cores * core_multiplier

            core_limit = int(os.environ.get("NINJA_CORE_LIMIT", j_value))
            j_value = min(j_value, core_limit)

            # On Windows, a -j higher than 1000 doesn't improve build times.
            # On macOS, ninja is limited to at most FD_SETSIZE (1024) open file
            # descriptors.
            if sys.platform in ["darwin", "win32"]:
                j_value = min(j_value, 1000)

            # Use a j value that reliably works with the open file descriptors
            # limit.
            if sys.platform in ["darwin", "linux"]:
                j_value = min(j_value, int(fileno_limit * 0.8))

            ninja_args.append("%d" % j_value)
        else:
            j_value = num_cores
            # Ninja defaults to |num_cores + 2|
            j_value += int(os.environ.get("NINJA_CORE_ADDITION", "2"))
            ninja_args.append("-j")
            ninja_args.append("%d" % j_value)

    if summarize_build:
        # Enable statistics collection in Ninja.
        ninja_args += ["-d", "stats"]

    ninja_args += input_args[1:]

    if summarize_build:
        # Print the command-line to reassure the user that the right settings
        # are being used.
        _print_cmd(ninja_args)

    with android_build_server_helper.build_server_context(
            build_id, output_dir,
            use_android_build_server=use_android_build_server):
        if use_reclient and not t_specified:
            return reclient_helper.run_ninja(ninja_args)
        return ninja.main(ninja_args)


def _upload_ninjalog(args, exit_code, build_duration):
    warnings.simplefilter("ignore", ResourceWarning)
    # Run upload script without wait.
    creationflags = 0
    if platform.system() == "Windows":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP
    cmd = [
        sys.executable,
        _NINJALOG_UPLOADER,
        "--exit_code",
        str(exit_code),
        "--build_duration",
        str(int(build_duration)),
        "--cmdline",
    ] + args[1:]
    subprocess.Popen(
        cmd,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        start_new_session=True,
        creationflags=creationflags,
    )


def main(args):
    start = time.time()
    # Generate Build ID randomly.
    # This ID is expected to be used consistently in all build tools.
    build_id = os.environ.get("AUTONINJA_BUILD_ID")
    if not build_id:
        build_id = str(uuid.uuid4())
        os.environ.setdefault("AUTONINJA_BUILD_ID", build_id)

    # On Windows the autoninja.bat script passes along the arguments enclosed in
    # double quotes. This prevents multiple levels of parsing of the special '^'
    # characters needed when compiling a single file but means that this script
    # gets called with a single argument containing all of the actual arguments,
    # separated by spaces. When this case is detected we need to do argument
    # splitting ourselves. This means that arguments containing actual spaces
    # are not supported by autoninja, but that is not a real limitation.
    input_args = args
    exit_code = 127
    if sys.platform.startswith("win") and len(args) == 2:
        input_args = args[:1] + args[1].split()
    try:
        exit_code = _main_inner(input_args, build_id)
    except KeyboardInterrupt:
        exit_code = 1
    finally:
        # Check the log collection opt-in/opt-out status, and display notice if necessary.
        should_collect_logs = build_telemetry.enabled()
        if should_collect_logs:
            elapsed = time.time() - start
            _upload_ninjalog(input_args, exit_code, elapsed)
    return exit_code


if __name__ == "__main__":
    sys.exit(main(sys.argv))
