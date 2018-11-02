'''
Created on 27. sep. 2013

@author: yngve
'''

import subprocess
import os.path
import copy
import deps_utils
import re
import sys

class Git:
  def __init__(self,source_dir=None, url=None, base_url=None, base_branch=None,
               source_branch=None, target_branch=None, dryrun=None,
               inherit=None, submodule=None):
    if inherit:
      self.root = source_dir or inherit.root
      self.base_url = base_url or inherit.base_url
      self.base_branch = base_branch or inherit.base_branch
      self.source_branch = source_branch or inherit.source_branch
      self.target_branch = target_branch or inherit.target_branch
      self.dryrun = (dryrun == True) if dryrun != None else inherit.dryrun
      if submodule:
        self.root = os.path.join(self.root, submodule)
        for origin in self.GitCommand("remote", ["-v"]):
          name, url = origin.split()[0:2]
          if name == "origin":
            self.url = url
      else:
        self.url=url or inherit.url
    else:
      self.root = source_dir
      self.url=url
      self.base_url = base_url
      self.base_branch = base_branch
      self.source_branch = source_branch
      self.target_branch = target_branch
      self.dryrun = (dryrun == True)

    assert(self.root)

    self.parent = os.path.split(self.root)[0]
    self.checkout_dir = os.path.split(self.root)[1]

    self.relative_url_prefix = []
    if self.base_url:
      prefix = ""
      path = self.url
      base = self.base_url[0:-1] if self.base_url[-1] == "/" else self.base_url
      while path.startswith(base+"/"):
        path = path.rpartition("/")[0]
        prefix = prefix + "../"
        self.relative_url_prefix.append((path,prefix))

  def __get_wd(self, module=None, cwd= None, no_cwd=False):
    if no_cwd:
      return None
    if not cwd:
      cwd = os.path.join(self.root, module) if module else self.root
    elif module:
      cwd = os.path.join(cwd, module)
    return cwd

  def getroot(self):
    return self.root

  def GitCommand(self, command, args, module=None, cwd= None, no_cwd=False,
                 raw = False, dryrun_response="", verbose = True):

    command = ["git", command]+ args

    cwd = self.__get_wd(module, cwd, no_cwd)

    print cwd, " (",module,"): ", " ".join(command)
    sys.stdout.flush()
    try:
      if not self.dryrun:
        output = subprocess.check_output(command, cwd = cwd)
      else:
        output = dryrun_response

    except:
      print 'Exception for "%s"' %(" ".join(command))
      sys.stdout.flush()
      raise

    if verbose:
      print output
    sys.stdout.flush()
    return output if raw else [x.strip() for x in output.splitlines()]

  def GitCommandPipe(self, command, args, module=None, cwd= None, no_cwd=False):

    command = ["git", command]+ args

    cwd = self.__get_wd(module, cwd, no_cwd)

    print cwd," (",module,"): ", " ".join(command)
    sys.stdout.flush()
    try:
      if not self.dryrun:
        pipe = subprocess.Popen(command, cwd = cwd,
              # stdout=subprocess.PIPE
              )
      else:
        pipe = None

    except:
      print 'Exception for "%s"' %(" ".join(command))
      sys.stdout.flush()
      raise

    return pipe

  def GitCommandStdout(self, command, args, module=None, cwd= None,
                       no_cwd=False, idle_timout=900, max_timeout = 0):

    pipe = self.GitCommandPipe(command, args, module=module, cwd= cwd,
                               no_cwd=no_cwd)

    # TODO: implement timeout handling
    if not pipe and not self.dryrun:
      print 'Exception for "%s"' %(" ".join(command))
      sys.stdout.flush()
      raise Exception("starting command failed")

    ret = pipe.wait() if not self.dryrun else 0
    print "Command returned", ret
    sys.stdout.flush()
    return ret

  def FetchSource(self, update_submodules=True):

    try:
      line = self.GitCommand("rev-parse", [
                        #"--show-toplevel"
                        "--is-inside-work-tree",# "--"
                        ],
                dryrun_response="true")[0]
      checked_out = line == "true"
    except:
      checked_out = False

    if not checked_out:
      if self.GitCommandStdout("clone", ["--recursive", self.url,
                               self.checkout_dir], cwd=self.parent) != 0:
        raise Exception("Cloning %s into %s failed" %(self.url, self.root))
    else:
      for origin in self.GitCommand("remote", ["-v"]):
        name, url = origin.split()[0:2]
        if name == "origin":
          if url != self.url:
            self.GitCommandStdout("remote", ["set-url", "origin", self.url]);
          break

      self.GitCommandStdout("reset", ["--hard"])
      self.GitCommandStdout("clean", ["-fd"])
      if update_submodules:
        self.GitCommandStdout("submodule", ["sync"])
        self.GitCommandStdout("submodule", ["foreach", "--recursive",
                                          "git reset --hard"])
        self.GitCommandStdout("submodule", ["foreach", "--recursive",
                                          "git clean -fd"])
      if self.GitCommandStdout("fetch", ["--recurse-submodules=yes",
                                         "origin"],) != 0:
        raise Exception("Fetching for %s failed" %(self.root,))

    if  self.target_branch and self.base_branch:
      if self.GitCommandStdout("checkout", ["-B", self.target_branch,
                                            self.base_branch], ) != 0:
        raise Exception("checking out %s for %s failed" %
                            (self.target_branch, self.root,))
    elif self.base_branch:
      if self.GitCommandStdout("checkout", [self.base_branch], ) != 0:
        raise Exception("checking out %s for %s failed" %
                            (self.target_branch, self.root,))

    if update_submodules:
      self.GitCommandStdout("submodule", ["sync"])
      if self.GitCommandStdout("submodule", ["update", "--init",
                                           "--recursive"], ) != 0:
        raise Exception("checking out submodules for %s failed" %(self.root,))


  def Checkout(self, revision, module = None, cwd=None, pull=False,
               update_submodules=True, only_commit=False):
    if self.GitCommandStdout("checkout", (["--detach"] if only_commit else []) +
                             [revision, "--"]  , module=module, cwd= cwd) != 0:
      raise Exception("checking out %s for %s/%s failed" %
                            (revision, self.root, module if module else ""))
    if pull and self.GitCommandStdout("pull", ["--ff-only", "origin", revision],
                                      module=module, cwd= cwd) != 0:
      raise Exception("Pulling %s for %s/%s failed" %
                            (revision, self.root, module if module else ""))

    if not module and update_submodules and self.GitCommandStdout("submodule",
            ["update", "--init", "--recursive"], ) != 0:
      raise Exception("checking out submodules for %s failed" %(self.root,))

  def Pull(self, module = None, cwd=None, update_submodules=True):
    if self.GitCommandStdout("pull", ["--ff-only"], module=module,
                             cwd= cwd) != 0:
      raise Exception("Pulling current branch for %s/%s failed" %
                            (self.root, module if module else ""))

    if (not module and update_submodules and
        self.GitCommandStdout("submodule",
                              ["update", "--init", "--recursive"], ) != 0):
      raise Exception("checking out submodules for %s failed" %(self.root,))

  def Fetch(self, module = None, cwd=None, update_submodules=True):
    if self.GitCommandStdout("fetch", ["origin"], module=module, cwd= cwd) != 0:
      raise Exception("Fetching for %s/%s failed" %
                              (self.root, module if module else ""))

  def Branch(self, revision=None,  module= None, cwd=None, branchname = None,
             update_submodules=True):
    assert(branchname or  self.target_branch)
    assert(revision or self.base_branch)
    if self.GitCommandStdout("checkout", ["--no-track", "-B",
                                          branchname or self.target_branch,
                                          revision or self.base_branch],
                              module=module, cwd= cwd) != 0:
      raise Exception("checking out %s for %s/%s failed" %
                            (branchname or self.target_branch, cwd or self.root,
                             module if module else ""))
    if (not module and update_submodules and
        self.GitCommandStdout("submodule",
                              ["update", "--init", "--recursive"], ) != 0):
      raise Exception("checking out submodules for %s failed" %
                            (cwd or self.root,))

  def BranchExists(self,branch, module= None, cwd=None):
    if "/" in branch:
      if self.GitCommandStdout("show-ref",
            ["--quiet", "--verify", 'refs/remotes/'+branch],
            module=module, cwd= cwd) == 0:
        return True
    return (self.GitCommandStdout("show-ref",
                    ["--quiet", "--verify", 'refs/heads/'+branch],
                    module=module, cwd= cwd) == 0 or
            self.GitCommandStdout("show-ref",
                    ["--quiet", "--verify", 'refs/remotes/origin/'+branch],
                    module=module, cwd= cwd) == 0)

  def GetConflicts(self,module= None, cwd=None):
    return self.GitCommand("diff", ["--name-only", "--diff-filter=U"],
                           module=module, cwd=cwd) # return conflicts

  def GetModified(self,module= None, cwd=None):
    return filter(None, self.GitCommand("diff",
                  ["--name-only", "--diff-filter=ACDMRTU"],
                  module=module, cwd=cwd)) # return modified files

  def GetUnCommitedItems(self,module= None, cwd=None):
    return self.GitCommand("diff", ["--name-only", "--cached"], module=module,
                  cwd=cwd) # return staged, but uncommitted files

  def Rebase(self, branch=None, source_commit=None, target_commit=None,
             module= None, cwd=None, continue_rebase=False, skip=False,
             interactive=False, no_autosquash=False):
    if continue_rebase:
      return self.GitCommandStdout("rebase", ["--continue"], module=module,
                                   cwd=cwd)
    elif skip:
      return self.GitCommandStdout("rebase", ["--skip"], module=module, cwd=cwd)

    assert target_commit or source_commit or branch
    assert not target_commit or (source_commit and branch)

    start_rebase_command = []
    if interactive:
      start_rebase_command.append("-i")
      if no_autosquash:
        start_rebase_command.append("--no-autosquash")
    if target_commit:
      start_rebase_command.extend(["--onto", target_commit])
    if source_commit:
      start_rebase_command.append(source_commit)
    if branch:
      start_rebase_command.append(branch)

    return self.GitCommandStdout("rebase", start_rebase_command, module=module,
                                 cwd=cwd)

  def Merge(self, module= None, cwd=None, revision=None, commit = False,
            ff_only = False, update_submodules=True):
    assert(revision or self.source_branch)
    module_file = self.GetFile(".gitmodules", module=module, cwd=cwd,
                               notexist_dont_fail=True)
    self.GitCommandStdout("merge", [(revision or self.source_branch),
                  ("--ff-only" if ff_only else "--ff"),
                  ("--commit" if commit else "--no-commit"),
                  "-X", "patience",
                  "--no-edit"] , module=module, cwd= cwd)
    if module_file != None:
      self.WriteFile(".gitmodules", module_file, module=module, cwd=cwd)
    if (not module and update_submodules and
        self.GitCommandStdout("submodule",
                              ["update", "--init", "--recursive"], ) != 0):
      raise Exception("checking out submodules for %s failed" %
                            (cwd or self.root,))
    return self.GetConflicts(module=module, cwd=cwd) # return conflicts

  def Commit(self, message, module= None, cwd=None):
    if not self.GetUnCommitedItems(module,cwd):
      return # Ignore empty commit
    if self.GitCommandStdout("commit", ["--no-edit", "-m", message],
                             module=module, cwd=cwd) != 0:
      raise BaseException("Committing %s/%s failed"%
                                  (cwd or self.root, (module or "")))

  def Push(self, branch, module= None, cwd=None):
    if self.GitCommandStdout("push", ["-u", "origin", branch],
                             module=module, cwd=cwd) != 0:
      raise BaseException("Pushing %s to origin from %s/%s failed" %
                              (branch, cwd or self.root, (module or "")))

  def Log(self, frm = None, to=None, reverse=False, full_sha=False,
          module= None, cwd=None):
    params = ["--pretty=oneline" if full_sha else "--oneline"]
    if reverse:
      params.append("--reverse")

    assert frm or to
    params.append((frm or "")+".."+(to or ""))

    log = self.GitCommand("log", params,module = module, cwd=cwd,
                          dryrun_response = "00000000000 Foo bar")
    return log

  def GetRevision(self, branch, module= None, cwd=None):
    revision = self.GitCommand("show-ref", [branch],module = module, cwd=cwd,
                               dryrun_response = "00000000000 HEAD"
                               )[0].split()[0]
    return revision

  def GetBranches(self, commit=None, module= None, cwd=None, remote_only=False):
    branches = self.GitCommand("branch",
                               ["--remote" if remote_only else "--all",
                                  "--contains", commit or "HEAD",],
                               module = module, cwd=cwd,
                               dryrun_response = "foo/bar")
    branches = [x.split()[0] for x in branches]
    return branches

  def GetCommonAncestor(self,revision1, revision2, module = None, cwd=None):
    if "/" in revision1:
      revision1 = self.GetRevision(revision1, module=module, cwd= cwd)
    if "/" in revision2:
      revision2 = self.GetRevision(revision2, module=module, cwd= cwd)
    return self.GitCommand("merge-base", [revision1, revision2], module=module,
                           cwd= cwd, dryrun_response="00000000")[0]

  def GetIsAncestor(self,revision_base, revision_tip, module = None, cwd=None):
    if "/" in revision_base:
      revision_base = self.GetRevision(revision_base, module=module, cwd= cwd)
    if "/" in revision_tip:
      revision_tip = self.GetRevision(revision_tip, module=module, cwd= cwd)

    ret = self.GitCommandStdout("merge-base",
                                ["--is-ancestor", revision_base, revision_tip],
                                module=module, cwd= cwd)

    if ret == 0:
      return True
    elif ret == 1:
      return False
    else:
      raise BaseException("Mergebase IsAncestor failed")

  def RemoveSubmodule(self, module, cwd=None):
    do_rm = True
    gitdir_mod = self.GitCommand("rev-parse", ["--git-dir"], module=module,
                                 cwd=cwd)[0]
    if self.GitCommandStdout("submodule", ["deinit", module], cwd=cwd) != 0:
      do_rm = True
    if self.GitCommandStdout("rm", [module], cwd=cwd) != 0:
      raise BaseException("Removing submodule %s failed"%(module,))
    if do_rm:
      subprocess.call(["rm", "-rf", module], cwd=self.__get_wd(None, cwd))
      subprocess.call(["rm", "-rf", gitdir_mod], cwd=self.__get_wd(None, cwd))
    self.GitCommandStdout("config", ["--file", '.gitmodules',
                  "--remove-section", 'submodule.%s'%( module,)], cwd=cwd)
    gitdir = self.GitCommand("rev-parse", ["--git-dir"], cwd=cwd)[0]
    self.GitCommandStdout("config", ["--file", os.path.join(gitdir, "config"),
                  "--remove-section", 'submodule.%s'%( module,)], cwd=cwd)
    self.AddFile(".gitmodules", cwd=cwd)

  def RemoveFile(self, filename, cwd=None):
    if self.GitCommandStdout("rm", ["-r", "-f", filename], cwd=cwd) != 0:
      raise BaseException("Removing file %s failed"%(filename,))

  def ResolveURL(self, url, absolute=False):
    assert(self.base_url)
    if url.startswith('https://chromium.googlesource.com/'):
      url = url.replace('https://chromium.googlesource.com', self.base_url,1)
    elif url.startswith('https://llvm.googlesource.com/'):
      url = url.replace('https://llvm.googlesource.com',
                        self.base_url+"/llvm",1)
    elif url.startswith('https://pdfium.googlesource.com/'):
      url = url.replace('https://pdfium.googlesource.com',
                         self.base_url+"/pdfium",1)
    elif url.startswith('https://android.googlesource.com/'):
      url = url.replace('https://android.googlesource.com',
                         self.base_url+"/android",1)
    elif url.startswith('https://boringssl.googlesource.com/'):
      url = url.replace('https://boringssl.googlesource.com',
                         self.base_url+"/boringssl",1)
    elif url.startswith('https://swiftshader.googlesource.com/'):
      url = url.replace('https://swiftshader.googlesource.com',
                         self.base_url+"/swiftshader",1)
    elif url.startswith('https://skia.googlesource.com/'):
      url = url.replace('https://skia.googlesource.com',
                         self.base_url+"/skia",1)
    elif re.search(r'^https://\w+.googlesource.com/', url):
      url = re.sub('^https://\w+.googlesource.com',
                         self.base_url+"/googlesource",1)

    if not url.startswith(self.base_url):
      raise BaseException(
          "Resolving URL failed because url %s does not match base url %s" %
              (url, self.base_url))
    if absolute:
      return url

    print "Resolve", url
    print "Current url", self.url
    url = os.path.relpath(url, self.url).replace("\\", "/")
    print "Relative URL", url
    return url

  def AddSubmodule(self, module, url, revision, cwd=None):
    resolver = self
    if cwd:
      gitdir = self.GitCommand("rev-parse", ["--git-dir"])[0]
      url_base = resolver.GitCommand("config",
                         ["-f", os.path.join(gitdir, "config"), "--get",
                          "submodule."+cwd.replace(self.root+"/", "")+".url"]
                       )[0]
      resolver = Git(inherit=resolver, source_dir=cwd, url= url_base)

    try:
      module_config = resolver.GetSubmoduleInfo(module)
    except:
      module_config = None

    if not module_config:
      url = resolver.ResolveURL(url)

      if self.GitCommandStdout("submodule", ["add", "-f", url, module],
                         cwd=cwd) != 0:
        raise BaseException("Adding submodule %s failed"%(module,))

    self.GitCommandStdout("submodule", ["sync", module])
    self.Fetch(module, cwd)
    self.Checkout(revision, module, cwd)
    self.AddFile(name=module, cwd=cwd)
    self.AddFile(".gitmodules", cwd=cwd)

  def ChangeSubmoduleURL(self, module, url, cwd=None):
    resolver = self
    gitdir = self.GitCommand("rev-parse", ["--git-dir"])[0]
    if cwd:
      url_base = resolver.GitCommand("config",
                         ["-f", os.path.join(gitdir, "config"), "--get",
                            "submodule."+cwd.replace(self.root+"/", "")+".url"]
                       )[0]
      resolver = Git(inherit=resolver, source_dir=cwd, url= url_base)

    url = resolver.ResolveURL(url)
    abs_url = resolver.url+"/"+url

    if self.GitCommandStdout("remote", ["set-url", "origin", abs_url],
                             module=module, cwd=cwd) != 0:
      raise BaseException("Updating submodule URL for %s failed"%(module,))

    self.GitCommand("config",
                    ["-f", os.path.join(gitdir, "config"),
                     "submodule."+(
                          cwd.replace(self.root+"/", "")+"." if cwd else "")+
                          "url", abs_url])
    self.GitCommand("config",
                    ["-f", ".gitmodules", "submodule."+(
                          cwd.replace(self.root+"/", "")+"." if cwd else "")+
                          "url", url])
    self.Fetch(module, cwd)

    self.AddFile(".gitmodules", cwd=cwd)

  def RepoExists(self, url):
    url = self.ResolveURL(url, absolute=True)

    return self.GitCommandStdout("ls-remote", [url, "HEAD"]) == 0

  def GetFile(self, name, revision=None,module=None, cwd=None,
              notexist_dont_fail=False):
    if revision:
      content = self.GitCommand("show", ["%s:%s" %(revision, name)],
                                module=module, cwd=cwd, raw=True)
    elif self.dryrun:
      content = ""
    else:
      cwd = self.__get_wd(module=module, cwd=cwd)

      try:
        reader = open(os.path.join(cwd, name))
      except:
        if notexist_dont_fail:
          return None
        raise
      content = reader.read()
      reader.close()

    return content

  def AddFile(self,name, module = None, cwd = None ):
    if self.GitCommandStdout("add", ["-f", name], module=module, cwd= cwd) != 0:
      raise Exception("Adding %s for %s/%s failed" %
                          (name, self.root if not cwd else cwd,
                               module if module and not cwd else ""))

  def WriteFile(self, name, content, module = None, cwd=None, binary=False,
                add=True):

    local_cwd = self.__get_wd(module, cwd)
    filename = os.path.join(local_cwd, name)

    if self.dryrun:
      print "Dryrun: Writing file", filename
      sys.stdout.flush()
    else:
      outfile = open(filename, "wb")

      outfile.write(content);
      outfile.close();

    if add:
      self.AddFile(name, module, cwd)

  def GetSubmoduleFileList(self):
    content = self.GitCommand("ls-tree", ["-r", "HEAD"], verbose=False)

    modules = [
        x[-1] for x in [y.strip().split() for y in content] if x[0] == "160000"
      ]

    return modules

  def GetSubmoduleInfo(self, mod=None, recursive=False):

    if not os.access(os.path.join(self.root, ".gitmodules"), os.F_OK):
      return {}

    content = self.GitCommand("submodule", ["status", "--"] + (mod or []))
    def gitid(x):
      if x and x[0] in "+-":
        x = x[1:]
      return x

    modules = dict([(x[1], {"id":gitid(x[0]),
              "module_name":x[1],
              "dir":self.root}) for x in [y.strip().split() for y in content
                                             if not y.startswith("..") ]])

    if recursive:
      for mod in list(modules.iterkeys()):
        subgit = Git(inherit=self, submodule=mod)
        submod = subgit.GetSubmoduleInfo(True)
        for k, d in submod.iteritems():
          parent = d.get("submodule_of", None)
          d["submodule_of"]= (mod + "/" + parent) if parent else mod
          modules[mod+"/"+k]=d

    return modules

  def GetGitDepInfo(self,revision=None, filename=None):

    content = self.GetFile(filename or "DEPS", revision = revision)

    return deps_utils.GetDepsContent(None, text=content, return_dict = True,
                                     git_url = self.base_url )

  def WriteGitDepInfo(self, deps, add=False, name=None):

    if not name:
      self.WriteGitDepInfo(deps, add=add, name="DEPS")
      return

    cwd = self.__get_wd()
    filename = os.path.join(cwd, name)
    if self.dryrun:
      print "Dryrun: Writing file", filename
      sys.stdout.flush()
    else:
      deps2 = copy.deepcopy(deps)
      for dep_cat in [deps2["deps"]]+list(deps2["deps_os"].itervalues()):
        for mod, token, var in [
            ('vivaldi/third_party/WebKit','VAR_WEBKIT_REV', 'webkit_rev'),
            ('vivaldi/third_party/ffmpeg', 'VAR_FFMPEG_HASH', 'ffmpeg_hash'),
                ]:
          if mod in dep_cat and dep_cat[mod]:
            url, _sep, rev = dep_cat[mod].partition("@")
            if rev:
              deps2["vars"][var] = "@"+rev
              dep_cat[mod] = url+token

      deps_utils.WriteDepsDict(filename, deps2)

    if add:
      self.AddFile(name)

  def GetDryRun(self):
    return self.dryrun

  def StartAutoBuild(self, branch=None, revision=None, user="automerger",
                     reason="Test of merged branch", cwd=None):
    autobuild_url = "http://byggmakker.viv.int:8010/builders/_selected/forceselected"

    if not branch:
      branch = self.target_branch

    if not revision:
      revision = self.GitCommand("show-ref", [branch],
                         dryrun_response = "00000000000 HEAD")[0].split()[0]

    query= {
      "selected":["Win Builder", "Linux x64", "Android Builder", "Mac Builder"],
      "username":user,
      "comments":reason,
      "branch":branch,
      "revision":revision,
      "submit":"Force Build"
    }

    import urllib
    querystring = urllib.urlencode(query, True)

    print "Sending query to %s : %s" % (autobuild_url, querystring)
    sys.stdout.flush()
    if not self.dryrun:
      urllib.urlopen(autobuild_url, querystring)

  def EndUseOfMirror(self):
    if self.url.startswith("ssh://mirror.viv.osl/"):
      self.url = self.url.replace("ssh://mirror.viv.osl/", "ssh://git.viv.int/")
      self.base_url = self.base_url.replace("ssh://mirror.viv.osl/",
                                            "ssh://git.viv.int/")
      self.GitCommandStdout("remote", ["set-url", "origin", self.url])
      self.GitCommandStdout("submodule", ["sync"])
