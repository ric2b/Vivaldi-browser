#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for formatting and writing DEPS files."""

import re
import sys
import subprocess
from deps2git.deps_utils import  *

DEPS_FIELDS = ['vars', 'deps', 'deps_os', 'include_rules',
              'skip_child_includes', 'hooks', 'recursion']

DEPS_VAR_LIST = {
  "vivaldi/src/third_party/WebKit":("webkit_rev", "VAR_WEBKIT_REV", False),
  "vivaldi/src/third_party/ffmpeg":("ffmpeg_hash", "VAR_FFMPEG_HASH", True),
}

def split_deps(deps):
  return tuple([deps[x] for x in DEPS_FIELDS])

def RemoveModule(deps_data, module):
  """Remove a module"""
  if not deps_data:
    print "RemoveModule: No DEPS data"
    return

  deps_data["deps"].pop(module, {})

  for x,y in deps_data["deps_os"].iteritems():
    y.pop(module, {})

def AddModule(deps_data, module, dep_locs, source, rev):
  """
  Add a module if it is not present in a deps location
  Ignores exisiting modules in locations
  Does not remove a module if it is not listed in a location
  """
  if not deps_data:
    print "AddModule: No DEPS data"
    return

  for x in dep_locs:
    deps = (deps_data["deps"] if x == "deps" else
              deps_data["deps_os"].setdefault(x,{}))
    if module in deps:
      continue
    if source and rev:
      deps[module] = source+"@"+ rev
    else:
      deps[module] = None

def UpdateModuleInfo(deps_data, modules, path_prefix="src"):
  """Update module revisions with data from submodules"""

  path_levels = path_prefix.count("/")+1

  if not deps_data:
    print "UpdateModuleInfo: No DEPS data"
    return

  for dep_list in [deps_data["deps"]] + list(deps_data["deps_os"].itervalues()):
    for mod, url_rev in dep_list.iteritems():
      if not url_rev:
        continue
      (url, sep, rev) = url_rev.partition("@")

      mod2 = mod.split("/", path_levels)[-1]

      if mod2 not in modules:
        continue

      mod_rev = modules[mod2]
      rev = "@"+(mod_rev if isinstance(mod_rev,str) else mod_rev["id"])

      if mod in DEPS_VAR_LIST:
        var, placeholder, add_quote = DEPS_VAR_LIST[mod]
        deps_data["vars"][var]=rev
        rev = placeholder

      dep_list[mod] = url+rev

def PrintDeps(deps_data):
  if not deps_data:
    print "PrintDeps: No DEPS data"
    return
  for (x1,y1) in [(x, deps_data[x]) for x in DEPS_FIELDS]:
    print x1, ":",
    if isinstance(y1, dict):
      print "{"
      for (x,y) in sorted(y1.iteritems()):
        if isinstance(y, dict):
          print "    ",x,"{"
          for (x2,y2) in sorted(y.iteritems()):
            print "        ", x2,":",y2
          print "},"
        else:
          print "     ", x,":",y
      print "},"
    elif isinstance(y1, list):
      print "["
      for x in y1:
        print "     ", x
      print "],"
    else:
      print y1

def RunHooks(deps_data, cwd, env=None):
  if not deps_data:
    print "RunHooks: No DEPS data"
    return
  for hook in deps_data["hooks"]:
    if "action" in hook:
      print 'running action "%s" in %s' %(hook["action"],cwd)
      try:
        st = subprocess.call(hook["action"], cwd=cwd, env=env, shell=True)
      except Exception,e:
        print "Env:", env
        print "Exception:", e
        st = -1
      if st != 0:
        print "Exit status:", st
        raise BaseException("Hook failed")
