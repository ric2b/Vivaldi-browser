import sys, os, os.path
import subprocess
import read_deps_file as deps_utils
import datetime
import platform

script_name = sys.argv[0]
if not os.path.isabs(script_name):
  script_name = os.path.abspath(os.path.join(os.getcwd(), script_name ))

sourcedir = os.path.abspath(os.path.join(os.path.split(script_name)[0],".."))

prefix_name = os.path.split(sourcedir)[1]

workdir = os.path.abspath(os.path.join(sourcedir,".."))
extra_subprocess_flags = {}
if platform.system() == "Windows":
  try:
    import win32con
    extra_subprocess_flags["creationflags"] = win32con.NORMAL_PRIORITY_CLASS
  except:
    pass

def fix_action(str):
  if str.startswith("vivaldi/") or str.startswith("vivaldi\\"):
    print "replacing vivaldi with %s for %s" % (prefix_name, str)
    sys.stdout.flush()
    str = str.replace("vivaldi", prefix_name,1)
  return str

def RunHooks(hooks, cwd, env=None, prefix_name=None):
  if not hooks:
    return
  for hook in hooks:
    if "action" in hook:
      action = hook["action"]
      if prefix_name and prefix_name != "vivaldi":
        action = [fix_action(x) for x in action]
      print 'running action "%s" in %s' %(action,cwd)
      sys.stdout.flush()
      if subprocess.call(action, cwd=cwd, env=env, shell=(os.name=="nt"),
                         **extra_subprocess_flags) != 0:
        raise BaseException("Hook failed")

if "--clobber-out" in sys.argv:
  import shutil
  build_dir = "out"
  out_dir = os.path.join(sourcedir,build_dir)
  if os.access(out_dir, os.R_OK):
    start_time = datetime.datetime.now()
    print "Deleting ", out_dir
    sys.stdout.flush()
    for _ in range(4):
      try:
        shutil.rmtree(out_dir)
      except:
        pass
      if not os.access(out_dir, os.R_OK):
        break
      print "New delete try after", (datetime.datetime.now()-start_time).total_seconds(), "seconds"
      sys.stdout.flush()
    if os.access(out_dir, os.R_OK):
      raise Exception("Could not delete out directory")
    stop_time = datetime.datetime.now()
    print "Deleted", out_dir, "in", (stop_time-start_time).total_seconds(), "seconds"
    sys.stdout.flush()

deps_content = deps_utils.GetDepsContent("DEPS")
(deps, deps_os, include_rules, skip_child_includes, hooks,
   deps_vars, recursion) = deps_content

RunHooks(hooks, cwd=workdir, prefix_name=prefix_name)
