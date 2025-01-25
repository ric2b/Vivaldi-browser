import sys, os, os.path
import subprocess
import argparse
import platform
import shutil
import datetime
import json

sourcedir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

is_windows = platform.system() == "Windows"
is_linux = platform.system() == "Linux"
is_mac = platform.system() == "Darwin"
is_mac_arm64 = is_mac and platform.processor() == "arm"
is_android = os.access(os.path.join(sourcedir, ".enable_android"), os.F_OK)
is_ios = os.access(os.path.join(sourcedir, ".enable_ios"), os.F_OK)
use_gn_ide_all = os.access(os.path.join(sourcedir, ".enable_gn_all_ide"), os.F_OK)
use_gn_unique_name = os.access(os.path.join(sourcedir, ".enable_gn_unique_name"), os.F_OK)
use_gn_reclient = os.access(os.path.join(sourcedir, ".enable_gn_reclient"), os.F_OK)

# Check python version
try:
  if is_windows:
    import check_python
    check_python.CheckPythonInstall()
except:
  pass

GN_env = dict(os.environ)

GN_env["PATH"] = os.pathsep.join([p for p in GN_env["PATH"].split(os.pathsep) if "depot_tools" not in p])

if is_linux:
  # Add path for downloaded clang
  os.environ["PATH"] = os.pathsep.join([
      os.path.join(sourcedir, "chromium", "third_party", "llvm-build",
                   "Release+Asserts", "bin"),
      os.environ["PATH"]
      ])

gn_name = "gn.exe" if is_windows else "gn"
bin_dir = os.path.join(sourcedir, "build", "bin",)
gn_path =  os.path.join(bin_dir, gn_name)
gn_workdir = os.path.join(sourcedir, "outgn")
gn_releasedir = os.path.join(gn_workdir, "bootstrap_Release")

parser = argparse.ArgumentParser()
parser.add_argument("--refresh", action="store_true");
parser.add_argument("--bootstrap", action="store_true");
parser.add_argument("--name");
parser.add_argument("--no-ide", action="store_true");
parser.add_argument("--ide")
parser.add_argument("--ide-all", action="store_true");
parser.add_argument("--ide-unique-name", action="store_true");
parser.add_argument("--official", action="store_true");
parser.add_argument("--static", action="store_true");
parser.add_argument("--no-hermetic", action="store_true");
parser.add_argument("--args-gn")
parser.add_argument("--filter-project")
parser.add_argument("--extra-debug", action="append", default=[])
parser.add_argument("--extra-release", action="append", default=[])
parser.add_argument("args", nargs=argparse.REMAINDER);

args = parser.parse_args()

# Need this file to be present
gclient_gni_content = """checkout_nacl=false
checkout_oculus_sdk=false
checkout_openxr=false
build_with_chromium=true
checkout_google_benchmark=false
checkout_ios_webkit=false
generate_location_tags = false
checkout_src_internal = false
"""

gclient_gni_file_name = os.path.join(sourcedir, "chromium/build/config/gclient_args.gni")
if not os.access(gclient_gni_file_name, os.F_OK) or open(gclient_gni_file_name, "r").read() != gclient_gni_content:
  with open(gclient_gni_file_name, "w") as f:
    f.write(gclient_gni_content)

extra_subprocess_flags = {}
if is_windows:
  try:
    import win32con
    extra_subprocess_flags["creationflags"] = win32con.NORMAL_PRIORITY_CLASS
  except:
    pass

if args.refresh or args.bootstrap or not os.access(gn_path, os.F_OK):
  start_time = datetime.datetime.now()
  try:
    os.makedirs(bin_dir)
  except:
    pass
  bootstrap_env = dict(os.environ)

  for x in ["GN_DEFINES", "DEPOT_TOOLS_WIN_TOOLCHAIN"]:
    if x in bootstrap_env:
      del bootstrap_env[x]
  if is_windows:
    def setup_toolchain():
      toolchain_args = []
      if os.access("build/toolchain.json", os.F_OK) and not args.no_hermetic:
        toolchain_args += ["--toolchain-json", os.path.join(sourcedir, "build/toolchain.json")]
      else:
        bootstrap_env["DEPOT_TOOLS_WIN_TOOLCHAIN"]="0"

      # Copied from Chromium
      def CallPythonScopeScript(command, **kwargs):
        response = subprocess.check_output(command, **kwargs).decode("utf-8")

        _globals = {"__builtins__":None}
        _locals = {}
        exec(response, _globals, _locals)

        return _locals

      toolchain_paths = CallPythonScopeScript(
          [sys.executable,
            os.path.join(sourcedir, "chromium", "build", "vs_toolchain.py"),] +
            toolchain_args + ["get_toolchain_dir"],
          cwd=sourcedir,
          env=bootstrap_env)
      try:
        os.makedirs(gn_releasedir)
      except:
        pass
      windows_x64_toolchain =  CallPythonScopeScript(
          [sys.executable,
           os.path.join(sourcedir, "chromium", "build", "toolchain",
                        "win", "setup_toolchain.py"),] +
           toolchain_args + [
           toolchain_paths["vs_path"],
           toolchain_paths["sdk_path"],
           toolchain_paths["runtime_dirs"],
           "win",
           "x64",
           "environment.x64",
          ],
          cwd=gn_releasedir,
          env=bootstrap_env)
      # End copied from Chromium
      return windows_x64_toolchain
    windows_x64_toolchain = setup_toolchain()
    bootstrap_env["PATH"] = windows_x64_toolchain["vc_bin_dir"] + os.pathsep +\
                            bootstrap_env["PATH"]

  def ninja_gn():
    if subprocess.call(["ninja",
        "-C", gn_releasedir,
        gn_name,
        ],
        cwd = sourcedir,
        env = bootstrap_env,
        shell = is_windows,
        **extra_subprocess_flags
      ) != 0:
      return False
    shutil.copy2(os.path.join(gn_releasedir, gn_name), gn_path)
    return True
  full_bootstrap = True
  if not args.bootstrap and os.access(os.path.join(gn_releasedir, "build.ninja"), os.F_OK):
    full_bootstrap = False
    full_bootstrap = not ninja_gn()

  if full_bootstrap:
    def do_full_bootstrap():
      extra_bootstrap_args = []
      if os.access(os.path.join(sourcedir, "thirdparty", "gn", "src", "last_commit_position.h"), os.F_OK):
        extra_bootstrap_args.append("--no-last-commit-position")
      if is_windows:
        extra_bootstrap_args.extend(["--platform", "msvc"])
      elif is_linux:
        extra_bootstrap_args.append("--allow-warnings")
      if subprocess.call(["python3",
        os.path.join(sourcedir, "thirdparty", "gn", "build",
                     "gen.py"),
        "--out-path", gn_releasedir.replace("\\", "/"),
        ] + extra_bootstrap_args,
        cwd = sourcedir,
        env = bootstrap_env,
        shell = is_windows,
        **extra_subprocess_flags
      ) != 0:
        return 1
      if not ninja_gn():
        return 1
      return 0

    if do_full_bootstrap() != 0:
      shutil.rmtree(gn_workdir, ignore_errors=True)
      if is_windows:
        setup_toolchain()
      if do_full_bootstrap() != 0:
        sys.exit(1)
  stop_time = datetime.datetime.now()
  print("Refreshed GN parser in", (stop_time-start_time).total_seconds(), "seconds")
  sys.stdout.flush()

gn_defines = os.environ.get("GN_DEFINES", "")
if gn_defines:
  if not int(os.environ.get("CHROME_HEADLESS", 0)):
    print("Using the GN_DEFINES environment variable is deprecated.\n"
          "Use the files args.gn, args.Release.gn, and/or args.Debug.gn in the source root dir instead.")
  gn_defines = " "+gn_defines

# Remove Google API client ID/secret apparently not needed
# and removing them is another way to disable Account Consistency
gn_defines += ' google_default_client_id="" google_default_client_secret=""'

if args.official:
  gn_defines += " is_official_build=true"

if args.static or is_ios: # IOS requires static linking
  gn_defines += " is_component_build=false"

if args.no_hermetic:
  gn_defines += " use_hermetic_toolchain=false"

if use_gn_reclient:
  # Temporary disabling of reclient on Windows, because problems with Linux clang 20 cross-compile
  gn_defines += ' vivaldi_enable_reclient=true'

if args.refresh or not args.args:
  is_builder = os.environ.get("CHROME_HEADLESS", 0) != 0
  ide_profile_name = args.name or os.path.basename(sourcedir)
  if (args.ide_unique_name or use_gn_unique_name) and not args.name and ide_profile_name.lower() == "vivaldi":
    test_drive, test_dir = os.path.splitdrive(os.path.dirname(sourcedir))
    test_dir = test_dir.replace("\\","/")
    while test_dir and test_dir != "/":
      dirname = os.path.basename(test_dir)
      ide_profile_name = dirname+"_"+ide_profile_name
      if dirname.lower() not in ["src", "vivaldi"]:
        break
      test_dir = os.path.dirname(test_dir)
    if (test_dir == "/" or not test_dir) and test_drive:
      ide_profile_name = test_drive[0]+"_"+ide_profile_name

  ide_kind = args.ide or os.environ.get("GN_IDE", None)
  produce_ide = not args.no_ide and ide_kind != "None" and not is_builder
  if produce_ide and not ide_kind:
    if platform.system() == "Windows":
      ide_kind = "vs2019"
    elif platform.system()== "Darwin":
      ide_kind = "xcode"
    else:
      ide_kind = "eclipse"

  if is_android:
    platform_target=' target_os="android"'
  elif is_ios:
    platform_target=' target_os="ios"'
  else:
    platform_target=""

  def include_arg(mode):
    user_arg_files = []
    user_arg_files.append(os.path.expanduser("~/.gn/args.gn"))
    if args.args_gn:
      user_arg_files.append(args.args_gn)
    user_arg_files.append(os.path.abspath("args.gn"))
    user_arg_files.append(os.path.abspath("args.{}.gn".format(mode)))

    tmp_args = []
    for f in user_arg_files:
      if f and os.access(f, os.F_OK):
        if is_windows:
           # convert to gn absolute file system label
          f = f.replace("\\", "/")
          if not args.args_gn:
            f= "/"+f
        tmp_args.append(f)
    user_arg_files = tmp_args

    return "".join(['import(\"'+ f + '\") ' for f in user_arg_files])

  profiles = [
      # args MUST be unquoted. are passed directly to the command
      ("out/"+project, ['--args='+ include_arg(project) +'is_debug=false'+platform_target+' ' + gn_defines])
    for project in ["Release"] + args.extra_release]
  if not is_builder and not args.official:
    for project in ["Debug"] + args.extra_debug:
      profiles.append(("out/"+project, ['--args='+ include_arg(project) +'is_debug=true'+platform_target+' ' + gn_defines]))

  if args.ide_all or use_gn_ide_all:
    ide_profile_name += "_all"

  for (target, target_args ) in profiles:
    ide_args = []
    if produce_ide:
      ide_args = [
        "--ide="+ide_kind,
        #"--no-deps",
        ]
      if args.filter_project:
        ide_args.append('--filters='+args.filter_project)
      elif not (args.ide_all or use_gn_ide_all):
        ide_args.append('--filters=:vivaldi')

      name = ide_profile_name+"_"+os.path.basename(target)
      if ide_kind.startswith("vs"):
        ide_args.append("--sln="+name)
      elif ide_kind == "xcode":
        ide_args.append("--workspace="+name)
    if subprocess.call([gn_path,
        "gen",
        target,
        ] +ide_args + target_args,
        cwd = sourcedir,
        shell = is_windows,
        env = GN_env,
        **extra_subprocess_flags
      ) != 0:
        sys.exit(1)


if not args.args:
  sys.exit(0)

def fix_arg(x):
  if x.startswith('--args="'):
    if x[-1] != '"':
      return x
    return x + " " + gn_defines+'"'
  if x.startswith('--args='):
    return x + " " + gn_defines
  return x

args_updated = [fix_arg(x) for x in args.args]

sys.exit(subprocess.call([gn_path ] + args_updated,
        cwd = sourcedir,
        shell = is_windows,
        **extra_subprocess_flags
      ))
