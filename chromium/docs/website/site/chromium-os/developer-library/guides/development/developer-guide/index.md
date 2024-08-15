---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: developer-guide
title: ChromiumOS Developer Guide
---

## Introduction

This guide describes how to work on ChromiumOS. If you want to help develop
ChromiumOS and you're looking for detailed information about how to get
started, you're in the right place.

[TOC]

### Target audience

The target audience of this guide is anyone who wants to obtain, build, or
contribute to ChromiumOS. That includes new developers who are interested in
the project and who simply want to browse through the ChromiumOS code, as well
as developers who have been working on ChromiumOS for a long time.

### Organization & content

This guide describes the common tasks required to develop ChromiumOS. The guide
is organized linearly, so that developers who are new to ChromiumOS can follow
the tasks in sequence. The tasks are grouped into the following sections:

*   [Prerequisites]
*   [Getting the source code]
*   [Building ChromiumOS]
*   [Installing ChromiumOS on your Device]
*   [Making changes to packages whose source code is checked into ChromiumOS git repositories]
*   [Making changes to non-cros_workon-able packages]
*   [Local Debugging]
*   [Remote Debugging]
*   [Troubleshooting]
*   [Running Tests]
*   [Additional information]

### Typography conventions

*   **Commands** are shown with different labels to indicate whether they apply
    to (1) your build computer (the computer on which you're doing development),
    (2) the [chroot] (ChromeOS SDK) on your build computer, or (3) your ChromiumOS computer (the
    device on which you run the images you build):

Label | Commands
--- | ---
`(outside)` | on your build computer, outside the chroot
`(inside)` | inside the chroot on your build computer
`(in/out)` | on your build computer, either inside or outside the chroot
`(device)` | on your ChromiumOS computer

Beneath the label, the command(s) you should type are prefixed with a generic
shell prompt, `$ `. This distinguishes input from the output of commands, which
is not so prefixed.

*   **Notes** are shown using the following conventions:
    *   **IMPORTANT NOTE** describes required actions and critical information
    *   **SIDE NOTE** describes explanations, related information, and
        alternative options
*   **TODO** describes questions or work that is needed on this document

### Modifying this document

If you're a ChromiumOS developer, **YOU SHOULD UPDATE THIS DOCUMENT** and fix
things as appropriate. See [README.md] for how to update this document. Bias
towards action:

*   If you see a TODO and you know the right answer, fix it!
*   If you see something wrong, fix it.
*   If you're not sure of the perfect answer, still fix it. Stick in a TODO
    noting your uncertainty if you aren't sure, but don't let anything you know
    to be wrong stick around.

Please try to abide by the following guidelines when you modify this document:

*   Put all general "getting started" information directly on this page. It's OK
    if this document grows big.
*   If some information is also relevant for other documents, put the
    information in this document and add links from the other documents to this
    document. If you do not have a choice and must put information relevant to
    getting started in other documents, add links in this document to discrete
    chunks of information in the other documents. Following a web of links can
    be very confusing, especially for people who are new to a project, so it's
    very important to maintain a linear flow as much as possible.
*   Where possible, describe a single way of doing things. If you specify
    multiple options, provide a clear explanation why someone following the
    instructions would pick one option over the others. If there is disagreement
    about how to do things, consider listing alternative options in a SIDE NOTE.
*   Keep Google-specific references to a minimum. A large number of the people
    working on ChromiumOS work at Google, and this document may occasionally
    contain references that are only relevant to engineers at Google. Google
    engineers, please keep such references to a minimum – ChromiumOS is an open
    source project that should stay as open as possible.

### More information

This document provides an overview of the tasks required to develop ChromiumOS.
After you've learned the basics, check out the links in the [Additional
information] section at the end of this document for tips and tricks, FAQs, and
important details (e.g., the ChromiumOS directory structure, using the dev
server, etc.).

Finally, if you build a ChromiumOS image, please read this important note about
[Attribution requirements].

## Prerequisites

You must have Linux to develop ChromiumOS. Any recent or up-to-date
distribution should work. However, we can't support everyone and their dog's
Linux distro, so the only official supported environment is listed below. If you
encounter issues with other setups, patches are generally welcomed, but please
do not expect us to figure out your distro.

*   [Ubuntu] Linux (version 20.04 - Focal)

    Most developers working on ChromiumOS are using Focal (the 20.04 LTS version
    of Ubuntu) and Debian testing (Buster).
    Things might work if you're running a different Linux distribution, but you
    will probably find life easier if you're on one of these.

*   an x86_64 64-bit system for performing the build

*   an account with `sudo` access

    You need root access to run the `chroot` command and to modify the mount
    table. **NOTE:** Do not run any of the commands listed in this document as
    root – the commands themselves will run sudo to get root access when needed.

*   many gigabytes of RAM

    Per [this thread][RAM-thread], linking Chrome requires somewhere between 8
    GB and 28 GB of RAM as of March 2017; you may be able to get by with less at
    the cost of slower builds with adequate swap space. Seeing an error like
    `error: ld terminated with signal 9 [Killed]` while building the
    `chromeos-chrome` package indicates that you need more RAM. If you are not
    building your own copy of Chrome, the RAM requirements will be substantially
    lower.

You will have a much nicer time if you also have:

*   a fast multi-processor machine with lots of memory

    The build system is optimized to make good use of all processors, and an 8
    core machine will build nearly 8x faster than a single core machine.

*   a good Internet connection

    This will help for the initial download (minimum of about 2 GB) and any
    further updates.

### Python

If your system does not have a compatible Python version installed, you'll need
to install Python 3.8 or greater.

To check your python version:
```bash
(outside)
$ python3 -V
```

If an error or a version lower than 3.8 is returned, proceed with the rest of
the section. If not, skip this section.

To install a specific Python version on your system, there are two options:
- The first option is to install a new Python version via the system packages.

  If an incompatible version is present, uninstall it:
  ```bash
  (outside)
  $ sudo apt-get remove <current Python package>
  ```
  And install a new version:
  ```bash
  (outside)
  $ sudo apt-get install python3.9
  ```
- The second option is to use pyenv. Pyenv has the advantage of allowing
  multiple Python versions to co-exist on your system and each project can
  specify individually specify which version to use.

  To use pyenv, install pyenv according to the notes on
  https://github.com/pyenv/pyenv . At this moment of writing on bash
  based system it should be something like this:

    ```bash
    (outside)
    $ git clone https://github.com/pyenv/pyenv.git ~/.pyenv
    $ cd ~/.pyenv && src/configure && make -C src && cd ..
    $ echo -e 'if shopt -q login_shell; then' \
      '\n  export PYENV_ROOT="$HOME/.pyenv"' \
      '\n  export PATH="$PYENV_ROOT/bin:$PATH"' \
      '\n eval "$(pyenv init --path)"' \
      '\nfi' >> ~/.bashrc
    $ echo -e 'if [ -z "$BASH_VERSION" ]; then'\
      '\n  export PYENV_ROOT="$HOME/.pyenv"'\
      '\n  export PATH="$PYENV_ROOT/bin:$PATH"'\
      '\n  eval "$(pyenv init --path)"'\
      '\nfi' >>~/.profile
    $ source ~/.profile
    ```

    To check that pyenv is properly installed:
    ```bash
    (outside)
    $ pyenv
    ```
    This should not have returned any error. If an error was raised, check for
    help at https://github.com/pyenv/pyenv

    To install a Python version on your system and make it the default:

    ```bash
    (outside)
    $ pyenv install 3.9.6
    $ pyenv global 3.9.6
    ```

    To check if the version is correctly installed:
    ```bash
    (outside)
    $ python3 -V
    Python 3.9.6
    ```

    If `Python 3.9.6` is not displayed, please check pyenv documentation and
    forums on how to solve the issue.

    Useful commands:
     - Check which versions can be installed: `pyenv install --list`
     - Install an additional versions: `pyenv install <version>`.
     - Check the versions installed on the current system: `pyenv versions`
     - Check the version active in the current directory: `pyenv version`

    In the previous step python 3.9.6 was define as the default python. If a
    project needs a specific python version, include a .python-version file in
    the root directory of the project with the required python version as
    content and install the python version with `pyenv install` if it is not yet
    present on your system.

### Install development tools

Some host OS tools are needed to manipulate code, bootstrap the development
environment, and run preupload hooks later on.

Install the git revision control system, the curl download helper, and more.
On Ubuntu 20.04 Focal, the magic incantation is:

```bash
(outside)
$ sudo add-apt-repository universe
$ sudo apt-get install git gitk git-gui curl xz-utils \
    python3-pkg-resources python3-virtualenv python3-oauth2client
```

On Debian Buster, the commands are similar:

```bash
(outside)
$ sudo apt-get install git gitk git-gui curl xz-utils \
    python3-pkg-resources python3-virtualenv python3-oauth2client
```

These commands also installs git's graphical front end (`git gui`) and revision
history browser (`gitk`).

### Install depot_tools

To get started, follow the initial instructions at [install depot_tools].
You only need to clone the repo & setup your PATH -- the rest of the document
is for browser developers.

This step is required so that you can use the `repo` to get/sync the source
code, as well as other CrOS specific tools.

### Optionally tweak your sudoers configuration

You can tweak your sudoers configuration to make `sudo` request your password
less frequently as described in [Making sudo a little more permissive].

### Set locale

These may not be needed if you are building on a system that you
already use, however if you have a clean instance on GCE, you'll need
to set a better locale. For example, on Debian Buster on GCE, do:

```bash
(outside)
$ sudo apt-get install locales
$ sudo dpkg-reconfigure locales
```

When running `dpkg-reconfigure locales`, choose a language with UTF-8, e.g.
`en_US.UTF-8`. For this change to take effect, you will need to log out and back
in (closing all terminal windows, tmux/screen sessions, etc.).

### Configure git

Setup git now. If you don't do this, you may run into errors/issues
later. Replace `you@example.com` and `Your Name` with your information:

```bash
(outside)
$ git config --global user.email "you@example.com"
$ git config --global user.name "Your Name"
```

### Get credentials to access source repos

Follow the [Gerrit guide] to get machine access credentials for the source
repos.

This will also set up your code review account(s), which you can use to upstream
changes back to ChromiumOS. This will be discussed in more detail in the
"[Making changes to packages whose source code is checked into ChromiumOS git repositories]"
section.

### Double-check that you are running a 64-bit architecture

Run the following command:

```bash
(outside)
$ uname -m
```

You should see the result: `x86_64`

If you see something else (for example, `i686`, which means you are on a 32-bit
machine or a 64-bit machine running a 32-bit OS) then you won't be able to build
ChromiumOS. The project would happily welcome patches to fix this.

### Verify that your default file permissions (umask) setting is correct

Sources need to be world-readable to properly function inside the chroot
(described later). For that reason, the last digit of your umask should not be
higher than 2, e.g. 002 or 022. Many distros have this by default; Ubuntu, for
instance, does not. It is essential to put the following line into your
`~/.bashrc` file before you checkout or sync your sources.

```bash
(outside)
$ umask 022
```

You can verify that this works by creating any file and checking if its
permissions are correct.

```bash
(outside)
$ touch ~/foo
$ ls -la ~/foo
-rw-r--r-- 1 user group 0 2012-08-30 23:09 /home/user/foo
```

## Get the Source

### Decide where your source will live

ChromiumOS developers commonly put their source code in
`~/chromiumos`. If you feel strongly, put your own source elsewhere, but
note that **all commands in this document assume that your source code is in**
`~/chromiumos`.

Create the directory for your source code with this command:

```bash
(outside)
$ mkdir -p ~/chromiumos
```

**IMPORTANT NOTE:** If your home directory is on NFS, you **must** place your
code somewhere else. Not only is it a bad idea to build directly from NFS for
performance reasons, but builds won't actually work (builds use sudo, and root
doesn't have access to your NFS mount, unless your NFS server has the
`no_root_squash` option). Wherever you place your source, you can still add a
symbolic link to it from your home directory (this is suggested), like so:

```bash
(outside)
$ mkdir -p /usr/local/path/to/source/chromiumos
$ ln -s /usr/local/path/to/source/chromiumos ~/chromiumos
```

### Get the source code

ChromiumOS uses [repo] to sync down source code. `repo` is a wrapper for the
[git] that helps deal with a large number of `git` repositories. You already
installed `repo` when you [installed `depot_tools` above][install depot_tools].

**Public:**
```bash
(outside)
$ cd ~/chromiumos
$ repo init -u https://chromium.googlesource.com/chromiumos/manifest -b main
$ repo sync -j4
```

*** note
**Note:** If you are using public manifest with devices that have restricted
binary prebuilts, such as GPU drivers on ARM devices, you'll have to
explicitly accept licenses. Read more at
https://www.chromium.org/chromium-os/licensing/building-a-distro/
***

**Googlers/internal manifest:**
```shell
(outside)
$ cd ~/chromiumos
$ repo init -u https://chrome-internal.googlesource.com/chromeos/manifest-internal -b main
$ repo sync -j4
```

*** note
**Note:** `-j4` tells `repo` to concurrently sync up to 4 repositories at once.
You can adjust the number based on how fast your internet connection is. For
the initial sync, it's generally requested that you use no more than 8
concurrent jobs. (For later syncs, when you already have the majority of the
source local, using -j16 or so is generally okay.)
***

*** note
**Note:** If you are on a slow network connection or have low disk space, you
can use the `-g minilayout` option. This starts you out with a minimum
amount of source code. This isn't a particularly well tested configuration and
has been known to break from time-to-time, so we usually recommend against it.
***

#### Optionally add Google API keys

*** note
**Note:** If you plan to build the image with Chrome, you can skip this step.
These keys are only necessary when you build the image with Chromium and need to
use features that access Google APIs (**signing in**, translating web pages,
geolocation, etc).
***

You will need to have keys (see [API Keys]) either in your include.gypi, or in a
file in your home directory called ".googleapikeys". If either of these file are
present for step 1 of building (below) they will be included automatically. If
you don't have these keys, these features of Chromium will be quietly disabled.

#### Branch Builds

If you want to build on a branch, pass the branch name to repo init (e.g: `repo
init -u <URL> [-g minilayout] -b release-R80-12739.B`).

When you use `repo init` you will be asked to confirm your name, email address,
and whether you want color in your terminal. This command runs quickly. The
`repo sync` command takes a lot longer.

More info can be found in the [working on a branch page].

### Make sure you are authorized to access Google Storage (GS) buckets

Building and testing ChromiumOS requires access to Google Storage. This is done
via [gsutil]. Once configured, an authorization key is placed in `~/.boto`.
Every time you access the [chroot] via `cros_sdk`, the `.boto` file is copied to
the [chroot]. If you run [gsutil] inside the chroot, it will configure the key
in the chroot version of `~/.boto`, but every time you re-run `cros_sdk`, it
will overwrite the `~/.boto` file in the chroot.

## Building ChromiumOS

### Create a chroot

To make sure everyone uses the same exact environment and tools to build
ChromiumOS, all building is done inside a [chroot]. This chroot is its own
little world: it contains its own compiler, its own tools (its own copy of bash,
its own copy of sudo), etc. Now that you've synced down the source code, you
need to create this chroot. Assuming you're already in `~/chromiumos` (or
wherever your source lives), the command to download and install the chroot is:

```bash
(outside)
$ cros_sdk
```

If this does not work, make sure you've added the depot_tools directory to your
PATH already (as was needed above with using `repo`).

This will download and setup a prebuilt chroot from ChromiumOS mirrors (under
400MB). If you prefer to build it from source, or have trouble accessing the
servers, use `cros_sdk --bootstrap`. Note that this will also enter the chroot.
If you prefer to build only, use `--download`.

The command with `--bootstrap` takes about half an hour to run on a four core
machine. It compiles quite a bit of software, which it installs into your
chroot, and downloads some additional items (around 300MB). While it is building
you will see a regular update of the number of packages left to build. Once the
command finishes, the chroot will take up total disk space of a little over 3GB.

The chroot lives by default at `~/chromiumos/chroot`. Inside that
directory you will find system directories like `/usr/bin` and `/etc`. These are
local to the chroot and are separate from the system directories on your
machine. For example, the chroot has its own version of the `ls` utility. It
will be very similar, but it is actually a different binary than the normal one
you use on your machine.

**SIDE NOTES:**

*   You shouldn't have to create the chroot very often. Most developers create
    it once and never touch it again unless someone explicitly sends out an
    email telling them to recreate their chroot.
*   The `cros_sdk` command currently doesn't work behind a proxy server, but
    there is a [workaround][crosbug/10048].

After `cros_sdk` is complete, it will enter the chroot and give you a shell.

### Enter the chroot

Most of the commands that ChromiumOS developers use on a day-to-day basis
(including the commands to build a ChromiumOS image) expect to be run from
within the chroot. You can enter the chroot by calling:

```bash
(outside)
$ cros_sdk
```

This is the same command used to create the chroot, but if the chroot already
exists, it will just enter.

NOTE: if you want to run a single command in the chroot (rather than entering
the chroot), prefix that command with `cros_sdk -- `.

This command will probably prompt you for your password for the `sudo` command
(entering the chroot requires root privileges). Once the command finishes, that
terminal is in the chroot and you'll be in the `~/chromiumos/src/scripts`
directory, where most build commands live. In the chroot you can only see a
subset of the filesystem on your machine. However, through some trickery (bind
mounts), you will have access to the whole `src` directory from within the
chroot – this is so that you can build the software within the chroot.

Note in particular that the `src/scripts` directory is the same `src/scripts`
directory found within the ChromiumOS directory you were in before you entered
the chroot, even though it looks like a different location. That's because when
you enter the chroot, the `~/chromiumos` directory in the chroot is mounted such
that it points to the main ChromiumOS directory `~/chromiumos` outside the
chroot. That means that changes that you make to the source code outside of the
chroot immediately take effect inside the chroot.

Calling this will also install a chroot, if you don't have one yet, for example
by not following the above.

While in the chroot you will see a special "(cr)" prompt to remind you
that you are there:

```bash
(cr) ((...)) johnnyrotten@flyingkite ~/chromiumos/src/scripts $
```

You generally cannot run programs on your filesystem from within the chroot. For
example, if you are using eclipse as an IDE, or gedit to edit a text file, you
will need to run those programs outside the chroot. As a consolation, you can
use vim. If you are desperate for Emacs, try typing `sudo emerge emacs`. Of
course this command will build Emacs from source so allow 5-10 minutes.

**IMPORTANT NOTES**:

*   **If you need to delete your chroot**, use `cros_sdk --delete` to delete it
    properly. Using `rm -rf` could end up deleting your source tree due to the
    active bind mounts.

**SIDE NOTES:**

*   If you need to share lots of files inside and outside chroot (for example,
    settings for your favorite editor / tools, or files downloaded by browser
    outside chroot, etc.), read [Tips and Tricks].
*   There is a file system loop because inside `~/chromiumos` you will find the
    chroot again. Don't think about this for too long. If you try to use `du -s
    ~/chromiumos/chroot/home` you might get a message about a corrupted
    file system. This is nothing to worry about, and just means that your
    computer doesn't understand this loop either. (If you _can_ understand this
    loop, try [something harder].)

### Select a board

Building ChromiumOS produces a disk image (usually just called an "image") that
can be copied directly onto the boot disk of a computer intended to run
ChromiumOS. Depending on the specifics of that computer, you may want different
files in the disk image. For example, if your computer has an ARM processor,
you'll want to make sure that all executables in the image are compiled for the
ARM instruction set. Similarly, if your computer has special hardware, you'll
want to include a matching set of device drivers.

Different classes of computers are referred to by ChromiumOS as different
target "boards". The following are some example boards:

*   **amd64-generic** - builds a generic image suitable for computers with an
    x86_64-compatible CPU (64 bit) or for running in a VM
*   **arm-generic** - builds a generic image suitable for computers with an ARM
    CPU (32 bit)
*   **arm64-generic** - builds a generic image suitable for computers with an
    ARM CPU (64 bit)
*   **samus, eve, \<your board name>** - builds an image specific to the chosen
    device (find your board name [here][ChromeOS Devices]); recommended for
    deploying to official hardware
*   **betty** - (Googlers only) builds an ARC++-enabled image for running in a
    VM

You need to choose a board for your first build. Be aware that the generic
images may not work well (or not at all) when run on official hardware. Don't
worry too much about this choice, though – you can always build for another
board later. If you want a list of known boards, you can look in
`~/chromiumos/src/overlays`.

Each command in the build processes takes a `--board` parameter. To facilitate
this, it can be helpful to keep the name of the board in a shell variable. This
is not strictly necessary, but if you do this, you can simply copy and paste the
commands below into your terminal program. Enter the following inside your
chroot:

```bash
(inside)
$ export BOARD=<your pick of board>
```

This setting only holds while you stay in the chroot. If you leave and come
back, you need to specify this setting again.

**SIDE NOTES:**

*   If you look at `~/chromiumos/src/overlays`, you may notice two different
    naming patterns: `overlay-BOARD` and `overlay-variant-BOARD-VARIANT` (note
    the dash here). If you intend to build for particular board variant, you'll
    want to use `BOARD_VARIANT` pair (note the underscore here) as your board
    name. E.g. for overlay-variant-peach-pi the correct value for `BOARD` would
    be `peach_pi`.

### Initialize the build for a board

To start building for a given board, issue the following command inside your
chroot (you should be in the `~/chromiumos/src/scripts` directory):

```bash
(inside)
$ setup_board --board=${BOARD}
```

This command sets up the board target with a default sysroot of
`/build/${BOARD}`. The command downloads a small amount of stuff and takes a few
minutes to complete.
`setup_board` also calls `update_chroot`, which will update the toolchain if
`repo sync` has been run and the toolchain is out of date.

**SIDE NOTES:**

*   If you pass the `--default` flag to `setup_board`, the command writes the
    board name in the file `~/chromiumos/src/scripts/.default_board` (it's the
    same thing as `echo ${BOARD} > ~/chromiumos/src/scripts/.default_board`).
    This makes it so that you don't need to specify a `--board` argument to
    subsequent commands. These instructions do not use the `--default` flag so
    that you can explicitly see what commands are board-specific.
*   If you want to clobber your old board files and start fresh,
    try passing the `--force` flag, which deletes the old `/build/${BOARD}`
    directory for you. Like `cros_sdk`, most people only re-run `setup_board`
    when told to (they don't re-run it even after a `repo sync`).
*   You can delete the board sysroot at any time with: `sudo rm -rf
    /build/${BOARD}`

### Set the chronos user password

On a ChromiumOS computer, you can get command line access (and root access
through the `sudo` command) by logging in with the shared user account
`"chronos"`. You should set a password for the `chronos` user by entering the
command below from inside the `~/chromiumos/src/scripts` directory:

```bash
(inside)
$ ./set_shared_user_password.sh
```

You will be prompted for a password, which will be stored in encrypted form in
`/etc/shared_user_passwd.txt`.

**SIDE NOTES:**

*   The encrypted password is stored inside chroot. That means that if you
    recreate your chroot, you have to run this command again.
*   If you don't set a shared user password, the password for the chronos
    account may end up being any number of things depending on some complicated
    (and constantly evolving) formula that includes whether you build a
    developer image, whether you boot into [Developer Mode], and the current
    state of the scripts. The password might be empty, or a well-known string
    (such as `"test0000"` on test images), or it might be impossible to login
    with chronos. It is strongly recommended that you simply set a shared user
    password. TODO: put a link to some place with more info about this.

### Build the packages for your board

To build all the packages for your board, run the following command:

```bash
(inside)
$ build_packages --board=${BOARD}
```

This step is the rough equivalent of `make all` in a standard Makefile
system. This command handles incremental builds; you should run it whenever you
change something and need to rebuild it (or after you run `repo sync`).

Normally, the build_packages command builds the stable version of a package
(i.e. from committed git sources), unless you are working on a package (with
`cros_workon`). If you are working on a package, build_packages will build using
your local sources. See below for information about `cros_workon`.

**SIDE NOTES:**

*   The first time you run `build_packages`, you should use
    `--autosetgov --autosetgov-sticky`.  If your system uses a power-saving
    [CPU
    governor](https://www.kernel.org/doc/Documentation/cpu-freq/governors.txt),
    these options will temporarily switch to `performance` for faster builds.
    `--autosetgov-sticky` makes `build_packages` remember to always use
    `--autosetgov`.
*   (For Googlers) `build_packages` is generally enough to include Chrome in
    your image if you have included the internal manifest when initializing the
    repo. However, there may be some cases where the build produces Chromium
    instead. Running the command again including the flags `--internal` and
    `--no-use-any-chrome` guarantees Chrome is included in the build.
    `--no-use-any-chrome` creates a fresh build of Chromium/Chrome instead of
    reusing an existing prebuilt, making the build process slower.
*   Like `setup_board`, `build_packages` also calls `update_chroot`, which will
    keep the SDK up-to-date with any possible changes.
*   By default, packages other than Chrome will be compiled in debug mode; that
    is, with `NDEBUG` undefined and with debugging constructs like `DCHECK`,
    `DLOG`, and the red "Debug image" message present. If you supply
    `--no-withdebug`, then `NDEBUG` will be defined and the debugging constructs
    will be removed.
*   The first time you run the `build_packages` command, it will take a long
    time (around 90 minutes on a four core machine), as it must build every
    package, and also download about 1.7GB of source packages and 1.3GB of
    binary packages. [See here][What does build_packages actually do?] for more
    information about what the build_packages command actually does. In short,
    it tries to download existing binary packages to avoid building anything
    (and puts them in `/build/${BOARD}/packages` for subsequent builds). Failing
    that, it downloads source packages, puts them in
    `/var/lib/portage/distfiles-target`, and builds them.

### Build a disk image for your board

Once the `build_packages` step is finished, you can build a ChromiumOS-base
developer image by running the command below from inside the
`~/chromiumos/src/scripts` directory:

```bash
(inside)
$ build_image --board=${BOARD} --no-enable-rootfs-verification test
```

The arguments for `build_image` specify what type of build you want. A test
image (in the example above) has additional test-specific packages and also
accepts incoming SSH connections. It is more convenient to use test images, but
developers could also build developer images. A developer image provides a
ChromiumOS-based image with additional developer packages. To build it use `dev`
instead of `test`. If building a test image, the password set using
`set_shared_user_password.sh` will be ignored and `"test0000"` will be the
password instead. The `--no-enable-rootfs-verification` turns off verified boot
allowing you to freely modify the root file system. The system is less secure
using this flag, however, for rapid development you may want to set this flag.
If you would like a more secure, locked-down version of ChromiumOS, then simply
remove the `--no-enable-rootfs-verification` flag. Finally if you want just the
pristine ChromiumOS-based image (closest to ChromeOS but not quite the same),
pass in `base` rather than `test` or `dev`. Use `build_image --help` for more
information.

The image produced by `build_image` will be located in
`~/chromiumos/src/build/images/${BOARD}/versionNum/` (where `versionNum` will
actually be a version number). The most recent image produced for a given board
will be symlinked to `~/chromiumos/src/build/images/${BOARD}/latest`.

At the end of `build_image`'s output, it will print a command you can run to
start the image in a `cros_vm` virtual machine. It will print something like:

```
To run the image in a virtual machine, use:
cros_vm --start --image-path=../build/images/amd64-generic/R111-15301.0.0-d2023_01_04_103804-a1/chromiumos_test_image.bin --board=amd64-generic
```

Remember this command for future use; see [Running an image in a virtual
machine](#running-an-image-in-a-virtual-machine).

**IMPORTANT NOTE:** It's up to you to delete old builds that you don't need.
Every time you run `build_image`, the command creates files that take up to
**8GB of space (!)**.

### Look at your disk image (optional)

The preferred way to mount the image you just built to look at its contents is:

```bash
(inside)
$ ./mount_gpt_image.sh --board=${BOARD} --safe --most_recent
```

If you built a test image, also make sure to add `-i chromiumos_test_image.bin`
to this command.

The `--safe` option ensures you do not make accidental changes to the Root FS.

Again, don't forget to unmount the root filesystem when you're done:

```bash
(inside)
$ ./mount_gpt_image.sh --board=${BOARD} -u
```

Optionally, you can unpack the partition as separate files and mount them
directly:

```bash
(inside)
$ cd ~/chromiumos/src/build/images/${BOARD}/latest
$ ./unpack_partitions.sh chromiumos_image.bin
$ mkdir -p rootfs
$ sudo mount -o loop,ro part_3 rootfs
```

This will do a loopback mount of the rootfs from your image to the location
`~/chromiumos/src/build/images/${BOARD}/latest/rootfs` in your chroot.

If you built with `--no-enable-rootfs-verification` you can omit the `ro`
option to mount it read write.

If you built an x86 ChromiumOS image, you can probably even try chrooting into
the image:

```bash
(inside)
$ sudo chroot ~/chromiumos/src/build/images/${BOARD}/latest/rootfs
```

This is a little hacky (the ChromiumOS rootfs isn't really designed to be a
chroot for your host machine), but it seems to work pretty well. Don't forget to
`exit` this chroot when you're done.

When you're done, unmount the root filesystem:

```bash
(inside)
$ sudo umount ~/chromiumos/src/build/images/${BOARD}/latest/rootfs
```

## Installing ChromiumOS on your Device

### Put your image on a USB disk

The easiest way to get your image running on your target computer is to put the
image on a USB flash disk (sometimes called a USB key), and boot the target
computer from the flash disk.

The first step is to disable auto-mounting of USB devices on your build computer
as it may corrupt the disk image while it's being written. On systems that use
GNOME or Cinnamon, run the following:

```bash
(outside)
$ gsettings set org.gnome.desktop.media-handling automount false
$ gsettings set org.gnome.desktop.media-handling automount-open false
$ gsettings set org.cinnamon.desktop.media-handling automount false
$ gsettings set org.cinnamon.desktop.media-handling automount-open false
```

Next, insert a USB flash disk (8GB or bigger) into your build computer. **This
disk will be completely erased, so make sure it doesn't have anything important
on it**. Wait ~10 seconds for the USB disk to register, then type the following
command:

```bash
(inside)
$ cros flash usb:// ${BOARD}/latest
```

For more details on using this tool, see the [CrOS Flash page].

When the `cros flash` command finishes, you can simply unplug your USB key and
it's ready to boot from.

**IMPORTANT NOTE:** To emphasize again, `cros flash` completely replaces the
contents of your USB disk. Make sure there is nothing important on your USB disk
before you run this command.

**SIDE NOTES:**

*   If you want to create a test image (used for integration testing), see the
    [Running Tests] section.

### Enter Developer Mode

See the [Developer Mode] documentation.

### Boot from your USB disk

After enabling [Developer Mode], you should set your system to boot from USB.

Let the device boot, login and open a shell (or switch to terminal 2 via
Ctrl+Alt+F2).

Run the following command:

```bash
(device)
$ sudo crossystem
```

You should see `dev_boot_usb` equal to 0. Set it to 1 to enable USB boot:

```bash
(device)
$ sudo crossystem dev_boot_usb=1
$ sudo crossystem dev_boot_signed_only=0
```

Now reboot. On the white screen (indicating [Developer Mode] is enabled),
plug-in the USB disk and press `Ctrl+U` ([Debug Button Shortcuts]).

### Getting to a command prompt on ChromiumOS

Since you set the shared user password (with `set_shared_user_password.sh`) when
you built your image, you have the ability to login as the chronos user:

1.  After your computer has booted to the ChromiumOS login screen, press `[
    Ctrl ] [ Alt ] [ F2 ]` to get a text-based login prompt. ( `[ F2 ]` may
    appear as `[ → ]` on your Notebook keyboard.)
2.  Log in with the chronos user and enter the password you set earlier.

Because you built an image with developer tools, you also have an alternate way
to get a terminal prompt. The alternate shell is a little nicer (in the very
least, it keeps your screen from dimming on you), even if it is a little harder
to get to. To use this alternate shell:

1.  Go through the standard ChromiumOS login screen (you'll need to setup a
    network, etc.) and get to the web browser. It's OK to login as guest.
2.  Press `[ Ctrl ] [ Alt ] [ T ]` to get the [crosh] shell.
3.  Use the `shell` command to get the shell prompt. NOTE: you **don't** need to
    enter the `chronos` password here, though you will still need the password
    if you want to use the `sudo` command.

### Installing your ChromiumOS image to your hard disk

Once you've booted from your USB key and gotten to the command prompt, you can
install your ChromiumOS image to the hard disk on your computer with this
command:

```bash
(device)
$ /usr/sbin/chromeos-install
```

**IMPORTANT NOTE:** Installing ChromiumOS onto your hard disk will **WIPE YOUR
HARD DISK CLEAN**.

### Running an image in a virtual machine

Many times it is easier to simply run ChromiumOS in a virtual machine like QEMU.
You can use the [cros_vm] command to start a VM with the previously built image.

When you start the VM, `cros_vm` will print out information about how to connect
to the running image via SSH and VNC.

For VNC it will normally say `VNC server running on ::1:5900` which means it's
serving on localhost on the default VNC port (5900). You can connect to
`localhost` with a VNC viewer to connect.

A good VNC client for Linux is the package tigervnc-viewer (available on at
least Debian); its command line program is `vncviewer`. Note that before you use
it, you should click **Options** → **Misc** → **Show dot when no cursor**.

***note
**SIDE NOTES:**

*   The output of the `build_image` command will also contain a full command
    to start a VM with that image.
*   Only KVM/QEMU VMs are actively supported at the moment.
    *   Non-KVM VMs often "work", but are unbearably slow.
*   If you're interested in creating a test image (used for integration
    testing), see the [Running Tests] section.
***

## Making changes to packages whose source code is checked into ChromiumOS git repositories

Now that you can build and run ChromiumOS, you're ready to start making changes
to the code. For further hands on with making changes, you can check out the
[build codelab].

**NOTE:** If you skipped to this section without building your own system image,
you may run into hard-to-fix dependency problems if you build your own versions
of system packages and try to deploy them to a system image that was built by a
builder. If you run into trouble, try going through the full [Building
ChromiumOS] process first and installing your own system image.

### Keep the tree green

Before you start, take a moment to understand Chromium's source management
strategy of "keeping the tree green". For the ChromiumOS project, **keeping the
tree green** means:

1.  Any new commits should not destabilize the build:
    *   Images built from the tree should always have basic functionality
        working.
    *   There may be minor functionality not working, and it may be the case,
        for example, that you will need to use Terminal to fix or work around
        some of the problems.
2.  If you must introduce unstable changes to the tree (which should happen
    infrequently), you should use parameterization to hide new, unstable
    functionality behind a flag that's turned off by default. The ChromiumOS
    team leaders may need to develop mechanisms for parameterizing different
    parts of the system (such as the init script).
3.  Internal "dev channel" releases will be produced directly from the tree,
    with a quick branch to check-point the release version. Any fixes required
    for a release will be pulled from the tree, avoiding merges back to tree.

This strategy has many benefits, including avoiding separate build trains for
parallel development (and the cost of supporting such development), as well as
avoiding large, costly merges from forked branches.

**SIDE NOTE**: "Keep the tree green" means something a bit different for
ChromiumOS than for Chromium, which is much further along in its life cycle.

The steps in this section describe how to make changes to a ChromiumOS package
whose source is checked into the ChromiumOS source control
system. Specifically, this is a package where:

*   The `**ebuild**` for the package lives in the
    `src/third_party/chromiumos-overlay` or `src/overlays/overlay-${BOARD}`
    directories.
*   There is an ebuild for the package that ends with  `9999.ebuild`.
*   The ebuild inherits from the `cros-workon`  class.
*   The ebuild has a `KEYWORDS` in the ebuild containing this architecture name
    (like "`x86`").

You can see a list of all such packages by running the following command from
inside the `~/chromiumos/src/scripts` directory:

```bash
(inside)
$ cros_workon --board=${BOARD} --all list
```

### Run cros_workon start

The first thing you need to do is to mark the package as active. Use the command
below, replacing `${PACKAGE_NAME}` with your package name (e.g., `chromeos-wm`):

```bash
(inside)
$ cros_workon --board=${BOARD} start ${PACKAGE_NAME}
```

This command:

*   Indicates that you'd like to build the `9999` version of the `ebuild`
    instead of the stable, committed version.
*   Indicates that you'd like to build from source every time.
*   If you specified that you wanted the `minilayout` when you did your `repo
    init`, this command adds a clause to your `.repo/local_manifest.xml` to tell
    `repo` to sync down the source code for this package next time you do a
    `repo sync`.

### Run repo sync

After running `cros_workon`, sync down the sources. This is critical if you're
using the `minilayout`, but is probably a good idea in any case to make sure
that you're working with the latest code (it'll help avoid merge conflicts
later). Run the command below anywhere under your `~/chromiumos` directory:

```bash
(outside)
$ repo sync
```

*** note
**Note:** Make sure your `umask` is [set to a supported
value](#Verify-that-your-default-file-permissions-umask_setting-is-correct)
(e.g. 022); otherwise, you may end up with bad file permissions in your source
tree.
***

### Find out which ebuilds map to which directories

The `cros_workon` tool can help you find out what ebuilds map to each
directory. You can view a full list of ebuilds and directories using the
following command:

```bash
(inside)
$ cros_workon --board=${BOARD} --all info
```

If you want to find out which ebuilds use source code from a specific directory,
you can use grep to find them. For example:

```bash
(inside)
$ cros_workon --board=${BOARD} --all info | grep platform/ec
```

This returns the following output:

```bash
chromeos-base/ec-utils chromiumos/platform/ec src/platform/ec
```

This tells you the following information:

1.  The name of the ebuild is `chromeos-base/ec-utils`
2.  The path to the git repository on the server is `chromiumos/platform/ec`
3.  The path to the source code on your system is `src/platform/ec`

You can similarly find what source code is associated with a given ebuild by
grepping for the ebuild name in the list.

To find out where the ebuild lives:

```bash
(inside)
$ equery-${BOARD} which ${PACKAGE_NAME}
```

As an example, for `PACKAGE_NAME=ec-utils`, the above command might display:

```bash
/home/.../chromiumos/src/third_party/chromiumos-overlay/chromeos-base/ec-utils/ec-utils-9999.ebuild
```

**SIDE NOTE:** If you run the same command **without** running `cros_workon`
first, you can see the difference:

```bash
/home/.../chromiumos/src/third_party/chromiumos-overlay/chromeos-base/ec-utils/ec-utils-0.0.1-r134.ebuild
```

### Create a branch for your changes

Since ChromiumOS uses `repo`/`git`, you should always create a local branch
whenever you make changes.

First, find the source directory for the project you just used `cros_workon` on.
This isn't directly related to the project name you used with `cros_workon`.
(TODO: This isn't very helpful - someone with more experience, actually tell us
how to find it reliably? --Meredydd)

cd into that directory, in particular the `"files/"` directory in which the
actual source resides. In the command below, replace `${BRANCH_NAME}` with a
name that is meaningful to you and that describes your changes (nobody else will
see this name):

```bash
(in/out)
$ repo start ${BRANCH_NAME} .
```

The branch that this creates will be based on the remote branch (TODO: which
one? --Meredydd). If you've made any other local changes, they will not be
present in this branch.

### Make your changes

You should be able to make your changes to the source code now. To incrementally
compile your changes, use either `cros_workon_make` or `emerge-${BOARD}`. To use
`cros_workon_make`, run

```bash
(inside)
$ cros_workon_make --board=${BOARD} ${PACKAGE_NAME}
```

This will build your package inside your source directory. Change a single file,
and it will rebuild only that file and re-link. If your package contains test
binaries, using

```bash
(inside)
$ cros_workon_make --board=${BOARD} ${PACKAGE_NAME} --test
```

will build and run those binaries as well. In case your tests have
out-of-package dependencies, you'll first need to run

```bash
(inside)
$ USE=test emerge-${BOARD} ${PACKAGE_NAME}
```

to pull them in.

Call `cros_workon_make --help` to see other options that are supported.

You probably want to get your changes onto your device now. You need to install
the changes you made by using

```bash
(inside)
$ cros_workon_make --board=${BOARD} ${PACKAGE_NAME} --install
```

You can then rebuild an image with `build_image` and reimage your device.

Alternatively, you can build your package using `emerge-${BOARD}` and quickly
install it to the device by using [cros deploy].

For example, if you want to build `ec-utils` to test on your device, use

```bash
(inside)
$ emerge-${BOARD} ec-utils
```

To install the package to the device, use

```bash
(inside)
$ cros deploy ${IP} ec-utils
```

### Set your editor

Many of the commands below (in particular `git`) open up an editor. You probably
want to run one of the three commands below depending on your favorite editor.

If you're not a \*nix expert, `nano` is a reasonable editor:

```bash
$ export EDITOR='nano'
```

If you love  `vi`:

```bash
$ export EDITOR='vi'
```

If you love `emacs` (and don't want an X window to open up every time you do
something):

```bash
$ export EDITOR='emacs -nw'
```

You should probably add one of those lines to your `.bashrc` (or similar file)
too.

### Submit changes locally

When your changes look good, commit them to your local branch using `git`. Full
documentation of how to use git is beyond the scope of this guide, but you might
be able to commit your changes by running something like the command below from
the project directory:

```bash
(in/out)
$ git commit -a
```

The git commit command brings up a text editor. You should describe your
changes, save, and exit the editor. Note that the description you provide is
only for your own use. When you upload your changes for code review, the repo
upload command grabs all of your previous descriptions, and gives you a chance
to edit them.

### Upload your changes and get a code review

Check out our [Gerrit Workflow] guide for details on our review process.

### Clean up after you're done with your changes

After you're done with your changes, you're ready to clean up. The most
important thing to do is to tell `cros_workon` that you're done by running the
following command:

```bash
(inside)
$ cros_workon --board=${BOARD} stop ${PACKAGE_NAME}
```

This command tells `cros_workon` to stop forcing the `-9999.ebuild` and to stop
forcing a build from source every time.

If you're using the `minilayout`, doing a `cros_workon` stop **will not** remove
your source code. The code will continue to stay on your hard disk and get
synced down.

## Making changes to non-cros_workon-able packages

If you want to make changes to something other than packages which source is
checked into the ChromiumOS source control system, you can follow the
instructions in the previous section, but skip the `cros_workon` step. Note
specifically that you still need to run `repo start` to [Create a branch for
your changes].

The types of changes that fall into this category include:

*   changes to build scripts (pretty much anything in `src/scripts`)
*   changes to `ebuild` files themselves (like the ones in
    `src/third_party/chromiumos-overlay`)
    *   To change these files, you need to "manually uprev" the package. For
        example, if we're making a modification to for the Pixel overlay (link),
        then
    *   `cd src/overlays/overlay-link/chromeos-base/chromeos-bsp-link`
    *   `mv chromeos-bsp-link-0.0.2-r29.ebuild chromeos-bsp-link-0.0.2-r30.ebuild`
    *   `chromeos-bsp-link-0.0.2-r29.ebuild` is a symlink that points to
        `chromeos-bsp-link-0.0.2.ebuild`. To uprev the package, simply increment the
        revision (r29) number.
    *   Note: Upreving should not be done when there is an ebuild for the
        package that ends with  `9999.ebuild`. Changes to the ebuild should
        usually be done in the `9999.ebuild` file.
*   adding small patches to existing packages whose source code is NOT checked
    into ChromiumOS git repositories
*   changes to `eclass` files (like the ones in
    `src/third_party/chromiumos-overlay/eclass`)
*   changes to the buildbot infrastructure (in `crostools`)
*   changes to ChromiumOS project documentation (in `docs`)
*   TODO: anything else?

### Adding small patches to existing packages

When you need to add small patches to existing packages whose source code is not
checked into a ChromiumOS git repository (e.g. it comes from portage, and is
not a `cros_workon`-able package), you need to do the following:

First, find the package ebuild file under `third_party/chromiumos-overlay`.

Then, [create a patch file](portage/how_to_patch_an_ebuild.md) from the exact
version of the package that is used by the current ebuild. If other patches
are already in the ebuild, you'll want to add your patch LAST, and build the
patch off of the source that has already had the existing patches applied
(either do it by hand, or set `FEATURES=noclean` and build your patch off of
the temp source). Note that patch order is significant, since the ebuild
expects each patch line number to be accurate after the previous patch is
applied.

Place your patch in the "files" subdir of the directory that contains the ebuild
file
(e.g. `third_party/chromiumos-overlay/dev-libs/mypackage/files/mypackage-1.0.0-my-little-patch.patch`).

Then, in the `prepare()` section of the ebuild (create one if it doesn't exist),
add an epatch line:

```bash
$ epatch "${FILESDIR}"/${P}-my-little-patch.patch
```

Lastly, you'll need to bump the revision number in the name of the ebuild file
(or symlink) so the build system picks up the change. The current wisdom is that
the ebuild file should be symlinked instead of being renamed. For example, if
the original ebuild file is `"mypackage-1.0.0.ebuild"`, you should create a
`"mypackage-1.0.0-r1.ebuild"` symbolic link that points at the original ebuild
file. If that symlink already exists, create the next higher "rN" symlink.

### Making changes to the way that the chroot is constructed

TODO: This section is currently a placeholder, waiting for someone to fill it
in. However, a few notes:

*   Many of the commands that take a `--board=${BOARD}` parameter also take a
    `--host` parameter, which makes the commands affect the host (i.e. the
    chroot) rather than the board.
    *   Most notably, `cros_workon --host` says that you want to build a package
        used in the chroot from source.

### Building an individual package

TODO: Document this better, and add the new `cros_workon_make`.

**SIDE NOTE:** To build an individual portage package, for a particular board,
use `emerge-${BOARD}`.

For example, if you want to build dash to test on your device:

```bash
(inside)
$ emerge-${BOARD} dash
```

To install the package to the device, see [cros deploy].

**SIDE NOTE:**

*   Typically, when building a package with `emerge-${BOARD}`, the dependencies have
    already been built. However, in some situations dependencies will need to be built
    as well. When that happens, `-jN` can be passed to `emerge-${BOARD}` to build
    different packages in parallel.

### Making changes to the Chromium web browser on ChromiumOS

If you want to make modifications to the Chromium web browser and quickly deploy
your changes to an already-built ChromiumOS image, see [Making changes to the
Chromium web browser on ChromiumOS]. Or if you have a pending Chromium change
that you'd like tested remotely on CrOS's CQ before submitting it, see the
[chromeos-uprev-tester] trybot.

There are other, historical/obsolete ways to build Chromium for ChromiumOS but
they are unsupported and will break in confusing ways.

## Local Debugging

### Debugging both x86 and non-x86 binaries on your workstation

If you build your projects incrementally, write unit tests and use them to drive
your development, you may want to debug your code without shipping it over to a
running device or VM.

`gdb-${BOARD}` sets up gdb in your board sysroot and ensures that gdb is using
the proper libraries, debug files, etc. for debugging, allowing you to run your
target-compiled binaries.

It should already be installed in your chroot. If you do not have the script,
update your repository to get the latest changes, then re-build your packages:

```bash
(in/out)
$ repo sync

(inside)
$ build_packages --board=...
```

This should install `gdb-${BOARD}` in the `/usr/local/bin` directory inside the
chroot. These board-specific gdb wrapper scripts correctly handle _both local
and remote debugging_ (see next section for more information on remote
debugging). When used for local debugging, these scripts will run inside a
special chroot-inside-your-chroot, rooted in the board's sysroot. For example if
you are using `gdb-lumpy`, it will run inside a chroot based entirely in your
`/build/lumpy` sysroot. _The libraries that it will load and use are the
libraries in the sysroot,_ i.e. the target libraries for the board; the gdb
binary it will use is the gdb binary in that tree. While debugging with
`gdb-lumpy` (for local debugging), you will not be able to see/access any files
outside of the `/build/lumpy` tree. While for the most part this is very good,
as it ensures the correct debugging environment, it does mean that if you want
to use this script to debug a lumpy binary, such as a unit test, that you built
outside of the `/build/lumpy` tree, you will need to copy the binary to the
`/build/lumpy` tree first. Also, if you want the debugger to be able to see the
source files when debugging, you will need to make sure they exist inside the
`/build/lumpy` tree as well (see example below).

**IMPORTANT NOTE 1**: Local and remote debugging functionality are combined in
this single script. Some of the options shown below only work for remote
debugging.

**IMPORTANT NOTE 2**: When doing local debugging of x86 binaries, they will try
to execute on your desktop machine (using the appropriate libraries and gdb
binaries). It is possible that for some x86 boards, the binaries may use
instructions not understood by your hardware (particularly some vector
instructions), in which case you will need to do remote debugging with the
actual hardware instead.

**IMPORTANT NOTE 3**: You can use this script with \*some\* debugging
functionality for local debugging of non-x86 binaries. The script loads QEMU and
runs the non-x86 binaries in QEMU. However QEMU has some unfortunate
limitations. For example you can "set" breakpoints in the binary (to see what
addresses correspond to locations in the source), examine the source or assembly
code, and execute the program. But QEMU does not actually hit the breakpoints,
so you cannot suspend execution in the middle when running under QEMU. For full
debugging functionality with non-x86 binaries, you must debug them remotely
running on the correct hardware (see next section on remote debugging). You can
see this in the example below, where gdb-daisy does not actually stop at the
breakpoint it appears to set, although it does correctly execute the program.

```bash
(inside)
(cr) $ gdb-daisy -h

usage: cros_gdb [-h]
                [--log-level {fatal,critical,error,warning,notice,info,debug}]
                [--log_format LOG_FORMAT] [--debug] [--nocolor]
                [--board BOARD] [-g GDB_ARGS] [--remote REMOTE] [--pid PID]
                [--remote_pid PID] [--no-ping] [--attach ATTACH_NAME] [--cgdb]
                [binary-to-be-debugged] [args-for-binary-being-debugged]

Wrapper for running gdb.

This handles the fun details like running against the right sysroot, via
QEMU, bind mounts, etc...

positional arguments:
  inf_args              Arguments for gdb to pass to the program being
                        debugged. These are positional and must come at the
                        end of the command line. This will not work if
                        attaching to an already running program.
...

(cr) $ gdb-daisy /bin/grep shebang /bin/ls
15:51:06: INFO: run: file /build/daisy/bin/grep
Reading symbols from /bin/grep...Reading symbols from /usr/lib/debug/bin/grep.debug...done.
done.
(daisy-gdb) b main
Breakpoint 1 at 0x2814: file grep.c, line 2111.
(daisy-gdb) disass main
Dump of assembler code for function main:
   0x00002814 <+0>: ldr.w r2, [pc, #3408] ; 0x3568 <main+3412>
   0x00002818 <+4>: str.w r4, [sp, #-36]!
   0x0000281c <+8>: movs r4, #0
   0x0000281e <+10>: strd r5, r6, [sp, #4]
   0x00002822 <+14>: ldr.w r3, [pc, #3400] ; 0x356c <main+3416>
   0x00002826 <+18>: movs r5, #2
   0x00002828 <+20>: strd r7, r8, [sp, #12]
...
(daisy-gdb) run
Starting program: /bin/grep shebang /bin/ls
qemu: Unsupported syscall: 26
#!/usr/bin/coreutils --coreutils-prog-shebang=ls
qemu: Unsupported syscall: 26
During startup program exited normally.
(daisy-gdb) quit
```

Note in the example above that, like "regular" gdb when given `--args`, you can
pass the arguments for the program being debugged to the gdb wrapper script just
by adding them to the command line after the name of the program being debugged
(except that `--args` isn't needed).

The commands below show how to copy your incrementally-compiled unit test binary
and source file(s) to the appropriate sysroot and then start gdb with that
binary (using the correct libraries, etc).

```bash
(inside)
(cr) $ cd /build/lumpy/tmp/portage
(cr) $ mkdir shill-test
(cr) $ cd shill-test
(cr) $ cp <path-to-binary>/shill_unittest .
(cr) $ cp <path-to-src>/shill_unittest.cc .
(cr) $ gdb-lumpy
(gdb-lumpy) directory /tmp/portage/shill-test # Tell gdb to add /tmp/portage/shill-test to the paths it searches for source files
(gdb-lumpy) file ./shill_unittest
```

If gdb is still looking for the source file in the wrong directory path, you can
use `set substitute-path <from> <to>` inside gdb to help it find the right path
(inside your sysroot) for searching for source files.

### Printing stack traces at runtime

See [./stack_traces.md] for how to print stack traces at runtime.

## Remote Debugging

### Setting up remote debugging by hand

If you want to manually run through all the steps necessary to set up your
system for remote debugging and start the debugger, see [Remote Debugging in
ChromiumOS].

### Automated remote debugging using gdb-${BOARD} script (gdb-lumpy, gdb-daisy, gdb-parrot, etc)

`gdb-${BOARD}` is a script that automates many of the steps necessary for
setting up remote debugging with gdb. It should already be installed in your
chroot. If you do not have the script, update your repository to get the latest
changes, then re-build your packages:

```bash
(in/out)
$ repo sync

(inside)
$ build_packages --board=...
```

This should install `gdb_remote` in the `/usr/bin` directory inside the
chroot. The `gdb-${BOARD}` script takes several options. The most important ones
are mentioned below.

`--gdb_args` (`-g`) are arguments to be passed to gdb itself (rather than to the
program gdb is debugging). If multiple arguments are passed, each argument
requires a separate -g flag.

E.g `gdb-lumpy --remote=123.45.67.765 -g "-core=/tmp/core" -g "-directory=/tmp/source"`

`--remote` is the ip_address or name for your Chromebook, if you are doing
remote debugging. If you omit this argument, the assumption is you are doing
local debugging in the sysroot on your desktop (see section above). if you are
debugging in the VM, then you need to specify either `:vm:` or `localhost:9222`.

`--pid` is the PID of a running process on the remote device to which you want
gdb/gdbserver to attach.

`--attach` is the name of the running process on the remote device to which you
want gdb/gdbserver to attach. If you want to attach to the Chrome browser
itself, there are three special names you can use: `browser` will attach to the
main browser process; `gpu-process` will attach to the GPU process; and
`renderer` will attach to the renderer process if there is only one. If there is
more than one renderer process `--attach=renderer` will return a list of the
renderer PIDs and stop.

To have gdb/gdbserver start and attach to a new (not already running) binary,
give the name of the binary, followed by any arguments for the binary, at the
end of the command line:

```bash
(inside)
$ gdb-daisy --remote=123.45.67.809 /bin/grep "test" /tmp/myfile
```

When doing remote debugging you \*must\* use the `--pid` or the `--attach`
option, or specify the name of a new binary to start. You cannot start a remote
debugging session without having specified the program to debug in one of these
three ways.

When you invoke `gdb-${BOARD} --remote=...`, it will connect to the notebook or
VM (automatically setting up port-forwarding on the VM), make sure the port is
entered into the iptables, and start up gdbserver, using the correct port and
binary, either attaching to the binary (if a remote PID or name was specified)
or starting up the binary. It will also start the appropriate version of gdb
(for whichever type of board you are debugging) on your desktop and connect the
gdb on your desktop to the gdbserver on the remote device.

### Edit/Debug cycle

If you want to edit code and debug it on the DUT you can follow this procedure

```bash
(inside)
$ CFLAGS="-ggdb" FEATURES="noclean" emerge-${BOARD} -v sys-apps/mosys
$ cros deploy --board=${BOARD} ${IP} sys-apps/mosys
$ gdb-${BOARD} --cgdb --remote "${IP}" \
  -g "--eval-command=directory /build/${BOARD}/tmp/portage/sys-apps/mosys-9999/work/" \
  /usr/sbin/mosys -V
```

This will build your package with debug symbols (assuming your package respects
`CFLAGS`). We need to use the `noclean` feature so that we have access to the
original sourcecode that was used to build the package. Some packages will
generate build artifacts and have different directory structures then the
tar/git repo. This ensures all the paths line up correctly and the source code
can be located. Ideally we would use the `installsources` feature, but we don't
have support for the debugedit package (yet!). Portage by default will strip the
symbols and install the debug symbols in `/usr/lib/debug/`. `gdb-${BOARD}` will
handle setting up the correct debug symbol path. cros deploy will then update
the rootfs on the DUT. We pass the work directory into `gdb-${BOARD}` so that
[cgdb] can display the sourcecode inline.

Quick primer on cgdb:

*   ESC: moves to the source window.
*   i: moves from source window to gdb window.

### Examples of debugging using the gdb-${BOARD} script

Below are three examples of using the board-specific gdb wrapper scripts to
start up debugging sessions. The first two examples show connecting to a remote
Chromebook. The first one automatically finds the browser's running GPU process,
attaches gdbserver to the running process, starts gdb on the desktop, and
connects the local gdb to gdbserver. It also shows the user running the `bt`
(backtrace) command after gdb comes up. The second example shows the user
specifying the PID of a process on the Chromebook. Again the script attaches
gdbserver to the process, starts gdb on the desktop, and connects the two. The
third example shows the user connecting to the main browser process in ChromeOS
running in a VM on the user's desktop. For debugging the VM, you can use either
`--remote=:vm:` or `--remote=localhost:9222` (`:vm:` gets translated into
`localhost:9222`).

Example 1:

```bash
(inside)
$ gdb-lumpy --remote=123.45.67.809 --attach=gpu-process

14:50:07: INFO: run: ping -c 1 -w 20 123.45.67.809
14:50:09: INFO: run: file /build/lumpy/opt/google/chrome/chrome
14:50:10: INFO: run: x86_64-cros-linux-gnu-gdb --quiet '--eval-command=set sysroot /build/lumpy' '--eval-command=set solib-absolute-prefix /build/lumpy' '--eval-command=set solib-search-path /build/lumpy' '--eval-command=set debug-file-directory /build/lumpy/usr/lib/debug' '--eval-command=set prompt (lumpy-gdb) ' '--eval-command=file /build/lumpy/opt/google/chrome/chrome' '--eval-command=target remote localhost:38080'
Reading symbols from /build/lumpy/opt/google/chrome/chrome...Reading symbols from/build/lumpy/usr/lib/debug/opt/google/chrome/chrome.debug...done.
(lumpy-gdb) bt
#0  0x00007f301fad56ad in poll () at ../sysdeps/unix/syscall-template.S:81
#1  0x00007f3020d5787c in g_main_context_poll (priority=2147483647, n_fds=3,   fds=0xdce10719840, timeout=-1, context=0xdce1070ddc0) at gmain.c:3584
#2  g_main_context_iterate (context=context@entry=0xdce1070ddc0,block=block@entry=1, dispatch=dispatch@entry=1, self=<optimized out>)  at gmain.c:3285
#3  0x00007f3020d5798c in g_main_context_iteration (context=0xdce1070ddc0may_block=1) at gmain.c:3351
#4  0x00007f30226a4c1a in base::MessagePumpGlib::Run (this=0xdce10718580, delegate=<optimized out>) at ../../../../../chromeos-cache/distfiles/target/chrome-src-internal/src/base/message_loop/message_pump_glib.cc:309
#5  0x00007f30226666ef in base::RunLoop::Run (this=this@entry=0x7fff72271af0) at ../../../../../chromeos-cache/distfiles/target/chrome-src-internal/src/base/run_loop.cc:55
#6  0x00007f302264e165 in base::MessageLoop::Run (this=this@entry=0x7fff72271db0) at ../../../../../chromeos-cache/distfiles/target/chrome-src-internal/src/base/message_loop/message_loop.cc:307
#7  0x00007f30266bc847 in content::GpuMain (parameters=...) at ../../../../../chromeos-cache/distfiles/target/chrome-src-internal/src/content/gpu/gpu_main.cc:365
#8  0x00007f30225cedee in content::RunNamedProcessTypeMain (process_type=..., main_function_params=..., delegate=0x7fff72272380 at ../../../../../chromeos-cache/distfiles/target/chrome-src-internal/src/content/app/content_main_runner.cc:385
#9  0x00007f30225cef3a in content::ContentMainRunnerImpl::Run (this=0xdce106fef50) at ../../../../../chromeos-cache/distfiles/target/chrome-src-internal/src/content/app/content_main_runner.cc:763
#10 0x00007f30225cd551 in content::ContentMain (params=...) at ../../../../../chromeos-cache/distfiles/target/chrome-src-internal/src/content/app/content_main.cc:19
#11 0x00007f3021fef02a in ChromeMain (argc=21, argv=0x7fff722724b8) at ../../../../../chromeos-cache/distfiles/target/chrome-src-internal/src/chrome/app/chrome_main.cc:66
#12 0x00007f301fa0bf40 in __libc_start_main (main=0x7f3021fee760 <main(int, char const**)>, argc=21, argv=0x7fff722724b8, init=<optimized out>, fini=<optimized out>, rtld_fini=<optimized out>,stack_end=0x7fff722724a8) at libc-start.c:292
#13 0x00007f3021feee95 in _start ()
(lumpy-gdb)
```

Example 2:

```bash
(inside)
$ gdb-daisy --pid=626 --remote=123.45.98.765
14:50:07: INFO: run: ping -c 1 -w 20 123.45.98.765
14:50:09: INFO: run: file /build/daisy/usr/sbin/cryptohomed
14:50:10: INFO: run: armv7a-cros-linux-gnueabi-gdb --quiet '--eval-command=set sysroot /build/daisy' '--eval-command=set solib-absolute-prefix /build/daisy' '--eval-command=set solib-search-path /build/daisy' '--eval-command=set debug-file-directory /build/daisy/usr/lib/debug' '--eval-command=set prompt (daisy-gdb) ' '--eval-command=file /build/daisy/usr/sbin/cryptohomed' '--eval-command=target remote localhost:38080'
Reading symbols from /build/daisy/usr/sbin/cryptohomed...Reading symbols from/build/daisy/usr/lib/debug/usr/bin/cryptohomed.debug...done.
(daisy-gdb)
```

Example 3:

```bash
(inside)
$ gdb-lumpy --remote=:vm: --attach=browser
15:18:28: INFO: run: ping -c 1 -w 20 localhost
15:18:31: INFO: run: file /build/lumpy/opt/google/chrome/chrome
15:18:33: INFO: run: x86_64-cros-linux-gnu-gdb --quiet '--eval-command=setsysroot /build/lumpy' '--eval-command=set solib-absolute-prefix /build/lumpy' '--eval-command=set solib-search-path /build/lumpy' '--eval-command=set debug-file-directory /build/lumpy/usr/lib/debug' '--eval-command=set prompt (lumpy-gdb) ' '--eval-command=file /build/lumpy/opt/google/chrome/chrome' '--eval-command=target remote localhost:48062'
Reading symbols from /build/lumpy/opt/google/chrome/chrome...Reading symbols from /build/lumpy/usr/lib/debug/opt/google/chrome/chrome.debug...done.
done.
Remote debugging using localhost:48062
...
(lumpy-gdb)
```

If you find problems with the board-specific gdb scripts, please file a bug
([crbug.com/new]) and add 'build-toolchain' as one of the labels in the
bug.

## Building Chrome for ChromiumOS

See [Simple Chrome Workflow].

## Troubleshooting

### I lost my developer tools on the stateful partition, can I get them back?

This happens sometimes because the security system likes to wipe out the
stateful partition and a lot of developer tools are in /usr/local/bin. But all
is not lost because there is a tool for updating the stateful partition from an
image created by the auto-update part of the dev_server. Sadly, it is normally
found in /usr/local so will have been lost too and you need to copy it over
manually. This works for me:

```bash
$ cd /tmp
$ scp me@myworkstation:/path/to/chromiumos/chroot/build/x86-whatever/usr/bin/stateful_update .
$ sudo sh stateful_update
$ sudo reboot
```

Note you can clobber the stateful partition (remove user accounts etc and force
OOBE) as part of this process by using a flag:

```bash
$ cd /tmp
$ scp me@myworkstation:/path/to/chromiumos/chroot/build/x86-whatever/usr/bin/stateful_update .
$ sudo sh stateful_update --stateful_change=clean
$ sudo reboot
```

### Disabling Enterprise Enrollment

Some devices may be configured with a policy that only allows logging in with
enterprise credentials, which will prevent you from logging in with a
non-enterprise Google account (e.g., `foo@gmail.com`). To disable the enterprise
enrollment setting:

*   Enable [Developer Mode].
*   Disable the enterprise enrollment check:

    ```bash
    (device)
    $ vpd -i RW_VPD -s check_enrollment=0
    $ dump_vpd_log --force
    $ crossystem clear_tpm_owner_request=1
    $ reboot
    ```

***note
**NOTE:** The enterprise policy can also prevent transitioning to
[Developer Mode], in which case you won't be able to perform the above commands.
***

## Running Tests

ChromiumOS integration (or "functional") tests are written using the [Tast] or
[Autotest] frameworks.

### Set up SSH connection between chroot and DUT

To run automated tests on your [DUT](glossary.md#acronyms), you first need to
set up SSH connection between chroot on your workstation and the DUT. See
[this document](tips-and-tricks.md#How-to-SSH-to-DUT-without-a-password)
for how to set it up.

#### For Googlers

If you are a Google engineer using a corp workstation, you may be required some
extra settings, depending on where your DUT is.

-   DUT at office: Use
    [corp-ssh-helper-helper](https://goto.google.com/corp-ssh-helper-helper).
-   DUT at your home: See [go/arc-wfh](https://goto.google.com/arc-wfh).
-   DUT at lab: See
    [go/chromeos-lab-duts-ssh](https://goto.google.com/chromeos-lab-duts-ssh).

### Tast

[Tast] is a Go-based integration testing framework with a focus on speed,
ease-of-use, and maintainability. Existing Autotest-based tests that run on the
ChromeOS Commit Queue are being ported to Tast and decommissioned as of 2018
Q4. Please strongly consider using Tast when writing new integration tests (but
be aware that not all functionality provided by Autotest is available in Tast;
for example, tests that use multiple devices simultaneously when running are not
currently supported).

Here are some starting points for learning more about Tast:

*   [Tast Quickstart]
*   [Tast: Running Tests]
*   [Tast: Writing Tests]
*   [Tast Overview]

Please contact the public [tast-users mailing list] if you have questions.

### Autotest

[Autotest] is a Python-based integration testing framework; the codebase is also
responsible for managing the [ChromeOS lab] that is used for hardware testing.
ChromiumOS-specific Autotest information is available in the [Autotest User
Documentation].

Additional Autotest documentation:

*   [Creating a new Autotest test]
*   [Running Autotest Smoke Suite On a VM Image]
*   [Seeing which Autotest tests are implemented by an ebuild]

### Creating a normal image that has been modified for test

See [Creating an image that has been modified for test] for information about
modifying a normal system image so that integration tests can be run on it.

### Creating a recovery image that has been modified for test

After building a test image using `build_image test` as described above, you
may wish to encapsulate it within a recovery image:

```bash
(inside)
$ ./mod_image_for_recovery.sh \
    --board=${BOARD} \
    --nominimize_image \
    --image ~/chromiumos/src/build/images/${BOARD}/latest/chromiumos_test_image.bin \
    --to ~/chromiumos/src/build/images/${BOARD}/latest/recovery_test_image.bin
```

If desired, you may specify a custom kernel with `--kernel_image
${RECOVERY_KERNEL}`.

You can write this recovery image out to the USB device like so:

```bash
(inside)
$ cros flash usb:// ~/chromiumos/src/build/images/${BOARD}/latest/recovery_test_image.bin
```

Note that there are some **downsides** to this approach which you should keep in
mind.

*   Your USB image will be a recovery mode/test image, but the ordinary image in
    your directory will be a non-test image.
*   If you use devserver, this will serve the non-test image not the test-image.
*   This means a machine installed with a test-enabled USB image will update to
    a non-test-enabled one.
*   As the `*-generic` boards set `USE=tpm`, recovery images built for
    `*-generic` don't work on devices with H1 chips (which requires `USE=tpm2`).

## Additional information

### Updating the chroot

You should run `update_chroot` after `repo sync`.
`repo sync` only updates the source code, `update_chroot` is required to apply
those changes to the chroot.
`update_chroot` can be run manually, alternatively it is run as part of
`setup_board` and `build_packages`.

```bash
(inside)
$ ./update_chroot
```

### Documentation on this site

You now understand the basics of building, running, modifying, and testing
ChromiumOS, but you've still got a lot to learn. Here are links to a few other
pages on the chromium.org site that you are likely to find helpful (somewhat
ordered by relevance):

*   The [Tips And Tricks] page has a lot of useful
    information to help you optimize your workflow.
*   If you haven't read the page about the  [devserver]  already, read it now.
*   Learn about the ChromiumOS [directory structure].
*   [The ChromiumOS developer FAQ]
*   [ChromiumOS Portage Build FAQ] is useful for portage-specific questions.
*   If you have questions about the `--no-enable-rootfs-verification` option,
    you might find answers on this [thread on chromium-os-dev][rootfs-thread].
*   [Running Smoke Suite on a VM Image] has good information about how to get up
    and running with autotest tests and VMs.
*   [Debugging Tips] contains information about how to debug the Chromium
    browser on your ChromiumOS device.
*   [Working on a Branch] has tips for working on branches.
*   [Git server-side information] talks about adding new git repositories.
*   The [Portage Package Upgrade Process] documents how our Portage
    packages can be upgraded when they fall out of date with respect to
    upstream.
*   The [ChromiumOS Sandboxing] page describes the mechanisms and tools used to
    sandbox (or otherwise restrict the privileges of) ChromiumOS system
    services.
*   The [Go in ChromiumOS] page provides information on using [Go] in
    ChromiumOS, including recommendations for project organization, importing
    and managing third party packages, and writing ebuilds.
*   The [SELinux](security/selinux.md) page provides information on SELinux in Chrome
    OS, including overview, writing policies, and troubleshooting.
*   The [Running a single binary with UBSAN](testing/single-binary-ubsan.md)
    page gives tips for using UBSAN (undefined behavior sanitizer) to find bugs
    in a binary. Instructions can also be used for ASAN (address sanitizer) and
    TSAN (thread sanitizer).

### External documentation

Below are a few links to external sites that you might also find helpful
(somewhat ordered by relevance):

*   **Definitely** look at the [ChromiumOS dev group] to see what people are
    talking about.
*   Check out the [Chromium bug tracker] to report bugs, look for known
    issues, or look for things to fix.
*   Get an idea of what's actively happening by looking at the [Chromium Gerrit]
    site. (Note that this is no longer the same site that Chromium uses)
*   Browse the source code on the [ChromiumOS gitweb].
*   Check the current status of the builds by looking at the
    [ChromiumOS build waterfall].
*   Check out the #chromium-os channel on [libera.chat] (some ChromiumOS
    developers hang out on this IRC channel).
*   If you're learning git, check out [Git for Computer Scientists],
    [Git Magic], or the [Git Manual].
*   Gentoo has several useful documentation resources
    *   [Gentoo Development Guide] is a useful resource for Portage, which is
        the underlying packaging system we use.
    *   [Gentoo Embedded Handbook]
    *   [Gentoo Cross Development Guide]
    *   [Gentoo Wiki on Cross-Compiling]
    *   [Gentoo Package Manager Specification]
*   The  [repo user docs]  might help you if you're curious about repo.
*   The  [repo-discuss group]  is a good place to talk about repo.


[README.md]: README.md
[Prerequisites]: #Prerequisites
[Getting the source code]: #Get-the-Source
[Building ChromiumOS]: #Building-ChromiumOS
[Installing ChromiumOS on your Device]: #Installing-ChromiumOS-on-your-Device
[Making changes to packages whose source code is checked into ChromiumOS git repositories]: #Making-changes-to-packages-whose-source-code-is-checked-into-ChromiumOS-git-repositories
[Making changes to non-cros_workon-able packages]: #Making-changes-to-non_cros_workon_able-packages
[Local Debugging]: #Local-Debugging
[Remote Debugging]: #Remote-Debugging
[Troubleshooting]: #Troubleshooting
[Running Tests]: #Running-Tests
[Additional information]: #Additional-information
[Attribution requirements]: #Attribution-requirements
[Ubuntu]: https://www.ubuntu.com/
[RAM-thread]: https://groups.google.com/a/chromium.org/d/topic/chromium-os-dev/ZcbP-33Smiw/discussion
[install depot_tools]: https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up
[Sync to Green]: #Sync-to-Green
[Making sudo a little more permissive]: tips-and-tricks.md#How-to-make-sudo-a-little-more-permissive
[Gerrit guide]: https://www.chromium.org/chromium-os/developer-guide/gerrit-guide
[repo]: https://code.google.com/p/git-repo/
[git]: https://git-scm.com/
[goto/chromeos-building]: http://goto/chromeos-building
[API Keys]: https://www.chromium.org/developers/how-tos/api-keys
[working on a branch page]: work_on_branch.md
[chroot]: https://en.wikipedia.org/wiki/Chroot
[gsutil]: gsutil.md
[crosbug/10048]: https://crbug.com/192478
[Tips And Tricks]: tips-and-tricks.md
[something harder]: https://www.google.com/search?q=the+fourth+dimension
[issues with virtual packages]: https://crbug.com/187712
[What does build_packages actually do?]: portage/ebuild_faq.md#what-does-build-packages-do
[CrOS Flash page]: cros_flash.md
[Debug Button Shortcuts]: debug_buttons.md
[ChromeOS Devices]: https://dev.chromium.org/chromium-os/developer-information-for-chrome-os-devices
[Developer Hardware]: https://dev.chromium.org/chromium-os/getting-dev-hardware/dev-hardware-list
[crosh]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/crosh/
[cros_vm]: cros_vm.md#launch-a-locally-built-vm-from-within-the-chroot
[cros deploy]: cros_deploy.md
[Create a branch for your changes]: #Create-a-branch-for-your-changes
[Making changes to the Chromium web browser on ChromiumOS]: simple_chrome_workflow.md
[chromeos-uprev-tester]: simple_chrome_workflow.md#testing-a-chromium-cl-remotely-on-cros-cq
[Remote Debugging in ChromiumOS]: https://www.chromium.org/chromium-os/how-tos-and-troubleshooting/remote-debugging
[cgdb]: https://cgdb.github.io/
[crbug.com/new]: https://crbug.com/new
[Simple Chrome Workflow]: simple_chrome_workflow.md
[Tast]: https://chromium.googlesource.com/chromiumos/platform/tast/
[Autotest]: https://autotest.github.io/
[Tast Quickstart]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/quickstart.md
[Tast: Running Tests]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/running_tests.md
[Tast: Writing Tests]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/writing_tests.md
[Tast Overview]: https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/overview.md
[tast-users mailing list]: https://groups.google.com/a/chromium.org/forum/#!forum/tast-users
[ChromeOS lab]: http://sites/chromeos/for-team-members/lab/lab-faq
[Autotest User Documentation]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/HEAD/docs/user-doc.md
[Creating a new Autotest test]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/HEAD/docs/user-doc.md#Writing-and-developing-tests
[Running Autotest Smoke Suite On a VM Image]: https://dev.chromium.org/chromium-os/testing/running-smoke-suite-on-a-vm-image
[Seeing which Autotest tests are implemented by an ebuild]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/HEAD/docs/user-doc.md#Q4_I-have-an-ebuild_what-tests-does-it-build
[Creating an image that has been modified for test]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/HEAD/docs/user-doc.md#W4_Create-and-run-a-test_enabled-image-on-your-device
[devserver]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/docs/devserver.md
[directory structure]: source_layout.md
[The ChromiumOS developer FAQ]: https://dev.chromium.org/chromium-os/how-tos-and-troubleshooting/developer-faq
[ChromiumOS Portage Build FAQ]: portage/ebuild_faq.md
[rootfs-thread]: https://groups.google.com/a/chromium.org/group/chromium-os-dev/browse_thread/thread/967e783e27dd3a9d/0fa20a1547de2c77?lnk=gst
[Running Smoke Suite on a VM Image]: https://dev.chromium.org/chromium-os/testing/running-smoke-suite-on-a-vm-image
[Debugging Tips]: https://dev.chromium.org/chromium-os/how-tos-and-troubleshooting/debugging-tips
[Working on a Branch]: work_on_branch.md
[Git server-side information]: https://dev.chromium.org/chromium-os/how-tos-and-troubleshooting/git-server-side-information
[Portage Package Upgrade Process]: portage/package_upgrade_process.md
[ChromiumOS Sandboxing]: sandboxing.md
[Go in ChromiumOS]: https://dev.chromium.org/chromium-os/developer-guide/go-in-chromium-os
[Go]: https://golang.org
[ChromiumOS dev group]: https://groups.google.com/a/chromium.org/group/chromium-os-dev
[Chromium bug tracker]: https://crbug.com/
[Chromium Gerrit]: https://chromium-review.googlesource.com/
[ChromiumOS gitweb]: https://chromium.googlesource.com/
[ChromiumOS build waterfall]: https://ci.chromium.org/p/chromeos
[libera.chat]: https://web.libera.chat/
[Gerrit Workflow]: contributing.md
[Git for Computer Scientists]: https://eagain.net/articles/git-for-computer-scientists/
[Git Magic]: http://www-cs-students.stanford.edu/~blynn/gitmagic/
[Git Manual]: http://schacon.github.com/git/user-manual.html
[Gentoo Development Guide]: https://devmanual.gentoo.org/
[Gentoo Embedded Handbook]: https://wiki.gentoo.org/wiki/Embedded_Handbook
[Gentoo Cross Development Guide]: https://wiki.gentoo.org/wiki/Embedded_Handbook
[Gentoo Wiki on Cross-Compiling]: https://wiki.gentoo.org/wiki/Crossdev
[Gentoo Package Manager Specification]: https://ci.chromium.org/p/chromeos
[repo user docs]: https://source.android.com/source/using-repo
[repo-discuss group]: https://groups.google.com/group/repo-discuss
[Developer Mode]: ./developer_mode.md
[./stack_traces.md]: stack_traces.md
[build codelab]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/codelab/README.md
