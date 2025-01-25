
import sys, os
import logging
import shlex

from git_urls import BASE_URL

logging.getLogger().setLevel(logging.ERROR)

SRC = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
depot_tools_path = os.path.join(SRC, "chromium/third_party/depot_tools")

sys.path.append(depot_tools_path)

os.environ["PATH"] = os.pathsep.join([os.environ["PATH"], depot_tools_path])
os.environ["DEPOT_TOOLS_BOOTSTRAP_PYTHON3"]="0"
os.environ["VPYTHON_BYPASS"]= "manually managed python not supported by chrome operations"

os.environ["DEPOT_TOOLS_COLLECT_METRICS"] = "0"
os.environ["DEPOT_TOOLS_METRICS"] = "0"

import gclient
import detect_host_arch
from gclient_scm import CipdRoot, GcsRoot
from gclient_utils import SplitUrlRevision, ExecutionQueue, freeze
from third_party.repo.progress import Progress

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

def IsAndroidEnabled():
  if "ANDROID_ENABLED" in os.environ:
    return True
  return os.access(os.path.join(SRC,".enable_android"), os.F_OK)

def IsReclientEnabled(host_os):
  if host_os not in ["win", "mac", "linux"]:
    return False
  if "FETCH_RECLIENT" in os.environ:
    return True
  return os.access(os.path.join(SRC,".enable_gn_reclient"), os.F_OK)

def IsIOSEnabled():
  if "IOS_ENABLED" in os.environ:
    return True
  return os.access(os.path.join(SRC,".enable_ios"), os.F_OK)

def get_variables(a_checkout_os=None, a_target_cpu=None):
  host_os = VivaldiBaseDeps.DEPS_OS_CHOICES.get(sys.platform, 'linux')
  if host_os == "unix":
    host_os = "linux"
  checkout_os = a_checkout_os or host_os
  checkout_cpu = ["x64"]
  if IsAndroidEnabled():
    checkout_os = "android"
    checkout_cpu = ["arm"]
  if IsIOSEnabled():
    checkout_os = "ios"
    checkout_cpu = ["arm64", "x64"]

  gnvars = GetGNVars()
  if "target_cpu" in gnvars:
    checkout_cpu = [gnvars["target_cpu"]]
  elif a_target_cpu:
    gnvars["target_cpu"] = a_target_cpu
    checkout_cpu = [a_target_cpu]
  else:
    if checkout_os == "win":
      checkout_cpu.append("x86")
    elif checkout_os == "mac":
      checkout_cpu.append("arm64")
    elif checkout_os == "android":
      checkout_cpu.extend(["arm64", "x86"])
    elif checkout_os == "linux":
      checkout_cpu.extend(["arm64", "arm", "x86"])

  checkout_os = "checkout_"+checkout_os


  global_vars = {
    "checkout_src_internal": False,
    checkout_os: True,
    "checkout_pgo_profiles": False,
    "build_with_chromium": True,
    "checkout_reclient": IsReclientEnabled(host_os),
  }
  for x in checkout_cpu:
    global_vars["checkout_"+x] = True

  if checkout_os in ["checkout_linux"] and "arm" in checkout_cpu:
    global_vars["checkout_x86"]= True # Linux Arm 32 requires x86 cpu for some targets

  if checkout_os in ["checkout_android", "checkout_linux"] :
    global_vars["checkout_linux"]= True # Always checking out android on linux systems
    global_vars["checkout_x64"]= True # Always checking out android on linux x64 systems (also Linux arm builds)

  for an_os in ["telemetry_dependencies"]:
    global_vars.setdefault("checkout_"+an_os, False)

  return (global_vars, gnvars)

def get_chromium_variables():
  global_vars, _gnvars = get_variables()
  if BASE_URL:
    global_vars.setdefault('chromium_git', BASE_URL)
    global_vars.setdefault('android_git', BASE_URL +'/android')
    global_vars.setdefault('aomedia_git', BASE_URL +'/aomedia')
    global_vars.setdefault('boringssl_git', BASE_URL +'/boringssl')
    global_vars.setdefault('dawn_git', BASE_URL +'/dawn')
    global_vars.setdefault('pdfium_git', BASE_URL +'/pdfium')
    global_vars.setdefault('quiche_git', BASE_URL +'/quiche')
    global_vars.setdefault('skia_git', BASE_URL +'/skia')
    global_vars.setdefault('swiftshader_git', BASE_URL +'/swiftshader')
    global_vars.setdefault('webrtc_git', BASE_URL +'/webrtc')
    global_vars.setdefault('betocore_git', BASE_URL +'/beto-core')

  global_vars["checkout_src_internal"]=False
  global_vars["checkout_nacl"]=False
  global_vars["git_dependencies"]="DEPS"

  return global_vars

class VivaldiBaseDeps(gclient.GitDependency):
  DEPS_OS_CHOICES = gclient.GClient.DEPS_OS_CHOICES

  def __init__(self, root_dir, target_os=None, variables={}, name=None, preloaded_content=None, preloaded_subdeps=None, gn_vars={}):
    self._root_dir = root_dir
    self._target_os = target_os or [self.DEPS_OS_CHOICES.get(sys.platform, 'unix')]
    self._target_cpu = [gn_vars.get("target_cpu")] if "target_cpu" in gn_vars else detect_host_arch.HostArch()
    self._cipd_root = None
    self._preloaded_deps_content = preloaded_content
    self._preloaded_subdeps = preloaded_subdeps
    self._sub_deps={}
    self._gcs_root=None

    class VivDepsOptions(object):
      process_all_deps = False
      verbose = False
      nohooks = True
      break_repo_locks=False
      force=False
      reset=False
    self._options = VivDepsOptions()

    super(VivaldiBaseDeps, self).__init__(
        parent=None,
        name= name or ".",
        url=None,
        managed=False,
        custom_deps=None,
        custom_vars=variables,
        custom_hooks=None,
        deps_file=os.path.join(self._root_dir, "DEPS"),
        should_process=True,
        should_recurse=False,
        relative=None,
        condition=None,
        print_outbuf=True,
        protocol=(BASE_URL or "ssh:").split(":")[0],
        git_dependencies_state="DEPS",
        )

  @property
  def root_dir(self):
    return self._root_dir

  @property
  def target_os(self):
    return  tuple(self._target_os)

  @property
  def target_cpu(self):
    return self._target_cpu

  def get_builtin_vars(self):
    our_vars = super(VivaldiBaseDeps, self).get_builtin_vars()
    our_vars.update(self.viv_variables)

    for n,v in list(our_vars.items()):
      if n.startswith("checkout_"):
        our_vars[n+"_str"] = str(v)

    return our_vars

  def GetCipdRoot(self):
    if not self._cipd_root:
      self._cipd_root = CipdRoot(
          self.root_dir,
          'https://chrome-infra-packages.appspot.com')
    return self._cipd_root

  def GetGcsRoot(self):
    if not self._gcs_root:
        self._gcs_root = GcsRoot(self.root_dir)
    return self._gcs_root

  def Load(self, recurse=False):
    self.ParseDepsFile()
    if recurse and isinstance(self, ChromiumDeps):
      for subname, deps_file  in self.recursedeps.items():
        if subname == "src-internal":
          continue
        if subname.startswith("src/"):
          subname=subname[4:]
        sub_dep = ChromiumDeps(root_dir=os.path.join(self.root_dir, subname),
                               variables=dict(self.get_vars()),
                              preloaded_content=(self._preloaded_subdeps or {}).get(subname, None))
        sub_dep.Load(recurse=False)
        self._sub_deps[subname] = sub_dep

  def _deps_to_objects(self, deps, use_relative_paths):
    new_deps = {}
    for name, dep_value in deps.items():
      if name.startswith("src/"):
        name = name[4:]
      new_deps[name] = dep_value
    return super(VivaldiBaseDeps, self)._deps_to_objects(new_deps, use_relative_paths)

  def RunHooks(self, update_hooks=None):
    for x in self.deps_hooks:
      if update_hooks and update_hooks.get(x.name, None):
        updater = update_hooks.get(x.name)
        if callable(updater):
          x._action = freeze(updater(x.action))
        else:
          x._action = freeze(list(x.action) + updater)
      x.run()

  def UpdateModules(self, cipd_list=None, exclude_modules=[], cipd_only=False, exclude_cipd=[]):
    if not self.dependencies:
      return

    anything_to_update = False
    for x in self.dependencies:
      x.really_should_process = x.should_process
      if not x.really_should_process:
        continue
      if x.GetScmName() == "git":
        if cipd_only or x.name in exclude_modules:
          x.really_should_process = False
        else:
          url, ref = SplitUrlRevision(x.url)
          if not url.endswith(".git"):
            x.set_url(url + ".git@"+ref)

      elif x.GetScmName() == "cipd":
        if cipd_list and x.name.split(":")[0] not in cipd_list:
          x.really_should_process = False
        if x.name.split(":")[0] in exclude_cipd:
          x.really_should_process = False
      elif x.GetScmName() == "gcs":
        if cipd_list and x.name.split(":")[0] not in cipd_list:
          x.really_should_process = False
        if x.name.split(":")[0] in exclude_cipd:
          x.really_should_process = False
      else:
        x.really_should_process = False
      x._should_process = x.really_should_process
      anything_to_update = anything_to_update or x.really_should_process

    if not anything_to_update:
      return

    revision_overrides = {}
    patch_refs = {}
    target_branches = {}
    pm = Progress('Syncing projects', 1)
    work_queue = ExecutionQueue(
        5, pm, ignore_requirements=True,
        verbose=True)
    self._options.verbose = True
    for s in self.dependencies:
      if s.really_should_process:
        work_queue.enqueue(s)
    work_queue.flush(revision_overrides, "update", [], options=self._options,
                     patch_refs=patch_refs, target_branches=target_branches,
                     skip_sync_revisions=None)
    if self._cipd_root:
      self._cipd_root.run("update")
    if self._gcs_root:
      self._gcs_root.clobber_deps_with_updated_objects(self.name)


    for subname, sub_deps in self._sub_deps.items():
      if subname in exclude_modules:
        continue
      sub_deps.UpdateModules(cipd_list=cipd_list, exclude_modules=exclude_modules, cipd_only=cipd_only)

  def GetModuleList(self, prefix=None):
    def prefixed_path(p):
      return (prefix+"/"+p if prefix else p).replace("\\","/")
    modules = {}
    for x in self.dependencies:
      if x.GetScmName() != "git" or not x.should_process:
        continue
      url, ref = SplitUrlRevision(x.url)
      assert url.startswith(BASE_URL), 'The URL "%s" is not a valid Vivaldi Git URL, please update variables.' % url
      ref = {
          "url": url,
          "commit": ref,
        }
      if prefix:
        ref["local_mod"] = x.name.replace("\\","/")
      modules[prefixed_path(x.name)] = ref

    for subname, sub_deps in self._sub_deps.items():
      modules.update(sub_deps.GetModuleList(prefix=prefixed_path(subname)))
    return modules

class VivaldiDeps(VivaldiBaseDeps):
  def __init__(self, root_dir=SRC, variables=get_variables()[0], gn_vars=get_variables()[1], **kwargs):
    self.viv_variables = dict(variables)
    return super(VivaldiDeps, self).__init__(root_dir = root_dir, variables=variables, gn_vars=gn_vars, **kwargs)

class ChromiumDeps(VivaldiBaseDeps):
  def __init__(self, root_dir=os.path.join(SRC, "chromium"), variables=get_chromium_variables(), gn_vars=get_variables()[1],  **kwargs):
    self.viv_variables = dict(variables)
    return super(ChromiumDeps, self).__init__(root_dir = root_dir, variables=variables, gn_vars=gn_vars, **kwargs)
