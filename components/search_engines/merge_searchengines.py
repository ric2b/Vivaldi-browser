# Copyright 2020 Vivaldi Technologies AS. All rights reserved.

from __future__ import print_function
from future import standard_library
standard_library.install_aliases()

import json
import os
import sys

_script_path = os.path.realpath(os.path.dirname(__file__))

sys.path.insert(0, os.path.normpath(_script_path + "/../../chromium/tools/json_comment_eater"))
try:
  import json_comment_eater
finally:
  sys.path.pop(0)

upstream_json_name = sys.argv[1]
vivaldi_json_name = sys.argv[2]
output_json_name = sys.argv[3]

with open(upstream_json_name, "r") as f:
  upstream_json = json.loads(json_comment_eater.Nom(f.read()))

with open(vivaldi_json_name, "r") as f:
  vivaldi_json = json.loads(json_comment_eater.Nom(f.read()))

upstream_json["int_variables"]["kCurrentDataVersion"] = vivaldi_json["int_variables"]["kCurrentDataVersion"]

upstream_json["elements"].update(vivaldi_json["elements"])

try:
  os.makedirs(os.path.dirname(output_json_name), True)
except:
  pass # will fail next

with open(output_json_name, "w") as f:
  json.dump(upstream_json, f, indent=2, separators=(',', ': '))
