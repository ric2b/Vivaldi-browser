from __future__ import print_function
from builtins import range

import sys, os, os.path
import subprocess
import argparse
import read_deps_file as deps_utils
import datetime
import platform
import shlex
import shutil

SRC = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
hooks_folder = os.path.join(SRC, "scripts", "templates", "hooks")
chromium_hooks_folder = os.path.join(SRC, "scripts", "templates", "chromium_hooks")
git_hooks_folder = os.path.join(SRC, ".git", "hooks")
chromium_git_hooks_folder = os.path.join(SRC, ".git", "modules", "chromium", "hooks")

depot_tools_path = os.path.join(SRC, "chromium/third_party/depot_tools")
os.environ["PATH"] = os.pathsep.join([depot_tools_path, os.environ["PATH"]])
os.environ["DEPOT_TOOLS_WIN_TOOLCHAIN"]="0"

OS_CHOICES = {
  "win32": "win",
  "win": "win",
  "cygwin": "win",
  "darwin": "mac",
  "ios": "ios",
  "mac": "mac",
  "unix": "linux",
  "linux": "linux",
  "linux2": "linux",
  "linux3": "linux",
  "android": "android",
  "arm":"arm",
  "arm64":"arm64",
  "x86":"x86",
  "x64":"x64",
}

def copy_files(src, dst):
  for f in os.listdir(src):
    if not os.access(os.path.join(dst, f), os.R_OK+os.X_OK):
      shutil.copy(os.path.join(src, f), os.path.join(dst, f))

def IsAndroidEnabled():
  if "ANDROID_ENABLED" in os.environ:
    return True
  return os.access(os.path.join(SRC,".enable_android"), os.F_OK)

def ProcessGNDefinesItems(items):
  """Converts a list of strings to a list of key-value pairs."""
  result = []
  for item in items:
    tokens = item.split('=', 1)
    # Some GN variables have hyphens, which we don't support.
    if len(tokens) == 2:
      val = tokens[1]
      if val[0] == '"' and val[-1] == '"':
        val = val[1:-1]
      elif val[0] == "'" and val[-1] == "'":
        val = val[1:-1]
      result += [(tokens[0], val)]
    else:
      # No value supplied, treat it as a boolean and set it. Note that we
      # use the string '1' here so we have a consistent definition whether
      # you do 'foo=1' or 'foo'.
      result += [(tokens[0], '1')]
  return result

def GetGNVars():
  """Returns a dictionary of all GN vars."""
  # GYP defines from the environment.
  env_items = ProcessGNDefinesItems(
      shlex.split(os.environ.get('GN_DEFINES', '')))

  return dict(env_items)

host_os = OS_CHOICES.get(sys.platform, 'unix')
checkout_os = host_os
checkout_cpu = "x64"
if IsAndroidEnabled():
  checkout_os = "android"
  checkout_cpu = "arm"

checkout_os = "checkout_"+checkout_os

gnvars = GetGNVars()
if "target_cpu" in gnvars:
  checkout_cpu = gnvars["target_cpu"]

checkout_cpu = "checkout_"+checkout_cpu

parser = argparse.ArgumentParser()
parser.add_argument("--clobber-out", action="store_true");
parser.add_argument("--args-gn")

args = parser.parse_args()

prefix_name = os.path.split(SRC)[1]

workdir = SRC
extra_subprocess_flags = {}
if platform.system() == "Windows":
  try:
    import check_win_python
    check_win_python.CheckPythonInstall()
  except:
    pass
  try:
    import win32con
    extra_subprocess_flags["creationflags"] = win32con.NORMAL_PRIORITY_CLASS
  except:
    pass

def RunHooks(hooks, cwd, env=None, prefix_name=None):
  if not hooks:
    return
  global_vars = {
    '__builtin__': None,
    'host_os': host_os,
    checkout_os: True,
    checkout_cpu: True,
  }
  global_vars["checkout_x64"]= True # Always checking out on x64 systems
  if checkout_os == "checkout_android":
    global_vars["checkout_linux"]= True # Always checking out android on linux systems

  for an_os in set(list(OS_CHOICES.values())+["telemetry_dependencies"]):
    global_vars.setdefault("checkout_"+an_os, False)
  for hook in hooks:
    if 'condition' in hook and not eval(hook['condition'], global_vars):
      continue
    if "action" in hook:
      action = hook["action"]
      if hook["name"] == "bootstrap-gn" and args.args_gn:
        action.extend(["--args-gn", args.args_gn])
      print('running action "%s" in %s' %(action,cwd))
      if subprocess.call(action, cwd=cwd, env=env, shell=(os.name=="nt"),
                         **extra_subprocess_flags) != 0:
        raise BaseException("Hook failed")

if args.clobber_out:
  import shutil
  build_dir = "out"
  out_dir = os.path.join(SRC,build_dir)
  if os.access(out_dir, os.R_OK):
    start_time = datetime.datetime.now()
    print("Deleting ", out_dir)
    for _ in range(4):
      try:
        shutil.rmtree(out_dir)
      except:
        pass
      if not os.access(out_dir, os.R_OK):
        break
      print("New delete try after", (datetime.datetime.now()-start_time).total_seconds(), "seconds")
    if os.access(out_dir, os.R_OK):
      raise Exception("Could not delete out directory")
    stop_time = datetime.datetime.now()
    print("Deleted", out_dir, "in", (stop_time-start_time).total_seconds(), "seconds")
  if IsAndroidEnabled() and int(os.environ.get("CHROME_HEADLESS",0)):
    print("Cleaning checkout ", SRC)
    subprocess.call(["git", "clean", "-fdx"], cwd=SRC)
    subprocess.call(["git", "submodule", "foreach", "--recursive", "git clean -fdx"], cwd=SRC)

if "CHROME_HEADLESS" not in os.environ:
  if os.access(hooks_folder, os.F_OK):
    copy_files(hooks_folder, git_hooks_folder)
  if os.access(chromium_hooks_folder, os.F_OK):
    copy_files(chromium_hooks_folder, chromium_git_hooks_folder)

deps_content = deps_utils.GetDepsContent("DEPS")
(deps, hooks, deps_vars, recursion) = deps_content

RunHooks(hooks, cwd=workdir, prefix_name=prefix_name)
