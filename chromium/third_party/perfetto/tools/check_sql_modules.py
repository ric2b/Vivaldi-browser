#!/usr/bin/env python3
# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This tool checks that every SQL object created without prefix
# '_' is documented with proper schema.

import argparse
from typing import List, Tuple
import os
import sys
import re

ROOT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(ROOT_DIR))

from python.generators.sql_processing.docs_parse import ParsedFile
from python.generators.sql_processing.docs_parse import parse_file
from python.generators.sql_processing.utils import check_banned_create_table_as
from python.generators.sql_processing.utils import check_banned_create_view_as
from python.generators.sql_processing.utils import check_banned_words
from python.generators.sql_processing.utils import check_banned_include_all


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument(
      '--stdlib-sources',
      default=os.path.join(ROOT_DIR, "src", "trace_processor", "perfetto_sql",
                           "stdlib"))
  parser.add_argument(
      '--verbose',
      action='store_true',
      default=False,
      help='Enable additional logging')
  parser.add_argument(
      '--name-filter',
      default=None,
      type=str,
      help='Filter the name of the modules to check (regex syntax)')

  args = parser.parse_args()
  errors = []
  modules: List[Tuple[str, str, ParsedFile]] = []
  for root, _, files in os.walk(args.stdlib_sources, topdown=True):
    for f in files:
      path = os.path.join(root, f)
      if not path.endswith(".sql"):
        continue
      rel_path = os.path.relpath(path, args.stdlib_sources)
      if args.name_filter is not None:
        pattern = re.compile(args.name_filter)
        if not pattern.match(rel_path):
          continue

      with open(path, 'r') as f:
        sql = f.read()

      parsed = parse_file(rel_path, sql)

      # Some modules (i.e. `deprecated`) should not be checked.
      if not parsed:
        continue

      modules.append((path, sql, parsed))

      if args.verbose:
        obj_count = len(parsed.functions) + len(parsed.table_functions) + len(
            parsed.table_views) + len(parsed.macros)
        print(
            f"""Parsing '{rel_path}' ({obj_count} objects, {len(parsed.errors)} errors)
- {len(parsed.functions)} functions + {len(parsed.table_functions)} table functions,
- {len(parsed.table_views)} tables/views,
- {len(parsed.macros)} macros.""")

  for path, sql, parsed in modules:
    lines = [l.strip() for l in sql.split('\n')]
    for line in lines:
      if line.startswith('--'):
        continue
      if 'RUN_METRIC' in line:
        errors.append(f"RUN_METRIC is banned in standard library.\n"
                      f"Offending file: {path}\n")
      if 'include perfetto module common.' in line.casefold():
        errors.append(
            f"Common module has been deprecated in the standard library.\n"
            f"Offending file: {path}\n")
      if 'insert into' in line.casefold():
        errors.append(f"INSERT INTO table is not allowed in standard library.\n"
                      f"Offending file: {path}\n")

    errors += parsed.errors
    errors += check_banned_words(sql, path)
    errors += check_banned_create_table_as(
        sql,
        path.split(ROOT_DIR)[1],
        args.stdlib_sources.split(ROOT_DIR)[1])
    errors += check_banned_create_view_as(sql, path.split(ROOT_DIR)[1])
    errors += check_banned_include_all(sql, path.split(ROOT_DIR)[1])

  if errors:
    sys.stderr.write("\n".join(errors))
    sys.stderr.write("\n")
  return 0 if not errors else 1


if __name__ == "__main__":
  sys.exit(main())
