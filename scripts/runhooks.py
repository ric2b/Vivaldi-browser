import sys, os
import subprocess
import argparse
import datetime
import platform
import shutil

import vivdeps

SRC = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
hooks_folder = os.path.join(SRC, "scripts", "templates", "hooks")
chromium_hooks_folder = os.path.join(SRC, "scripts", "templates", "chromium_hooks")
git_hooks_folder = os.path.join(SRC, ".git", "hooks")
chromium_git_hooks_folder = os.path.join(SRC, ".git", "modules", "chromium", "hooks")

#depot_tools_path = os.path.join(SRC, "chromium/third_party/depot_tools")
# os.environ["PATH"] = os.pathsep.join([os.environ["PATH"], depot_tools_path])
if os.access("build/toolchain.json", os.F_OK) and "DEPOT_TOOLS_WIN_TOOLCHAIN" in os.environ:
  del os.environ["DEPOT_TOOLS_WIN_TOOLCHAIN"]
os.environ["DEPOT_TOOLS_BOOTSTRAP_PYTHON3"]="0"
os.environ["VPYTHON_BYPASS"]= "manually managed python not supported by chrome operations"

def copy_files(src, dst):
  for f in os.listdir(src):
    if not os.access(os.path.join(dst, f), os.R_OK+os.X_OK):
      shutil.copy(os.path.join(src, f), os.path.join(dst, f))

parser = argparse.ArgumentParser()
parser.add_argument("--clobber-out", action="store_true");
parser.add_argument("--download-pgo-profiles", action="store_true");
parser.add_argument("--args-gn")
parser.add_argument("--checkout-os")
parser.add_argument("--target-cpu")

args = parser.parse_args()

def RunHooks():
  global_vars, gn_vars = vivdeps.get_variables(args.checkout_os, args.target_cpu)

  if args.download_pgo_profiles or gn_vars.get("is_official_build", False) in ["true", 1, "1", "True", True]:
    global_vars["checkout_pgo_profiles"] = True

  update_hooks = {}
  if args.args_gn:
    update_hooks["bootstrap-gn"] = ["--args-gn", args.args_gn]

  hooks = vivdeps.VivaldiDeps(variables = global_vars, gn_vars=gn_vars)
  hooks.Load()
  hooks.RunHooks(update_hooks)

if args.clobber_out:
  import shutil
  for build_dir in ["out"]:
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
  if vivdeps.IsAndroidEnabled() and int(os.environ.get("CHROME_HEADLESS",0)):
    print("Cleaning checkout ", SRC)
    subprocess.call(["git", "clean", "-fdx"], cwd=SRC)
    subprocess.call(["git", "submodule", "foreach", "--recursive", "git clean -fdx"], cwd=SRC)

if "CHROME_HEADLESS" not in os.environ:
  if os.access(hooks_folder, os.F_OK):
    copy_files(hooks_folder, git_hooks_folder)
  if os.access(chromium_hooks_folder, os.F_OK):
    copy_files(chromium_hooks_folder, chromium_git_hooks_folder)

RunHooks()
