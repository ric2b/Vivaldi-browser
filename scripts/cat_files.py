#!/usr/bin/env python
# Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved

import argparse


parser = argparse.ArgumentParser()

parser.add_argument("output")
parser.add_argument("files", nargs=argparse.REMAINDER)

options = parser.parse_args()

new_content = ""
for filename in options.files:
  with open(filename) as f:
    content = f.read()
    if content[-1] != "\n":
      content += "\n"
    new_content += content

with open(options.output, "w") as f:
  f.write(new_content)
