# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import argparse

argparser = argparse.ArgumentParser()

argparser.add_argument("target")
argparser.add_argument("locales", nargs=argparse.REMAINDER)

options = argparser.parse_args()

if "en-US" in options.locales:
  options.locales.remove("en-US");

if not options.locales:
  raise Exception("Must specify at least one locale");

with open(options.target, "wt") as f:
  print("""# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import argparse
import os

LOCALES = [
  "%(locales)s"
]

argparser = argparse.ArgumentParser()
argparser.add_argument("prefix_path")
argparser.add_argument("filename")
options = argparser.parse_args()

print("-"*40)
print("<translations>")
for locale in LOCALES:
  spec_locale = locale if locale != "nb" else "no"
  params = dict(
    path = options.prefix_path,
    filename = options.filename,
    locale = locale,
    spec_locale = spec_locale,
  )
  fname = "%%(path)s/%%(filename)s_%%(locale)s.xtb" %% params
  if not os.access(fname, os.R_OK):
    with open(fname, "wt") as f:
      print (\"\"\"<?xml version="1.0" ?>
<!DOCTYPE translationbundle>
<translationbundle lang="%%s">
</translationbundle>\"\"\" %% spec_locale, file=f)
    print('  <file path="%%(filename)s_%%(locale)s.xtb" lang="%%(spec_locale)s" />' %% params)

print("</translations>")
print("-"*40)
""" % dict(locales = '",\n  "'.join(options.locales)), file=f)
