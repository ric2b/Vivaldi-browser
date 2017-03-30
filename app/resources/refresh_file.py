# Copyright (c) 2016 Vivaldi Technologies AS. All rights reserved

import os
import hashlib
import shutil

def __hash_file(filename):
  digest =hashlib.sha256()
  with open(filename,"r") as oldfile:
    while True:
      content = oldfile.read(128*1024)
      if not content:
        break;
      digest.update(content)

  return digest.hexdigest()

def conditional_refresh_file(filename, new_content, stamp_file=None):

  if os.access(filename, os.F_OK):
    old_digest = __hash_file(filename)

    new_digest =hashlib.sha256(new_content)

    if old_digest == new_digest.hexdigest():
      # No need to update target file
      if stamp_file and not os.access(stamp_file, os.F_OK):
        # But the stamp file needs to be created
        with open(stamp_file, "w") as _stamptouch:
          pass # just opening files will update date
      return

  with open(filename,"w") as newfile:
    newfile.write(new_content)

  if stamp_file:
    with open(stamp_file, "w") as _stamptouch:
      pass # just opening files will update date

def conditional_copy(source_filename, dest_filename, hash_check=False):

  if os.access(source_filename, os.F_OK) and os.access(dest_filename, os.F_OK):
    if not hash_check:
      src_time = os.path.getmtime(source_filename)
      dst_time = os.path.getmtime(dest_filename)

      if src_time <= dst_time:
        return
    else:
      cur_digest = __hash_file(dest_filename)
      src_digest = __hash_file(source_filename)

      if cur_digest == src_digest:
        return

  shutil.copy2(source_filename, dest_filename)
