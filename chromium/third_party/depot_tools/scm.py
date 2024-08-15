# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""SCM-specific utility classes."""

from collections import defaultdict
import os
import platform
import re
from typing import Mapping, List

import gclient_utils
import subprocess2

# TODO: Should fix these warnings.
# pylint: disable=line-too-long

# constants used to identify the tree state of a directory.
VERSIONED_NO = 0
VERSIONED_DIR = 1
VERSIONED_SUBMODULE = 2


def determine_scm(root):
    """Similar to upload.py's version but much simpler.

    Returns 'git' or None.
    """
    if os.path.isdir(os.path.join(root, '.git')):
        return 'git'

    try:
        subprocess2.check_call(['git', 'rev-parse', '--show-cdup'],
                               stdout=subprocess2.DEVNULL,
                               stderr=subprocess2.DEVNULL,
                               cwd=root)
        return 'git'
    except (OSError, subprocess2.CalledProcessError):
        return None


class GIT(object):
    current_version = None
    rev_parse_cache = {}

    # Maps cwd -> {config key, [config values]}
    # This cache speeds up all `git config ...` operations by only running a
    # single subcommand, which can greatly accelerate things like
    # git-map-branches.
    _CONFIG_CACHE: Mapping[str, Mapping[str, List[str]]] = {}

    @staticmethod
    def _load_config(cwd: str) -> Mapping[str, List[str]]:
        """Loads git config for the given cwd.

        The calls to this method are cached in-memory for performance. The
        config is only reloaded on cache misses.

        Args:
            cwd: path to fetch `git config` for.

        Returns:
            A dict mapping git config keys to a list of its values.
        """
        if cwd not in GIT._CONFIG_CACHE:
            try:
                rawConfig = GIT.Capture(['config', '--list', '-z'],
                                        cwd=cwd,
                                        strip_out=False)
            except subprocess2.CalledProcessError:
                return {}

            cfg = defaultdict(list)

            # Splitting by '\x00' gets an additional empty string at the end.
            for line in rawConfig.split('\x00')[:-1]:
                key, value = map(str.strip, line.split('\n', 1))
                cfg[key].append(value)

            GIT._CONFIG_CACHE[cwd] = cfg

        return GIT._CONFIG_CACHE[cwd]

    @staticmethod
    def _clear_config(cwd: str) -> None:
        GIT._CONFIG_CACHE.pop(cwd, None)


    @staticmethod
    def ApplyEnvVars(kwargs):
        env = kwargs.pop('env', None) or os.environ.copy()
        # Don't prompt for passwords; just fail quickly and noisily.
        # By default, git will use an interactive terminal prompt when a
        # username/ password is needed.  That shouldn't happen in the chromium
        # workflow, and if it does, then gclient may hide the prompt in the
        # midst of a flood of terminal spew.  The only indication that something
        # has gone wrong will be when gclient hangs unresponsively.  Instead, we
        # disable the password prompt and simply allow git to fail noisily.  The
        # error message produced by git will be copied to gclient's output.
        env.setdefault('GIT_ASKPASS', 'true')
        env.setdefault('SSH_ASKPASS', 'true')
        # 'cat' is a magical git string that disables pagers on all platforms.
        env.setdefault('GIT_PAGER', 'cat')
        return env

    @staticmethod
    def Capture(args, cwd=None, strip_out=True, **kwargs):
        env = GIT.ApplyEnvVars(kwargs)
        output = subprocess2.check_output(['git'] + args,
                                          cwd=cwd,
                                          stderr=subprocess2.PIPE,
                                          env=env,
                                          **kwargs)
        output = output.decode('utf-8', 'replace')
        return output.strip() if strip_out else output

    @staticmethod
    def CaptureStatus(cwd,
                      upstream_branch,
                      end_commit=None,
                      ignore_submodules=True):
        # type: (str, str, Optional[str]) -> Sequence[Tuple[str, str]]
        """Returns git status.

        Returns an array of (status, file) tuples."""
        if end_commit is None:
            end_commit = ''
        if upstream_branch is None:
            upstream_branch = GIT.GetUpstreamBranch(cwd)
            if upstream_branch is None:
                raise gclient_utils.Error('Cannot determine upstream branch')

        command = [
            '-c', 'core.quotePath=false', 'diff', '--name-status',
            '--no-renames'
        ]
        if ignore_submodules:
            command.append('--ignore-submodules=all')
        command.extend(['-r', '%s...%s' % (upstream_branch, end_commit)])

        status = GIT.Capture(command, cwd)
        results = []
        if status:
            for statusline in status.splitlines():
                # 3-way merges can cause the status can be 'MMM' instead of 'M'.
                # This can happen when the user has 2 local branches and he
                # diffs between these 2 branches instead diffing to upstream.
                m = re.match(r'^(\w)+\t(.+)$', statusline)
                if not m:
                    raise gclient_utils.Error(
                        'status currently unsupported: %s' % statusline)
                # Only grab the first letter.
                results.append(('%s      ' % m.group(1)[0], m.group(2)))
        return results

    @staticmethod
    def GetConfig(cwd, key, default=None):
        values = GIT._load_config(cwd).get(key, None)
        if not values:
            return default

        return values[-1]

    @staticmethod
    def GetConfigBool(cwd, key):
        return GIT.GetConfig(cwd, key) == 'true'

    @staticmethod
    def GetConfigList(cwd, key):
        return GIT._load_config(cwd).get(key, [])

    @staticmethod
    def YieldConfigRegexp(cwd, pattern):
        """Yields (key, value) pairs for any config keys matching `pattern`."""
        p = re.compile(pattern)
        for name, values in GIT._load_config(cwd).items():
            if p.match(name):
                for value in values:
                    yield name, value

    @staticmethod
    def GetBranchConfig(cwd, branch, key, default=None):
        assert branch, 'A branch must be given'
        key = 'branch.%s.%s' % (branch, key)
        return GIT.GetConfig(cwd, key, default)

    @staticmethod
    def SetConfig(cwd,
                  key,
                  value=None,
                  *,
                  value_pattern=None,
                  modify_all=False,
                  scope='local'):
        """Sets or unsets one or more config values.

        Args:
            cwd: path to fetch `git config` for.
            key: The specific config key to affect.
            value: The value to set. If this is None, `key` will be unset.
            value_pattern: For use with `all=True`, allows further filtering of
                the set or unset operation based on the currently configured
                value. Ignored for `all=False`.
            modify_all: If True, this will change a set operation to
                `--replace-all`, and will change an unset operation to
                `--unset-all`.
            scope: By default this is the local scope, but could be `system`,
                `global`, or `worktree`, depending on which config scope you
                want to affect.
        """
        GIT._clear_config(cwd)

        args = ['config', f'--{scope}']
        if value == None:
            args.extend(['--unset' + ('-all' if modify_all else ''), key])
        else:
            if modify_all:
                args.append('--replace-all')

            args.extend([key, value])

        if modify_all and value_pattern:
            args.append(value_pattern)
        GIT.Capture(args, cwd=cwd)

    @staticmethod
    def SetBranchConfig(cwd, branch, key, value=None):
        assert branch, 'A branch must be given'
        key = 'branch.%s.%s' % (branch, key)
        GIT.SetConfig(cwd, key, value)

    @staticmethod
    def IsWorkTreeDirty(cwd):
        return GIT.Capture(['status', '-s'], cwd=cwd) != ''

    @staticmethod
    def GetEmail(cwd):
        """Retrieves the user email address if known."""
        return GIT.GetConfig(cwd, 'user.email', '')

    @staticmethod
    def ShortBranchName(branch):
        """Converts a name like 'refs/heads/foo' to just 'foo'."""
        return branch.replace('refs/heads/', '')

    @staticmethod
    def GetBranchRef(cwd):
        """Returns the full branch reference, e.g. 'refs/heads/main'."""
        try:
            return GIT.Capture(['symbolic-ref', 'HEAD'], cwd=cwd)
        except subprocess2.CalledProcessError:
            return None

    @staticmethod
    def GetRemoteHeadRef(cwd, url, remote):
        """Returns the full default remote branch reference, e.g.
        'refs/remotes/origin/main'."""
        if os.path.exists(cwd):
            try:
                # Try using local git copy first
                ref = 'refs/remotes/%s/HEAD' % remote
                ref = GIT.Capture(['symbolic-ref', ref], cwd=cwd)
                if not ref.endswith('master'):
                    return ref
                # Check if there are changes in the default branch for this
                # particular repository.
                GIT.Capture(['remote', 'set-head', '-a', remote], cwd=cwd)
                return GIT.Capture(['symbolic-ref', ref], cwd=cwd)
            except subprocess2.CalledProcessError:
                pass

        try:
            # Fetch information from git server
            resp = GIT.Capture(['ls-remote', '--symref', url, 'HEAD'])
            regex = r'^ref: (.*)\tHEAD$'
            for line in resp.split('\n'):
                m = re.match(regex, line)
                if m:
                    return ''.join(GIT.RefToRemoteRef(m.group(1), remote))
        except subprocess2.CalledProcessError:
            pass
        # Return default branch
        return 'refs/remotes/%s/main' % remote

    @staticmethod
    def GetBranch(cwd):
        """Returns the short branch name, e.g. 'main'."""
        branchref = GIT.GetBranchRef(cwd)
        if branchref:
            return GIT.ShortBranchName(branchref)
        return None

    @staticmethod
    def GetRemoteBranches(cwd):
        return GIT.Capture(['branch', '-r'], cwd=cwd).split()

    @staticmethod
    def FetchUpstreamTuple(cwd, branch=None):
        """Returns a tuple containing remote and remote ref,
        e.g. 'origin', 'refs/heads/main'
        """
        try:
            branch = branch or GIT.GetBranch(cwd)
        except subprocess2.CalledProcessError:
            pass
        if branch:
            upstream_branch = GIT.GetBranchConfig(cwd, branch, 'merge')
            if upstream_branch:
                remote = GIT.GetBranchConfig(cwd, branch, 'remote', '.')
                return remote, upstream_branch

        upstream_branch = GIT.GetConfig(cwd, 'rietveld.upstream-branch')
        if upstream_branch:
            remote = GIT.GetConfig(cwd, 'rietveld.upstream-remote', '.')
            return remote, upstream_branch

        # Else, try to guess the origin remote.
        remote_branches = GIT.GetRemoteBranches(cwd)
        if 'origin/main' in remote_branches:
            # Fall back on origin/main if it exits.
            return 'origin', 'refs/heads/main'

        if 'origin/master' in remote_branches:
            # Fall back on origin/master if it exits.
            return 'origin', 'refs/heads/master'

        return None, None

    @staticmethod
    def RefToRemoteRef(ref, remote):
        """Convert a checkout ref to the equivalent remote ref.

        Returns:
            A tuple of the remote ref's (common prefix, unique suffix), or None if it
            doesn't appear to refer to a remote ref (e.g. it's a commit hash).
        """
        # TODO(mmoss): This is just a brute-force mapping based of the expected
        # git config. It's a bit better than the even more brute-force
        # replace('heads', ...), but could still be smarter (like maybe actually
        # using values gleaned from the git config).
        m = re.match('^(refs/(remotes/)?)?branch-heads/', ref or '')
        if m:
            return ('refs/remotes/branch-heads/', ref.replace(m.group(0), ''))

        m = re.match('^((refs/)?remotes/)?%s/|(refs/)?heads/' % remote, ref
                     or '')
        if m:
            return ('refs/remotes/%s/' % remote, ref.replace(m.group(0), ''))

        return None

    @staticmethod
    def RemoteRefToRef(ref, remote):
        assert remote, 'A remote must be given'
        if not ref or not ref.startswith('refs/'):
            return None
        if not ref.startswith('refs/remotes/'):
            return ref
        if ref.startswith('refs/remotes/branch-heads/'):
            return 'refs' + ref[len('refs/remotes'):]
        if ref.startswith('refs/remotes/%s/' % remote):
            return 'refs/heads' + ref[len('refs/remotes/%s' % remote):]
        return None

    @staticmethod
    def GetUpstreamBranch(cwd):
        """Gets the current branch's upstream branch."""
        remote, upstream_branch = GIT.FetchUpstreamTuple(cwd)
        if remote != '.' and upstream_branch:
            remote_ref = GIT.RefToRemoteRef(upstream_branch, remote)
            if remote_ref:
                upstream_branch = ''.join(remote_ref)
        return upstream_branch

    @staticmethod
    def IsAncestor(maybe_ancestor, ref, cwd=None):
        # type: (string, string, Optional[string]) -> bool
        """Verifies if |maybe_ancestor| is an ancestor of |ref|."""
        try:
            GIT.Capture(['merge-base', '--is-ancestor', maybe_ancestor, ref],
                        cwd=cwd)
            return True
        except subprocess2.CalledProcessError:
            return False

    @staticmethod
    def GetOldContents(cwd, filename, branch=None):
        if not branch:
            branch = GIT.GetUpstreamBranch(cwd)
        if platform.system() == 'Windows':
            # git show <sha>:<path> wants a posix path.
            filename = filename.replace('\\', '/')
        command = ['show', '%s:%s' % (branch, filename)]
        try:
            return GIT.Capture(command, cwd=cwd, strip_out=False)
        except subprocess2.CalledProcessError:
            return ''

    @staticmethod
    def GenerateDiff(cwd,
                     branch=None,
                     branch_head='HEAD',
                     full_move=False,
                     files=None):
        """Diffs against the upstream branch or optionally another branch.

        full_move means that move or copy operations should completely recreate the
        files, usually in the prospect to apply the patch for a try job."""
        if not branch:
            branch = GIT.GetUpstreamBranch(cwd)
        command = [
            '-c', 'core.quotePath=false', 'diff', '-p', '--no-color',
            '--no-prefix', '--no-ext-diff', branch + "..." + branch_head
        ]
        if full_move:
            command.append('--no-renames')
        else:
            command.append('-C')
        # TODO(maruel): --binary support.
        if files:
            command.append('--')
            command.extend(files)
        diff = GIT.Capture(command, cwd=cwd, strip_out=False).splitlines(True)
        for i in range(len(diff)):
            # In the case of added files, replace /dev/null with the path to the
            # file being added.
            if diff[i].startswith('--- /dev/null'):
                diff[i] = '--- %s' % diff[i + 1][4:]
        return ''.join(diff)

    @staticmethod
    def GetDifferentFiles(cwd, branch=None, branch_head='HEAD'):
        """Returns the list of modified files between two branches."""
        if not branch:
            branch = GIT.GetUpstreamBranch(cwd)
        command = [
            '-c', 'core.quotePath=false', 'diff', '--name-only',
            branch + "..." + branch_head
        ]
        return GIT.Capture(command, cwd=cwd).splitlines(False)

    @staticmethod
    def GetAllFiles(cwd):
        """Returns the list of all files under revision control."""
        command = ['-c', 'core.quotePath=false', 'ls-files', '--', '.']
        return GIT.Capture(command, cwd=cwd).splitlines(False)

    @staticmethod
    def GetSubmoduleCommits(cwd, submodules):
        # type: (string, List[string]) => Mapping[string][string]
        """Returns a mapping of staged or committed new commits for submodules."""
        if not submodules:
            return {}
        result = subprocess2.check_output(['git', 'ls-files', '-s', '--'] +
                                          submodules,
                                          cwd=cwd).decode('utf-8')
        commit_hashes = {}
        for r in result.splitlines():
            # ['<mode>', '<commit_hash>', '<stage_number>', '<path>'].
            record = r.strip().split(maxsplit=3)  # path can contain spaces.
            assert record[0] == '160000', 'file is not a gitlink: %s' % record
            commit_hashes[record[3]] = record[1]
        return commit_hashes

    @staticmethod
    def GetPatchName(cwd):
        """Constructs a name for this patch."""
        short_sha = GIT.Capture(['rev-parse', '--short=4', 'HEAD'], cwd=cwd)
        return "%s#%s" % (GIT.GetBranch(cwd), short_sha)

    @staticmethod
    def GetCheckoutRoot(cwd):
        """Returns the top level directory of a git checkout as an absolute path.
        """
        root = GIT.Capture(['rev-parse', '--show-cdup'], cwd=cwd)
        return os.path.abspath(os.path.join(cwd, root))

    @staticmethod
    def GetGitDir(cwd):
        return os.path.abspath(GIT.Capture(['rev-parse', '--git-dir'], cwd=cwd))

    @staticmethod
    def IsInsideWorkTree(cwd):
        try:
            return GIT.Capture(['rev-parse', '--is-inside-work-tree'], cwd=cwd)
        except (OSError, subprocess2.CalledProcessError):
            return False

    @staticmethod
    def IsVersioned(cwd, relative_dir):
        # type: (str, str) -> int
        """Checks whether the given |relative_dir| is part of cwd's repo."""
        output = GIT.Capture(['ls-tree', 'HEAD', '--', relative_dir], cwd=cwd)
        if not output:
            return VERSIONED_NO
        if output.startswith('160000'):
            return VERSIONED_SUBMODULE
        return VERSIONED_DIR

    @staticmethod
    def ListSubmodules(repo_root):
        # type: (str) -> Collection[str]
        """Returns the list of submodule paths for the given repo.

        Path separators will be adjusted for the current OS.
        """
        if not os.path.exists(os.path.join(repo_root, '.gitmodules')):
            return []
        config_output = GIT.Capture(
            ['config', '--file', '.gitmodules', '--get-regexp', 'path'],
            cwd=repo_root)
        return [
            line.split()[-1].replace('/', os.path.sep)
            for line in config_output.splitlines()
        ]

    @staticmethod
    def CleanupDir(cwd, relative_dir):
        """Cleans up untracked file inside |relative_dir|."""
        return bool(GIT.Capture(['clean', '-df', relative_dir], cwd=cwd))

    @staticmethod
    def ResolveCommit(cwd, rev):
        cache_key = None
        # We do this instead of rev-parse --verify rev^{commit}, since on
        # Windows git can be either an executable or batch script, each of which
        # requires escaping the caret (^) a different way.
        if gclient_utils.IsFullGitSha(rev):
            # Only cache full SHAs
            cache_key = hash(cwd + rev)
            if val := GIT.rev_parse_cache.get(cache_key):
                return val

            # git-rev parse --verify FULL_GIT_SHA always succeeds, even if we
            # don't have FULL_GIT_SHA locally. Removing the last character
            # forces git to check if FULL_GIT_SHA refers to an object in the
            # local database.
            rev = rev[:-1]
        res = GIT.Capture(['rev-parse', '--quiet', '--verify', rev], cwd=cwd)
        if cache_key:
            # We don't expect concurrent execution, so we don't lock anything.
            GIT.rev_parse_cache[cache_key] = res

        return res

    @staticmethod
    def IsValidRevision(cwd, rev, sha_only=False):
        """Verifies the revision is a proper git revision.

        sha_only: Fail unless rev is a sha hash.
        """
        try:
            sha = GIT.ResolveCommit(cwd, rev)
        except subprocess2.CalledProcessError:
            return None

        if sha_only:
            return sha == rev.lower()
        return True
