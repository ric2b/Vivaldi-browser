#!/usr/bin/env python
# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import sys, os
import argparse
import subprocess

argparser = argparse.ArgumentParser();

argparser.add_argument("po_file_prefix")
argparser.add_argument("locales", nargs=argparse.REMAINDER)

options = argparser.parse_args()

scriptdir = os.path.dirname(__file__)

filemap = [
  {
    "name": "vivaldi_native_strings",
    "dir": os.path.join(scriptdir, "../native_resources"),
  },
  {
    "name": "vivaldi_components",
    "dir": os.path.join(scriptdir, "components"),
  },
  {
    "name": "vivaldi_components_strings",
    "dir": os.path.join(scriptdir, "components_strings"),
  },
  {
    "name": "vivaldi_generated_resources",
    "dir": os.path.join(scriptdir, "generated"),
  },
  {
    "name": "vivaldi_strings",
    "dir": os.path.join(scriptdir, "strings"),
  },
]

for mapentry in filemap:
  name_prefix = mapentry["name"]
  dir = mapentry["dir"]
  for locale in options.locales:
    subprocess.check_call(["python",
          os.path.join(scriptdir, "po2xtb.py"),
          "--locale", locale if locale != "nb" else "no",
          "--vivaldi-file", name_prefix,
          options.po_file_prefix+locale+".po",
          os.path.join(dir, name_prefix+".grd"),
          os.path.join(dir, "strings", name_prefix+"_"+locale+".xtb"),
          ]
        )
