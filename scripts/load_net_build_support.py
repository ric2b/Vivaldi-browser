# Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

import os
import urllib.request

download_list = [
  ("base/edge_versions.json", b"{}"),
  ]

for target, content in download_list:
  if os.access(target, os.F_OK):
    continue; # Not overwriting exisiting file

  with open(target, "wb") as f:
    f.write(content)
  print("Updated %s" % target)
