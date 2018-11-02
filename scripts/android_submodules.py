
import os
import sys

import tools.gittools.gittools as Git

SRC = os.path.dirname(os.path.dirname(__file__))

BLACKLISTED_MODULES = []
BLACKLISTED_OS = []
BLACKLISTED_OS_CONDITION = ["checkout_"+x for x in BLACKLISTED_OS]
BASE_URL= "ssh://git.viv.int/srv/git/vivaldi"
CHROMIUM_URL = BASE_URL + "/chromium/src.git"

def __GetModuleInfoFromDeps(deps_info, selected_os=None, git=None):
  submodules = {}
  for mod, ref in deps_info["deps"].iteritems():
    #print mod, ref
    if mod.startswith("src/"):
      mod = mod[4:] # remove "src/"
    mod.replace("\\", "/")
    if ref == None:
      continue

    if isinstance(ref, dict):
      condition = ref["condition"]
      if any([cond in condition for cond in BLACKLISTED_OS_CONDITION]):
        continue
      if selected_os not in condition:
        continue
      ref = ref["url"]

    url, commit = ref.split("@")[:2]
    if commit != None and mod not in BLACKLISTED_MODULES:
      submodules[mod] = {
            "url":git.ResolveURL(url, True) if git else url,
            "commit":commit
          }

  return submodules

def IsAndroidEnabled():
  if "ANDROID_ENABLED" in os.environ:
    return True
  return os.access(os.path.join(SRC,".enable_android"), os.F_OK)

def main():
  if not IsAndroidEnabled():
    return 0

  chromium_git = Git.Git(source_dir=os.path.join(SRC, "chromium"),
                         url = CHROMIUM_URL,
                         base_url = BASE_URL)

  deps_info = chromium_git.GetGitDepInfo()
  submodules = __GetModuleInfoFromDeps(deps_info, "checkout_android", chromium_git)

  def _checkout_modules(_submodules):
    for path, config in _submodules.iteritems():
      module_git = Git.Git(inherit=chromium_git, url=config["url"],
                          source_dir = os.path.join(SRC, "chromium", path))
      module_git.FetchSource(update_submodules=False)
      module_git.Checkout(revision=config["commit"], only_commit=True,
                          update_submodules=False)

  _checkout_modules(submodules)

  main_submodules = dict(submodules)
  submodules = {}
  for path in deps_info["recursedeps"]:
    deps_file = "DEPS"
    if isinstance(path, (list, tuple)):
      path, deps_file = path[:2]
    if path.startswith("src/"):
      path = path[4:] # remove "src/"
    print path
    if path not in main_submodules:
      continue

    submodule_git = Git.Git(inherit=chromium_git,
                         source_dir=os.path.join(SRC, "chromium", path),
                         url = main_submodules[path]["url"],
                         base_url = BASE_URL)

    sub_deps_info = submodule_git.GetGitDepInfo(filename=deps_file)
    additional_modules = __GetModuleInfoFromDeps(sub_deps_info, "checkout_android", submodule_git)
    for sub_path, config in additional_modules.iteritems():
      submodules[os.path.join(path, sub_path)] = config

  _checkout_modules(submodules)

  return 0

if __name__ == '__main__':
  sys.exit(main())