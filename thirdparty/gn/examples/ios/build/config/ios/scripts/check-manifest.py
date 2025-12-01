# Copyright 2025 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Check the content of a bundle according to a manifest."""

import sys
import os


def write_if_needed(path, content):
  """Write `content` to `path` if needed."""
  if os.path.isfile(path):
    # If the file exists and has the same content, skip writing.
    with open(path, encoding='utf-8') as file:
      if content == file.read():
        return
  with open(path, 'w', encoding='utf-8') as file:
    file.write(content)
    file.flush()



def read_manifest(manifest_path):
  """Read a manifest file."""
  manifest_content = set()
  with open(manifest_path, encoding='utf-8') as manifest_file:
    for line in manifest_file:
      assert line.endswith('\n')
      manifest_content.add(line[:-1])
  return manifest_content


def main(args):
  if len(args) != 3:
    print(
      f'usage: check-manifest.py bundle-dir manifest-path output-path',
      file=sys.stderr)
    return 1

  bundle_dir, manifest_path, output_path = args
  if not os.path.isdir(bundle_dir):
    print(
      f'error: {bundle_dir}: not a directory',
      file=sys.stderr)
    return 1

  has_unexpected_files = False
  manifest_content = read_manifest(manifest_path)
  for dirpath, dirnames, filenames in os.walk(bundle_dir):
    dirnames_to_skip = []
    for dirname in dirnames:
      subdirpath = os.path.relpath(os.path.join(dirpath, dirname), bundle_dir)
      if subdirpath in manifest_content:
        dirnames_to_skip.append(dirname)
    if dirnames_to_skip:
      dirnames[:] = list(set(dirnames) - set(dirnames_to_skip))
    for filename in filenames:
      filepath = os.path.relpath(os.path.join(dirpath, filename), bundle_dir)
      if filepath not in manifest_content:
        fullpath = os.path.normpath(os.path.join(bundle_dir, filepath))
        print(f'error: {fullpath}: unexpected file', file=sys.stderr)
        has_unexpected_files = True

  write_if_needed(output_path, 'ko\n' if has_unexpected_files else 'ok\n')

  if has_unexpected_files:
    return 1

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
