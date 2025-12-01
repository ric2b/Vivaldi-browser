#!/usr/bin/env python3
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import sys

import gclient_utils
import git_common


# Squash a branch, taking care to rebase the branch on top of the new commit
# position of its upstream branch.
def rebase_branch(branch, initial_hashes):
    print('Re-parenting branch %s.' % branch)
    assert initial_hashes[branch] == git_common.hash_one(branch)

    upstream_branch = git_common.upstream(branch)
    old_upstream_branch = initial_hashes[upstream_branch]

    # Because the branch's upstream has potentially changed from squashing it,
    # the current branch is rebased on top of the new upstream.
    git_common.run('rebase', '--onto', upstream_branch, old_upstream_branch,
                   branch, '--update-refs')


# Squashes all branches that are part of the subtree starting at `branch`.
def rebase_subtree(branch, initial_hashes, downstream_branches):
    # Rebase us onto our parent
    rebase_branch(branch, initial_hashes)

    # Recurse on downstream branches, if any.
    for downstream_branch in downstream_branches[branch]:
        rebase_subtree(downstream_branch, initial_hashes, downstream_branches)


def children_have_diverged(branch, downstream_branches, diverged_branches):
    # If we have no diverged branches, then no children have diverged.
    if not diverged_branches:
        return False

    # If we have diverged, then our children have diverged.
    if branch in diverged_branches:
        return True

    # If any of our children have diverged, then we need to return true.
    for downstream_branch in downstream_branches[branch]:
        if children_have_diverged(downstream_branch, downstream_branches,
                                  diverged_branches):
            return True

    return False

def main(args):
    if gclient_utils.IsEnvCog():
        print('squash-branch command is not supported in non-git environment.',
              file=sys.stderr)
        return 1
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-m',
        '--message',
        metavar='<msg>',
        default=None,
        help='Use the given <msg> as the first line of the commit message.')
    opts = parser.parse_args(args)
    if git_common.is_dirty_git_tree('squash-branch'):
        return 1

    # Save off the current branch so we can return to it at the end.
    return_branch = git_common.current_branch()

    # Save the hashes before we mutate the tree so that we have all of the
    # necessary rebasing information.
    _, tree = git_common.get_branch_tree()
    initial_hashes = git_common.get_hashes(tree)
    downstream_branches = git_common.get_downstream_branches(tree)
    diverged_branches = git_common.get_diverged_branches(tree)

    # We won't be rebasing our squashed branch, so only check any potential
    # children
    for branch in downstream_branches[return_branch]:
        if children_have_diverged(branch, downstream_branches,
                                  diverged_branches):
            print('Cannot use `git squash-branch` since some children have '
                  'diverged from their upstream and could cause conflicts.')
            return 1

    git_common.squash_current_branch(opts.message)

    # Fixup our children with our new state.
    for branch in downstream_branches[return_branch]:
        rebase_subtree(branch, initial_hashes, downstream_branches)

    git_common.run('checkout', return_branch)

    return 0


if __name__ == '__main__':
    try:
        sys.exit(main(sys.argv[1:]))
    except KeyboardInterrupt:
        sys.stderr.write('interrupted\n')
        sys.exit(1)
