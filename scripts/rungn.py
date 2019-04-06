import sys, os, os.path
import subprocess
import argparse
import platform
import shutil
import datetime

script_name = sys.argv[0]
if not os.path.isabs(script_name):
  script_name = os.path.abspath(os.path.join(os.getcwd(), script_name ))

sourcedir = os.path.abspath(os.path.dirname(os.path.dirname(script_name)))

is_windows = platform.system() == "Windows"
is_linux = platform.system() == "Linux"
is_android = os.access(os.path.join(sourcedir, ".enable_android"), os.F_OK)

os.environ["DEPOT_TOOLS_WIN_TOOLCHAIN"]="0"

# Check python version on Windows
if is_windows:
  version = sys.version_info
  problem_version = (version.major, version.minor) != (2, 7)  or version.micro < 14
  problem_pywin32 = False
  try:
    import win32file
  except:
    problem_pywin32 = True

  if problem_version or problem_pywin32:
    string = ""
    if problem_version:
      string += "Python 2 version 2.7.14 or higher needed (Current version %d.%d.%d)" % (version.major, version.minor, version.micro)
    if problem_pywin32:
      if string:
        string += ", and additionally "
      string += """pypiwin32 must be installed, use "pip install pypiwin32" to install."""
    raise Exception(string)
elif is_linux:
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
parser.add_argument("--official", action="store_true");
parser.add_argument("--static", action="store_true");
parser.add_argument("args", nargs=argparse.REMAINDER);

args = parser.parse_args()

# Need this file to be present
gclient_gni_content = """checkout_nacl=false
checkout_libaom=false
checkout_oculus_sdk=false
build_with_chromium=true
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
  for x in ["VIVALDI_SIGNING_KEY", "VIVALDI_SIGN_EXECUTABLE", "GN_DEFINES"]:
    if x in bootstrap_env:
      del bootstrap_env[x]
  if is_windows:
    def setup_toolchain():
      # Copied from Chromium
      def CallPythonScopeScript(command, **kwargs):
        response = subprocess.check_output(command, **kwargs)

        _globals = {"__builtins__":None}
        _locals = {}
        exec(response, _globals, _locals)

        return _locals

      toolchain_paths = CallPythonScopeScript(
          [sys.executable,
            os.path.join(sourcedir, "chromium", "build", "vs_toolchain.py"),
          "get_toolchain_dir"],
          cwd=sourcedir)
      try:
        os.makedirs(gn_releasedir)
      except:
        pass
      windows_x64_toolchain =  CallPythonScopeScript(
          [sys.executable,
           os.path.join(sourcedir, "chromium", "build", "toolchain",
                        "win", "setup_toolchain.py"),
           toolchain_paths["vs_path"],
           toolchain_paths["sdk_path"],
           toolchain_paths["runtime_dirs"],
           "win",
           "x64",
           "environment.x64",
          ],
          cwd=gn_releasedir)
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
      if subprocess.call(["python",
        os.path.join(sourcedir, "thirdparty", "gn", "build",
                     "gen.py"),
        "--out-path", gn_releasedir,
        ],
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
  print "Refreshed GN parser in", (stop_time-start_time).total_seconds(), "seconds"
  sys.stdout.flush()

gn_defines = os.environ.get("GN_DEFINES", "")
if gn_defines:
  gn_defines = " "+gn_defines

if args.official:
  gn_defines += " is_official_build=true"

if args.static:
  gn_defines += " is_component_build=false"

user_arg_file = os.path.expanduser("~/.gn/args.gn")
if os.access(user_arg_file, os.F_OK):
  if is_windows:
     # convert to gn absolute file system label
    user_arg_file = "/"+user_arg_file.replace("\\", "/")
else:
  user_arg_file = None

if args.refresh or not args.args:
  is_builder = os.environ.get("CHROME_HEADLESS", 0) != 0
  ide_profile_name = args.name or os.path.basename(sourcedir)
  ide_kind = args.ide or os.environ.get("GN_IDE", None)
  produce_ide = not args.no_ide and ide_kind != "None" and not is_builder
  if produce_ide and not ide_kind:
    if platform.system() == "Windows":
      ide_kind = "vs2017"
    elif platform.system()== "darwin":
      ide_kind = "xcode"
    else:
      ide_kind = "eclipse"

  platform_target=' target_os="android"' if is_android else ""

  include_arg = 'import(\"'+ user_arg_file + '\") ' if user_arg_file else ""

  profiles = [
      # args MUST be unquoted. are passed directly to the command
      ("out/Release", ['--args='+ include_arg +'is_debug=false'+platform_target+' ' + gn_defines]),
    ]
  if not is_builder and not args.official:
    profiles.append(("out/Debug", ['--args='+ include_arg +'is_debug=true'+platform_target+' ' + gn_defines]))
  if is_windows and is_builder:
    profiles.append(("out/Release_x64",
                    ['--args='+ include_arg +'is_debug=false target_cpu="x64"'+gn_defines]))

  if args.ide_all:
    ide_profile_name += "_all"

  for (target, target_args ) in profiles:
    ide_args = []
    if produce_ide:
      ide_args = [
        "--ide="+ide_kind,
        #"--no-deps",
        ]
      if not args.ide_all:
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
        **extra_subprocess_flags
      ) != 0:
        sys.exit(1)


if not args.args:
  sys.exit(0)

def fix_arg(x):
  if x.startswith('--args="'):
    if x[-1] != '"':
      return x
    return x[:8]+include_arg+ x[8:-1]+ gn_defines+'"'
  if x.startswith('--args='):
    return x[:7]+include_arg+ x[7:]+ gn_defines
  return x

args_updated = [fix_arg(x) for x in args.args]

sys.exit(subprocess.call([gn_path ] + args_updated,
        cwd = sourcedir,
        shell = is_windows,
        **extra_subprocess_flags
      ))
