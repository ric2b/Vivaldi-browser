#!/bin/env vpython3
# Copyright 2024 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Utility for opting in or out of metrics collection"""
import argparse
import sys

import config


def main():
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument('--disable',
                        '-d',
                        dest='enable',
                        action='store_false',
                        default=None,
                        help='Disable telemetry collection.')

    parser.add_argument('--enable',
                        '-e',
                        dest='enable',
                        action='store_true',
                        default=None,
                        help='Enable telemetry collection.')

    parser.add_argument('--bot-enable',
                        '-b',
                        dest='bot_enable',
                        action='store_true',
                        default=False,
                        help='Enable for bots. Ignores googler check. '
                        'Not for human users.')

    args = parser.parse_args()

    if args.enable is not None:
        cfg = config.Config(config.DEFAULT_CONFIG_FILE)
        cfg.trace_config.update(args.enable, 'USER')
        cfg.flush()
    elif args.bot_enable:
        cfg = config.Config(config.DEFAULT_CONFIG_FILE)
        cfg.trace_config.update(args.bot_enable, 'BOT_USER')
        cfg.flush()
    else:
        print('Error: --enable --disable or --bot-enable flag is required.')


if __name__ == '__main__':
    sys.exit(main())
