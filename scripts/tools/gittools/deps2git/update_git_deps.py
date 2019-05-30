#!/usr/bin/python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Convert SVN based DEPS into .DEPS.git for use with NewGit."""
from __future__ import print_function
from __future__ import absolute_import

import optparse
import os, os.path
import sys

from . import deps_utils
from . import git_tools
from . import svn_to_git_public


def SplitScmUrl(url):
  """Given a repository, return a set containing the URL and the revision."""
  url_split = url.split('@')
  scm_url = url_split[0]
  scm_rev = 'HEAD'
  if len(url_split) == 2:
    scm_rev = url_split[1]
  return (scm_url, scm_rev)

def GetSubModuleRev(workspace, path):
  """ Get the submodule revision hash for the given path in the base repo"""
  if not workspace or not path:
    raise Exception("No workspace or path")
    
  (st, output) = git_tools.GetStatusOutput("git rev-parse HEAD",
                       workspace+"/"+path)

  if st:
    raise Exception("Rev-parse returned error %d:%s", st,output)
    
  return output.strip()


def UpdateSubModuleDeps(deps, options, deps_vars):
  """Convert a 'deps' section in a .DEPS.git file with info from submodules."""
  new_deps = {}
  bad_git_urls = set([])

  for dep in deps:
    if not deps[dep]:  # dep is 'None' and emitted to exclude the dep
      new_deps[dep] = None
      continue

    # Get the URL and the revision/hash for this dependency.
    dep_url, dep_rev = SplitScmUrl(deps[dep])

    path = dep
    git_url = dep_url

    # Get the Git hash based off the SVN rev.
    git_hash = ''
    if dep_rev != 'HEAD':
      subpath = path.partition('/')[2]
      # Pass-through the hash for Git repositories. Resolve the hash for
      # submodule repositories.
      if os.path.exists(options.workspace + "/"+ subpath+"/.git"):
        git_hash = '@%s' % GetSubModuleRev(options.workspace, subpath)
      else:
        git_hash = "@%s" % (dep_rev,)

    # If this is webkit, we need to add the var for the hash.
    if dep == 'src/third_party/WebKit' and dep_rev:
      deps_vars['webkit_rev'] = git_hash
      git_hash = 'VAR_WEBKIT_REV'
    # If this is webkit, we need to add the var for the hash.
    elif dep == 'src/third_party/ffmpeg' and dep_rev:
      deps_vars['ffmpeg_hash'] = git_hash
      git_hash = 'VAR_FFMPEG_HASH'

    # Add this Git dep to the new deps.
    new_deps[path] = '%s%s' % (git_url, git_hash)

  return new_deps, bad_git_urls


def main():
  parser = optparse.OptionParser()
  parser.add_option('-d', '--deps', default='.DEPS.git',
                    help='path to the DEPS file to convert')
  parser.add_option('-w', '--workspace', metavar='PATH',
                    help='top level of a git-based gclient checkout')
  options = parser.parse_args()[0]

  # Get the content of the DEPS file.
  deps_content = deps_utils.GetDepsContent(options.deps)
  (deps, deps_os, include_rules, skip_child_includes, hooks,
   deps_vars, recursion) = deps_content

  # Convert the DEPS file to Git.
  deps, baddeps = UpdateSubModuleDeps(deps, options, deps_vars)
  for os_dep in deps_os:
    deps_os[os_dep], os_bad_deps = UpdateSubModuleDeps(
        deps_os[os_dep], options, deps_vars)
    baddeps = baddeps.union(os_bad_deps)

  if baddeps:
    print('\nUnable to resolve the following repositories. '
        'Please make sure\nthat any svn URLs have a git mirror associated with '
        'them.\nTo see the exact error, run `git ls-remote [repository]` where'
        '\n[repository] is the URL ending in .git (strip off the @revision\n'
        'number.) For more information, visit http://code.google.com\n'
        '/p/chromium/wiki/UsingNewGit#Adding_new_repositories_to_DEPS.\n', file=sys.stderr)
    for dep in baddeps:
      print(' ' + dep, file=sys.stderr)
    return 2

  # Write the DEPS file to disk.
  deps_utils.WriteDeps(options.deps, deps_vars, deps, deps_os, include_rules,
                       skip_child_includes, hooks, recursion)
  return 0


if '__main__' == __name__:
  sys.exit(main())
