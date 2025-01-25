#!/usr/bin/env python3
# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import sys, os
import argparse
import subprocess

argparser = argparse.ArgumentParser();

argparser.add_argument("po_file_prefix")
argparser.add_argument("locales", nargs=argparse.REMAINDER)

options = argparser.parse_args()

scriptdir = os.path.dirname(__file__)

# Keep in sync with:
#   FILE list in vivapp/bin/gettext/update-chromium.sh
#   filemap in app/resources/po2xtb_batch.py
#   CHROMIUM_VIVALDI_RESOURCE_LIST in app/resources_list.gni
filemap = {
  # Android
  "vivaldi_android_chrome_strings": "android",
  "vivaldi_android_chrome_tab_ui_strings": "android-tab-ui",
  "vivaldi_android_strings": os.path.join("..", "android"),
  "vivaldi_android_webapps_strings": "android_webapps",
  # iOS
  "vivaldi_ios_strings": "ios_strings",
  "vivaldi_ios_native_strings": os.path.join("..", "ios"),
  "vivaldi_ios_branding_strings": "ios_branding_strings",
  "vivaldi_ios_widget_kit_extension_strings": "ios_widget_kit_extension_strings",
  # Desktop
  "vivaldi_components_strings": "components_strings",
  "vivaldi_components": "components",
  "vivaldi_content_strings": "content",
  "vivaldi_generated_resources": "generated",
  "vivaldi_installer_strings": os.path.join("..", "installer"),
  "vivaldi_native_strings": os.path.join("..", "native_resources"),
  "vivaldi_strings": "strings",
  "vivaldi_ui_strings": "ui_strings",
}

for name_prefix, dir in filemap.items():
  for locale in options.locales:
    subprocess.check_call(["python3",
      os.path.join(scriptdir, "po2xtb.py"),
      "--locale", locale if locale != "nb" else "no",
      "--vivaldi-file", name_prefix,
      options.po_file_prefix+locale+".po",
      os.path.join(scriptdir, dir, name_prefix+".grd"),
      os.path.join(scriptdir, dir, "strings", name_prefix+"_"+locale+".xtb"),
    ])
