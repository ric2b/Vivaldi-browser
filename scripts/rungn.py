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
gn_name = "gn.exe" if is_windows else "gn"
bin_dir = os.path.join(sourcedir, "build", "bin",)
gn_path =  os.path.join(bin_dir, gn_name)
gn_workdir = os.path.join(sourcedir, "outgn")
gn_releasedir = os.path.join(gn_workdir, "Release")

parser = argparse.ArgumentParser()
parser.add_argument("--refresh", action="store_true");
parser.add_argument("--name");
parser.add_argument("--no-ide", action="store_true");
parser.add_argument("--ide")
parser.add_argument("--ide-all", action="store_true");
parser.add_argument("args", nargs=argparse.REMAINDER);

args = parser.parse_args()

extra_subprocess_flags = {}
if is_windows:
  import win32con
  extra_subprocess_flags["creationflags"] = win32con.NORMAL_PRIORITY_CLASS

if args.refresh or not os.access(gn_path, os.F_OK):
  start_time = datetime.datetime.now()
  try:
    os.makedirs(bin_dir)
  except:
    pass
  bootstrap_env = dict(os.environ)
  for x in ["VIVALDI_SIGNING_KEY", "VIVALDI_SIGN_EXECUTABLE", "GN_DEFINES"]:
    if x in bootstrap_env:
      del bootstrap_env[x]
  full_bootstrap = True
  if os.access(os.path.join(gn_workdir, "Release", "build.ninja"), os.F_OK):
    full_bootstrap = False
    if subprocess.call(["ninja",
        "-C", gn_releasedir,
        gn_name,
        ],
        cwd = sourcedir,
        env = bootstrap_env,
        shell = is_windows,
        **extra_subprocess_flags
      ) != 0:
      full_bootstrap = True
    else:
      shutil.copy2(os.path.join(gn_releasedir, gn_name), gn_path)
  if full_bootstrap:
    if subprocess.call(["python",
        os.path.join(sourcedir, "chromium", "tools", "gn",
                     "bootstrap", "bootstrap.py"),
        "--output", gn_path,
        "--no-clean",
        "--outwork", gn_workdir,
        ],
        cwd = sourcedir,
        env = bootstrap_env,
        shell = is_windows,
        **extra_subprocess_flags
      ) != 0:
        sys.exit(1)
  stop_time = datetime.datetime.now()
  print "Refreshed GN parser in", (stop_time-start_time).total_seconds(), "seconds"
  sys.stdout.flush()

if args.refresh or not args.args:
  is_builder = os.environ.get("CHROME_HEADLESS", 0) != 0
  ide_profile_name = args.name or os.path.basename(sourcedir)
  ide_kind = args.ide or os.environ.get("GN_IDE", None)
  produce_ide = not args.no_ide and ide_kind != "None" and not is_builder
  if produce_ide and not ide_kind:
    if platform.system() == "Windows":
      ide_kind = "vs2015"
    elif platform.system()== "darwin":
      ide_kind = "xcode"
    else:
      ide_kind = "eclipse"

  profiles = [
      # args MUST be unquoted. are passed directly to the command
      ("out/Release", ['--args=is_debug=false']),
    ]
  if not is_builder:
    profiles.append(("out/Debug", ['--args=is_debug=true']))
  if is_windows and is_builder:
    profiles.append(("out/Release_x64",
                    ['--args=is_debug=false target_cpu="x64"']))

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

sys.exit(subprocess.call([gn_path ] + args.args,
        cwd = sourcedir,
        shell = is_windows,
        **extra_subprocess_flags
      ))
