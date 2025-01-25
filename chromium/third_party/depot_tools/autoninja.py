#!/usr/bin/env vpython3
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

import logging
import json
import multiprocessing
import os
import platform
import re
import shelve
import shlex
import shutil
import subprocess
import sys
import time
import warnings

import google.auth
from google.auth.transport.requests import AuthorizedSession

import build_telemetry
import gn_helper
import ninja
import ninjalog_uploader
import reclient_helper
import siso

if sys.platform in ["darwin", "linux"]:
    import resource

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


def _adc_account():
    """Returns account used to authenticate with GCP application default credentials."""

    try:
        # Suppress warnings from google.auth.default.
        # https://github.com/googleapis/google-auth-library-python/issues/271
        warnings.filterwarnings(
            "ignore",
            "Your application has authenticated using end user credentials from"
            " Google Cloud SDK without a quota project.",
        )
        credentials, _ = google.auth.default(
            scopes=["https://www.googleapis.com/auth/userinfo.email"])
    except google.auth.exceptions.DefaultCredentialsError:
        # Application Default Crendetials is not configured.
        return None
    finally:
        warnings.resetwarnings()

    with AuthorizedSession(credentials) as session:
        try:
            response = session.get(
                "https://www.googleapis.com/oauth2/v1/userinfo")
        except Exception:
            # Ignore exception.
            return None

    return response.json().get("email")


def _gcloud_auth_account():
    """Returns active account authenticated with `gcloud auth login`."""
    if shutil.which("gcloud") is None:
        return None

    accounts = json.loads(
        subprocess.check_output("gcloud auth list --format=json",
                                shell=True,
                                text=True))
    for account in accounts:
        if account["status"] == "ACTIVE":
            return account["account"]
    return None


def _luci_auth_account():
    """Returns active account authenticated with `luci-auth login -scopes-context`."""
    if shutil.which("luci-auth") is None:
        return None

    # First line returned should be "Logged in as account@domain.com."
    # Extract the account@domain.com from that line.
    try:
        info = subprocess.check_output("luci-auth info -scopes-context",
                                       shell=True,
                                       stderr=subprocess.STDOUT,
                                       text=True).split('\n')[0]
        if info.startswith("Logged in as "):
            return info[len("Logged in as "):-1]
    except subprocess.CalledProcessError:
        return None
    return None


def _is_google_corp_machine():
    """This assumes that corp machine has gcert binary in known location."""
    return shutil.which("gcert") is not None


def _is_google_corp_machine_using_external_account():
    if os.environ.get("AUTONINJA_SKIP_EXTERNAL_ACCOUNT_CHECK") == "1":
        print(
            "WARNING: AUTONINJA_SKIP_EXTERNAL_ACCOUNT_CHECK env var is set.\n"
            "This is only for some infra, do not set this in personal"
            " development machine.",
            file=sys.stderr)
        return False

    if not _is_google_corp_machine():
        return False

    with shelve.open(os.path.join(_SCRIPT_DIR, ".autoninja")) as db:
        last_false = db.get("last_false")
        now = time.time()
        if last_false is not None and now < last_false + 12 * 60 * 60:
            # Do not check account if it is checked in last 12 hours.
            return False

        account = _adc_account()
        if account and not account.endswith("@google.com"):
            return True

        account = _luci_auth_account()
        if account and not account.endswith("@google.com"):
            return True

        account = _gcloud_auth_account()
        if not account:
            db["last_false"] = now
            return False

        # Handle service account and google account as internal account.
        if not (account.endswith("@google.com")
                or account.endswith("gserviceaccount.com")):
            return True

        db["last_false"] = now
        return False


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


def main(args, should_collect_logs=False):
    # if user doesn't set PYTHONPYCACHEPREFIX and PYTHONDONTWRITEBYTECODE
    # set PYTHONDONTWRITEBYTECODE=1 not to create many *.pyc in workspace
    # and keep workspace clean.
    if not os.environ.get("PYTHONPYCACHEPREFIX"):
        os.environ.setdefault("PYTHONDONTWRITEBYTECODE", "1")
    # The -t tools are incompatible with -j
    t_specified = False
    j_specified = False
    offline = False
    output_dir = "."
    input_args = args
    summarize_build = os.environ.get("NINJA_SUMMARIZE_BUILD") == "1"
    # On Windows the autoninja.bat script passes along the arguments enclosed in
    # double quotes. This prevents multiple levels of parsing of the special '^'
    # characters needed when compiling a single file but means that this script
    # gets called with a single argument containing all of the actual arguments,
    # separated by spaces. When this case is detected we need to do argument
    # splitting ourselves. This means that arguments containing actual spaces
    # are not supported by autoninja, but that is not a real limitation.
    if sys.platform.startswith("win") and len(args) == 2:
        input_args = args[:1] + args[1].split()

    # Ninja uses getopt_long, which allow to intermix non-option arguments.
    # To leave non supported parameters untouched, we do not use getopt.
    for index, arg in enumerate(input_args[1:]):
        if arg.startswith("-j"):
            j_specified = True
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
        elif arg in ("-h", "--help"):
            print(
                "autoninja: Use -o/--offline to temporary disable remote execution.",
                file=sys.stderr,
            )
            print(file=sys.stderr)

    use_remoteexec = False
    use_reclient = None
    use_siso = False

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
            if k == "use_siso" and v == "true":
                use_siso = True
                continue
            if k == "use_reclient" and v == "false":
                use_reclient = False
                continue
        if use_reclient is None:
            use_reclient = use_remoteexec

        if use_remoteexec:
            if _is_google_corp_machine_using_external_account():
                print(
                    "You can't use a non-@google.com account (%s and/or %s) on"
                    " a corp machine.\n"
                    "Please login via `gcloud auth login --update-adc` with"
                    " your @google.com account instead.\n" %
                    (_adc_account(), _gcloud_auth_account()),
                    file=sys.stderr,
                )
                return 1

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
            if use_remoteexec:
                if use_reclient:
                    return reclient_helper.run_siso(
                        [
                            'siso',
                            'ninja',
                            # Do not authenticate when using Reproxy.
                            '-project=',
                            '-reapi_instance=',
                        ] + input_args[1:],
                        should_collect_logs)
                return siso.main(["siso", "ninja"] + input_args[1:])
            return siso.main(["siso", "ninja", "--offline"] + input_args[1:])

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

    # If --offline is set, then reclient will use the local compiler instead of
    # doing a remote compile. This is convenient if you want to briefly disable
    # remote compile. It avoids having to rebuild the world when transitioning
    # between RBE/non-RBE builds. However, it is not as fast as doing a "normal"
    # non-RBE build because an extra process is created for each compile step.
    if offline:
        # Tell reclient to do local compiles.
        os.environ["RBE_remote_disabled"] = "1"

    # On macOS and most Linux distributions, the default limit of open file
    # descriptors is too low (256 and 1024, respectively).
    # This causes a large j value to result in 'Too many open files' errors.
    # Check whether the limit can be raised to a large enough value. If yes,
    # use `ulimit -n .... &&` as a prefix to increase the limit when running
    # ninja.
    if sys.platform in ["darwin", "linux"]:
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

    args = ['ninja']
    num_cores = multiprocessing.cpu_count()
    if not j_specified and not t_specified:
        if not offline and use_remoteexec:
            args.append("-j")
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

            args.append("%d" % j_value)
        else:
            j_value = num_cores
            # Ninja defaults to |num_cores + 2|
            j_value += int(os.environ.get("NINJA_CORE_ADDITION", "2"))
            args.append("-j")
            args.append("%d" % j_value)

    if summarize_build:
        # Enable statistics collection in Ninja.
        args += ["-d", "stats"]

    args += input_args[1:]

    if summarize_build:
        # Print the command-line to reassure the user that the right settings
        # are being used.
        _print_cmd(args)

    if use_reclient:
        return reclient_helper.run_ninja(args, should_collect_logs)
    return ninja.main(args)


def _upload_ninjalog(args):
    # Run upload script without wait.
    creationflags = 0
    if platform.system() == "Windows":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP
    subprocess.Popen(
        [sys.executable, _NINJALOG_UPLOADER, "--cmdline"] + args[1:],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        creationflags=creationflags,
    )


if __name__ == "__main__":
    # Check the log collection opt-in/opt-out status, and display notice if necessary.
    _should_collect_logs = build_telemetry.enabled()
    try:
        exit_code = main(sys.argv, _should_collect_logs)
    except KeyboardInterrupt:
        exit_code = 1
    finally:
        if _should_collect_logs:
            _upload_ninjalog(sys.argv)
    sys.exit(exit_code)
