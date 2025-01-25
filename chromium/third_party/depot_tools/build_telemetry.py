#!/usr/bin/env python3
# Copyright 2024 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import os
import subprocess
import sys
import textwrap

import utils

_DEFAULT_CONFIG_PATH = utils.depot_tools_config_path("build_telemetry.cfg")

_DEFAULT_COUNTDOWN = 10

VERSION = 1


class Config:

    def __init__(self, config_path, countdown):
        self._config_path = config_path
        self._config = None
        self._notice_displayed = False
        self._countdown = countdown

    def load(self):
        """Loads the build telemetry config."""
        if self._config:
            return

        config = {}
        if os.path.isfile(self._config_path):
            with open(self._config_path) as f:
                try:
                    config = json.load(f)
                except Exception:
                    pass
                if config.get("version") != VERSION:
                    config = None  # Reset the state for version change.

        if not config:
            config = {
                "is_googler": is_googler(),
                "status": None,
                "countdown": self._countdown,
                "version": VERSION,
            }

        self._config = config

    def save(self):
        with open(self._config_path, "w") as f:
            json.dump(self._config, f)

    @property
    def path(self):
        return self._config_path

    @property
    def is_googler(self):
        if not self._config:
            return
        return self._config.get("is_googler") == True

    @property
    def countdown(self):
        if not self._config:
            return
        return self._config.get("countdown")

    @property
    def version(self):
        if not self._config:
            return
        return self._config.get("version")

    def enabled(self):
        if not self._config:
            print("WARNING: depot_tools.build_telemetry: %s is not loaded." %
                  self._config_path,
                  file=sys.stderr)
            return False
        if not self._config.get("is_googler"):
            return False
        if self._config.get("status") == "opt-out":
            return False

        if self._should_show_notice():
            remaining = max(0, self._config["countdown"] - 1)
            self._show_notice(remaining)
            self._notice_displayed = True
            self._config["countdown"] = remaining
            self.save()

        # Telemetry collection will happen.
        return True

    def _should_show_notice(self):
        if self._notice_displayed:
            return False
        if self._config.get("countdown") == 0:
            return False
        if self._config.get("status") == "opt-in":
            return False
        return True

    def _show_notice(self, remaining):
        """Dispalys notice when necessary."""
        print(
            textwrap.dedent(f"""\
            *** NOTICE ***
            Google-internal telemetry (including build logs, username, and hostname) is collected on corp machines to diagnose performance and fix build issues. This reminder will be shown {remaining} more times. See http://go/chrome-build-telemetry for details. Hide this notice or opt out by running: build_telemetry [opt-in] [opt-out]
            *** END NOTICE ***
            """))

    def opt_in(self):
        self._config["status"] = "opt-in"
        self.save()
        print("build telemetry collection is opted in")

    def opt_out(self):
        self._config["status"] = "opt-out"
        self.save()
        print("build telemetry collection is opted out")


def load_config(cfg_path=_DEFAULT_CONFIG_PATH, countdown=_DEFAULT_COUNTDOWN):
    """Loads the config from the default location."""
    cfg = Config(cfg_path, countdown)
    cfg.load()
    return cfg


def is_googler():
    """Checks whether this user is Googler or not."""
    p = subprocess.run(
        "cipd auth-info",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        shell=True,
    )
    if p.returncode != 0:
        return False
    lines = p.stdout.splitlines()
    if len(lines) == 0:
        return False
    l = lines[0]
    # |l| will be like 'Logged in as <user>@google.com.' for googler using
    # reclient.
    return l.startswith("Logged in as ") and l.endswith("@google.com.")


def enabled():
    """Checks whether the build can upload build telemetry."""
    cfg = load_config()
    return cfg.enabled()


def main():
    parser = argparse.ArgumentParser(prog="Build Telemetry util")
    parser.add_argument('status', nargs=1, choices=['opt-in', 'opt-out'])
    args = parser.parse_args()

    cfg = load_config()

    if not cfg.is_googler:
        cfg.save()
        return

    if args.status == "opt-in":
        cfg.opt_in()
        return
    if args.status == "opt-out":
        cfg.opt_out()
        return


if __name__ == "__main__":
    sys.exit(main())
