# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is now only used by the closure_compilation builders."""

import argparse
import glob
import os
import shlex
import sys

script_dir = os.path.dirname(os.path.realpath(__file__))
chrome_src = os.path.abspath(os.path.join(script_dir, os.pardir))


def ProcessGNDefinesItems(items):
  """Converts a list of strings to a list of key-value pairs."""
  result = []
  for item in items:
    tokens = item.split('=', 1)
    # Some GN variables have hyphens, which we don't support.
    if len(tokens) == 2:
      val = tokens[1]
      if val[0] == '"' and val[-1] == '"':
        val = val[1:-1]
      elif val[0] == "'" and val[-1] == "'":
        val = val[1:-1]
      result += [(tokens[0], val)]
    else:
      # No value supplied, treat it as a boolean and set it. Note that we
      # use the string '1' here so we have a consistent definition whether
      # you do 'foo=1' or 'foo'.
      result += [(tokens[0], '1')]
  return result


def GetSupplementalFiles():
  return []


def GetGNVars(_):
  """Returns a dictionary of all GYP vars."""
  # GYP defines from the environment.
  env_items = ProcessGNDefinesItems(
      shlex.split(os.environ.get('GN_DEFINES', '')))

  # GYP defines from the command line.
  parser = argparse.ArgumentParser()
  parser.add_argument('-D', dest='defines', action='append', default=[])
  cmdline_input_items = parser.parse_known_args()[0].defines
  cmdline_items = ProcessGNDefinesItems(cmdline_input_items)

  return dict(env_items + cmdline_items)
