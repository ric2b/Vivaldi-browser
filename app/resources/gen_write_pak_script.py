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
argparser.add_argument("--android", action="store_true")
argparser.add_argument("filename")
argparser.add_argument("prefix_path", default="", nargs="?")
options = argparser.parse_args()

print("-"*40)
print("<output>")
format = "%%(path)s%%(filename)s_%%(locale)s.%%(ext)s"
if options.android:
  format = "%%(path)s-%%(locale)s/%%(filename)s.%%(ext)s"
for locale in LOCALES:
  spec_locale = locale if locale != "nb" else "no"
  params = dict(
    path = options.prefix_path + "/" if options.prefix_path and not options.android else options.prefix_path,
    locale = locale,
    spec_locale = spec_locale,
    ext = "xml" if options.android else "pak",
    filename = options.filename,
    type = "android" if options.android else "data_package",
  )
  print( ('  <output filename="' + format + '" type="%%(type)s" lang="%%(spec_locale)s" />') %% params)
print("</output>")
print("-"*40)
""" % dict(locales = '",\n  "'.join(sorted(list(set(options.locales))))), file=f)
