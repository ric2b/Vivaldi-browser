---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: chroot-clangd-guide
title: Using Clangd LSP Server in the Chroot
---

[TOC]

## Motivation

IDE features like macro-expansion, auto-complete, find-references, and
go-to-declaration are indisuputably useful for programmers.

Language Servers provide these features without committing to configuring a full
IDE like CLion.
So if you work in Emacs, Vim, Sublime, VSCode or similiar but don't have all
these features, this guide is for you (assuming you like work being easier).

For Googlers that work in CLion, checkout [go/clion-for-chromeos][go-clion].

## Disclaimer

This guide has only been tested with a Zephyr RTOS Project development workflow.

## Background on Language Servers

Language Servers do the work of an IDE and give that information to clients.
The LSP (Language Server Protocol) attempts to abstract away much of the work
done by editors to provide standard IDE features from a server.
An editor only must have an LSP client.

See [the official page][lsp] for more information.

## Setup Steps

### Chroot Setup

If you want to use an editor outside the chroot then you must be able to invoke
`cros_sdk` and in following invocations not require a password due to a longer
sudo password timeout.
If you followed [CROS Developer Guide][cros-dev-guide] then you should have this
setup; specifically if you followed [how to make sudo a little more
permissive][permissive-sudo].

### Create a Compilation Database

So that ClangD can understand how your project is built, you need to generate a
special JSON called a compilation database, on a per-project-basis; typically
named `compile_commands.json`. See [Clangd JSON Compilation Database
Spec][clangd-c-db] for more information on compilation databases. There are
several methods for generating the database, and the best method for a
particular project may depend on its build system.

#### Bear

[Bear][bear] is a tool available in package managers inside and outside the
chroot (`dev-util/bear` in Portage, `bear` in Debian repositories). Note that
Bear runs the build command as a side effect of generating the database. Using
Bear is as simple as invoking `bear -- <YOUR BUILD COMMAND>`. For example: `bear
-- zmake testall`.

#### `compiledb`

[`compiledb`][compiledb] is a tool for generating databases from `make` builds.
It does this by running a dry-run build. Despite this, `compiledb` is somewhat
slower than Bear. It is available via `pip` inside and outside the chroot. Use
it like this: `compiledb make <target>`.

#### `gn`

Users of [gn][gn] can generate `compile_commands.json` with [ninja][ninja] by
invoking [`ninja -t compdb`][ninja-tools].

#### Other tools

See the [Sarcasm Notebook on Generating CompilationDatabases][sarcasm-c-db] for
other build systems and tools.

#### Updating the database

You should not have to regenerate this unless the structure of your build
changes. If Clangd was working great but then started to have issues,
regenerating the compilation database can be a quick fix. For minor build
changes, tools should be able to quickly, incrementally update the database.

In theory, the build could have changed any time you check out a new commit in
Git. To keep up with this, you can use Git hooks to update the database each
time HEAD changes. With Bear (for instance), that would look something like
this:

`$HOME/bin/compiledb_hook.sh`:
```sh
#!/bin/sh

bear make -j 72 <target> >/dev/null 2>&1 &
```

`$HOME/bin/post-rewrite_hook.sh`:
```sh
#!/bin/sh
# Only execute for rebase; the other case for post-rewrite (commit --amend) is
# covered by post-commit.
case "$1" in
    rebase) exec .git/hooks/post-merge ;;
esac
```

```sh
$ ls -l $PROJECT_DIRECTORY/.git/hooks/
total 60K
lrwxrwxrwx 1 $USER primarygroup   35 Oct 27 12:50 post-checkout -> /home/$USER/bin/compiledb_hook.sh
lrwxrwxrwx 1 $USER primarygroup   35 Oct 27 12:50 post-commit -> /home/$USER/bin/compiledb_hook.sh
lrwxrwxrwx 1 $USER primarygroup   35 Oct 27 12:50 post-merge -> /home/$USER/bin/compiledb_hook.sh
lrwxrwxrwx 1 $USER primarygroup   38 Oct 27 12:50 post-rewrite -> /home/$USER/bin/post-rewrite_hook.sh
...
```


[comment]: # (TODO http://b/204810365 - compilation db from emerge)

### Install an LSP Clangd Client

Your editor needs a client that speaks LSP.
The [LSP project page][lsp-clients] provides a comprehensive list of LSP
clients.

### Connecting to Chroot Clangd

#### Launching your editor from *inside* chroot

If you launch your editor *inside* the chroot then you can just point your
editor's LSP client at the chroot's `clangd` binary and you're done!

#### Launching your editor from *outside* chroot

For launching your editor outside of the chroot we need to write a short simple
script that essentially wraps the chroot clangd.

It's going to look something like below:

```bash
#!/bin/bash

# I work in CROS EC
EC_DIR="/mnt/host/source/src/platform/ec"

# Don't need color and I want to start the command in the EC directory
# I do not know why but the server is more stable with --no-ns-pid
CROS_SDK_OPTS="--nocolor --no-ns-pid --working-dir $EC_DIR"

# An important clangd option here is 'path-mappings', when your editor talks to
# clangd, the path mappings allow it to understand the path the editor is sending
# over rpc and translate that editor known path to something the clangd server
# understands from its context inside the chroot. This option is also useful for
# remote development, but that's a different albeit similar topic.
# NOTE: You may not even need this option, but do know it exists.
# NOTE: I set these options here, but these may be replacable by configuring
# your editor's LSP client.
CLANGD_OPTS="--inlay-hints --completion-style=detailed --background-index \
--clang-tidy --path-mappings='/bigssd=/home/$USER' --header-insertion=never"

# Let's run the command!
# It runs in the foreground.
cros_sdk $CROS_SDK_OPTS -- clangd $CLANGD_OPTS

# If for whatever reason the editor crashed but clangd didn't stop,
# this cleans any spawned clangd processes.
# This helps if your lsp client ends up having to restart clangd too.
trap 'kill $(jobs -p)' EXIT
```

**NOTE:** When using this script to re/start the server with `cros_sdk --
clangd` after the sudo password timeout, you must prime sudo to be passwordless
with a `sudo` command.

If you use other language servers in the chroot with an editor outside, a
similar wrapper script is likely needed.

Make sure your wrapper script is executable!

Now just point your LSP client at your wrapper script posing as a `clangd`
executable and it should just work.

## See also

* [Language Server Protocol][lsp]
* [ClangD Project][clangd]
* [Sarcasm Notes on Generating Compilation Databases][sarcasm-c-db]

[bear]: https://github.com/rizsotto/Bear
[clangd]: https://clangd.llvm.org
[clangd-c-db]: https://clang.llvm.org/docs/JSONCompilationDatabase.html
[compiledb]: https://github.com/nickdiego/compiledb
[cros-dev-guide]: ./developer_guide.md#chromiumos-developer-guide
[gn]: https://gn.googlesource.com/gn
[go-clion]: http://go/clion-for-chromeos
[lsp]: https://microsoft.github.io/language-server-protocol
[lsp-clients]: https://langserver.org/#implementations-client
[ninja]: https://ninja-build.org
[ninja-tools]: https://ninja-build.org/manual.html#_extra_tools
[permissive-sudo]: ./tips-and-tricks.md#how-to-make-sudo-a-little-more-permissive
[sarcasm-c-db]: https://sarcasm.github.io/notes/dev/compilation-database.html#how-to-generate-a-json-compilation-database
