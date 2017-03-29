import sys, os, os.path
import subprocess

script_name = sys.argv[0]
if not os.path.isabs(script_name):
	script_name = os.path.abspath(os.path.join(os.getcwd(), script_name ))

sourcedir = os.path.abspath(os.path.join(os.path.split(script_name)[0],".."))

prefix_name = os.path.split(sourcedir)[1]

workdir = os.path.abspath(os.path.join(sourcedir,".."))

sys.path.append(os.path.join(sourcedir, "chromium", "tools", "deps2git"))
import deps_utils

def fix_action(str):
	if str.startswith("vivaldi/") or str.startswith("vivaldi\\"):
		print "replacing vivaldi with %s for %s" % (prefix_name, str)
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
			if subprocess.call(action, cwd=cwd, env=env, shell=(os.name=="nt")) != 0:
				raise BaseException("Hook failed")

if "--clobber-out" in sys.argv:
  import shutil
  build_dir = "out"

  out_dir = os.path.join(sourcedir,build_dir)
  if os.access(out_dir, os.R_OK):
    print "Deleting ", out_dir
    for _ in range(3):
      shutil.rmtree(out_dir)
      if not os.access(out_dir, os.R_OK):
        break
    if os.access(out_dir, os.R_OK):
      raise Exception("Could not delete out directory")
  

deps_content = deps_utils.GetDepsContent("DEPS")
(deps, deps_os, include_rules, skip_child_includes, hooks,
   deps_vars, recursion) = deps_content

RunHooks(hooks, cwd=workdir, prefix_name=prefix_name)
