---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: simple-chrome-workflow
title: Building Chrome for ChromeOS (Simple Chrome)
---

This workflow allows you to quickly build/deploy Chrome to a ChromeOS
[VM] or device without needing a ChromeOS source checkout or chroot. It's useful
for trying out your changes on ChromeOS while you're doing Chrome
development. If you have an OS checkout and want your local Chrome changes to
be included when building a full OS image, see
[below](#building-ash-chrome-in-the-chroot).

At its core is the `chrome-sdk` shell which sets up the shell environment and
fetches the necessary SDK components (ChromeOS toolchain, sysroot, VM, etc.).

[TOC]

## Typography conventions

| Label         | Paths, files, and commands                            |
|---------------|-------------------------------------------------------|
|  (shell)      | outside the chroot and SDK shell on your workstation  |
|  (sdk)        | inside the `chrome-sdk` SDK shell on your workstation |
|  (chroot)     | inside the `cros_sdk` chroot on your workstation      |
|  (device)     | in your [VM] or ChromeOS device                        |


## Getting started

Check out a copy of the [Chrome source code and depot_tools].

> **Important:** Be certain to [update .gclient] to include
> `target_os = ["chromeos"]`.

### Get the Google API keys

In order to sign in to ChromeOS you must have Google API keys:

*   External contributors: See [api-keys]. You'll need to put them in your
    `out_$BOARD/Release/args.gn file`, see below.
*   *Googlers*: See [chrome build instructions] to get the internal source.
    If you have `src-internal` in your `.gclient` file the official API keys
    will be set up automatically.

### Set up gsutil

Make sure you have credentials to access Google Storage bucket by setting up
your `~/.boto` file. See the [gsutil setup documentation](https://www.chromium.org/chromium-os/developer-library/reference/tools/gsutil/) for
more details.

### Install additional build dependencies

Run the [install-build-deps.py] script in Chrome's source code, located at
`$CHROME_DIR/src/build/install-build-deps.py`. This will install other packages
that you will need to build and run binaries.

### VM versus Device

The easiest way to develop on ChromeOS is to use a [VM].

If you need to test hardware-specific features such as graphics acceleration,
bluetooth, mouse or input events, etc, you may also use a physical device
(Googlers: Chromestop has the hardware). See [Set up the ChromeOS device] for
details.

---

## Enter the Simple Chrome environment

There are two ways to use Simple Chrome: 1) shell-less flow
(recommended) and 2) traditional flow.

The shell-less flow is currently in use by all of [Chrome's builders], so
there's some guarantee that it will function correctly. Conversely, the
traditional flow that uses the shell has no such continuous build coverage and
is mostly community-supported. In practice, however, the two flows are mostly
identical under the hood. So if one works, the other is likely to as well.

The shell-less flow currently [does not handle custom
toolchains](http://b//187793436) thus you should use the traditional flow
if you need custom toolchains.

### Shell-less flow

The shell-less flow conforms more closely to how the browser is built for
its other supported platforms. In this workflow, the `cros chrome-sdk` is
never called directly by hand, but instead called once during `gclient
sync`.

To do this, simply add the board you're interested in to the `cros_boards`
(device) or `cros_boards_with_qemu_images` (VM) [custom gclient var] of your
`.gclient` file. For Googlers, be sure to include
`"checkout_src_internal": True` and set up [reclient][reclient-googlers]
to speed up builds.

```
solutions = [
  {
    "name": "src",
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "custom_deps": {},
    "custom_vars": {
        "checkout_src_internal": True,
        "cros_boards": "<board name>",
    },
  },
]
target_os = ["chromeos"]
```

The board's SDK will then be downloaded (and cached) everytime
you run `gclient sync`, which should be run after every update to your chromium
checkout.  You may specify additional boards in the `cros_boards` variable by
appending `:<board>` to the variable. For example, `"cros_boards":
"amd64-generic:betty-pi-arc"` includes the `amd64-generic` and `betty-pi-arc`
boards. Similar to the traditional flow, this will also create a convenient build dir at
`out_$BOARD/Release` ($BOARD refers to the board name) for each board listed in
your `.gclient` file, with `args.gn` file that contains a line that looks like
this (if your board is `hatch`):

```
import("//build/args/chromeos/hatch.gni")
```

This is required to build Chrome for the board correctly. If you don't
need to customize GN args, you can simply generate GN files by:

```
(shell) gn gen out_$BOARD/Release
```

If you want to customize GN args (e.g. adding `is_chrome_branded=true`), make
sure that you keep the import statement like this (if your board is
`hatch`):

```
(shell) gn gen out_hatch/Release --args='import("//build/args/chromeos/hatch.gni") is_chrome_branded=true'
```

> **Note:** Unbranded Chrome [applies experimental field trial
> flags](https://www.chromium.org/developers/gn-build-configuration/#official-chrome-build)
> by default.

You can also use `gn args out_hatch/Release` to edit your GN flags with a
text editor. See also [ChromeOS Build Instructions] for other useful GN flags.

To compile Chrome:

```
(shell) autoninja -C out_$BOARD/Release chrome
```

Unlike the traditional shell flow, any tool or script you would run should
include the full path inside chromite.
For example, a `deploy_chrome` invocation in the shell-less flow looks like:

```
(shell) ./third_party/chromite/bin/deploy_chrome --build-dir=out_${BOARD}/Release --device=$IP_ADDR
```

### Traditional flow

Building Chrome for ChromeOS requires a toolchain customized for each
Chromebook model (or "board"). For the ChromeOS [VM], and non-Googlers, use
`amd64-generic`. For a physical device, look up the [ChromeOS board name] by
navigating to the URL `about:version` on the device. For example:
`Platform 10176.47.0 (Official Build) beta-channel samus` has board `samus`.

To enter the Simple Chrome environment, run these from within your Chrome
checkout:

```
(shell) cd /path/to/chrome/src
(shell) export BOARD=amd64-generic
(shell) cros chrome-sdk --board=$BOARD --log-level=info [--download-vm]
```

The command prompt will change to look like `(sdk $BOARD $VERSION)`.

Entering the Simple Chrome environment does the following:

1.  Fetches the ChromeOS toolchain and sysroot (SDK) for building Chrome.
1.  Creates out_$BOARD/Release and generates or updates args.gn.
1.  Adds `use_remoteexec` to the gn args which builds Chrome with [reclient].
    reclient can be disabled with `--no-use-remoteexec`.
    [Additional setup may be required](https://chromium.googlesource.com/chromium/src/+/main/docs/linux/build_instructions.md#use-reclient).
1.  `--download-vm` will download a ChromeOS VM and a QEMU binary.

### cros chrome-sdk options

*   `--chrome-branding` Sets up Simple Chrome to build and deploy the internal *Chrome* instead of *Chromium*.
*   `--official` Enables the official build level of optimization.
*   `--gn-extra-args='extra_arg=foo other_extra_arg=bar'` For setting
    extra gn args, e.g. 'dcheck_always_on=true'.
*   `--log-level=info` Sets the log level to 'info' or 'debug' (default is
    'warn').
*   `--nogn-gen` Do not run 'gn gen' automatically. Use this option to persist
    changes made to a previous session's gn args.

> **Googlers**: Use `--chrome-branding` if you need a branded *Chrome* build including resources and components from src-internal to work on internal features like ARC and assistant. `--official` doesn't involve branding, instead it enables an additional level of optimization and removes development conveniences like runtime stack traces. Use it for performance testing, not for debugging.

**ChromeOS developers**

Use the following command:
```
(shell) cros chrome-sdk --chrome-branding --board=$BOARD --log-level=info
```

*Optional*: Please help development by setting `dcheck_always_on=true` and filing
bugs if you encounter any DCHECK crashes:
```
(shell) cros chrome-sdk --chrome-branding --board=$BOARD --log-level=info --gn-extra-args='dcheck_always_on=true'
```
Alternatively, you can set `dcheck_is_configurable=true` to log DCHECK errors without crashing.

### cros chrome-sdk tips

> **Important:** When you sync/update your Chrome source, the ChromeOS SDK
> version (src/chromeos/CHROMEOS_LKGM) may change. When the SDK version changes
> you may need to exit and re-enter the Simple Chrome environment to
> successfully build and deploy Chrome.

> **Non-Googlers**: Only generic boards have publicly available SDK downloads,
> so you will need to use a generic board (e.g. amd64-generic) or your own
> ChromeOS build (see [Using a custom ChromeOS build]). For more info and
> updates star [crbug.com/360342].

> **Note**: See also [Using a custom ChromeOS build].

---

## Build Chrome

To build Chrome, `cd` into `src` subdirectory of `chromium` checkout and run:

```
(sdk) autoninja -C out_${SDK_BOARD}/Release chrome nacl_helper
```

> **Note**: Targets other than **chrome**, **nacl_helper** or
> (optionally) **chromiumos_preflight** are not supported in Simple Chrome and
> will likely fail. browser_tests should be run outside the Simple Chrome
> environment. Some unit_tests may be built in the Simple Chrome environment and
> run in the ChromeOS VM. For details, see
> [Running a Chrome Google Test binary in the VM].

> **Note:** The default extensions will be installed by the test image you use
> below.

---

## Set up the ChromeOS device

### VM

If you are planning on using VM, start it:

```
(sdk) cros_vm --start
```

You can then connect to it via SSH (use `test0000` as a password):

```
(sdk) ssh -p 9222 root@localhost
```

or via VNC to `localhost:5900`.

Unless you plan to also setup a device, you can skip the rest of this section.

### Getting started

You need the following:
1.  USB flash drive 4 GB or larger (for example, a Sandisk Extreme USB 3.0)
1.  USB to Gigabit Ethernet adapter

Before you can deploy your build of Chrome to the device, it needs to have a
"test" OS image loaded on it. A test image has tools like rsync that are not
part of the base image.

Chrome should be deployed to a recent ChromeOS test image, ideally the
version shown in your SDK prompt (or `(sdk) echo $SDK_VERSION`).

### Create a bootable USB stick

**Non-Googlers**: The build infrastructure is currently in flux. See
[crbug.com/360342] for more details. You may need to build your own ChromeOS
image.

Flash the latest canary test image to your USB stick using `cros flash`:

```
(sdk) cros flash usb:// xbuddy://remote/${SDK_BOARD}/latest-canary
```

You can also flash an image with the sdk version (the SDK prompt has the
full version, for instance, R81-12750.0.0):

```
(sdk) cros flash usb:// xbuddy://remote/${SDK_BOARD}/R81-12750.0.0
```

> **Tip:** If the device already has a test image installed, the following
can be used to update the device directly.

```
(sdk) $ cros flash $IP_ADDR xbuddy://remote/${SDK_BOARD}/latest-canary
```

See the [CrOS Flash page] for more details.

### Put your ChromeOS device in dev mode

> **Note:** Switching to dev mode wipes all data from the device (for security
> reasons).

Most recent devices can use the [generic instructions]. To summarize:

1.  With the device on, hit Esc + Refresh (F2 or F3) + power button
1.  Wait for the white "recovery screen"
1.  Hit Ctrl-D to switch to developer mode (there's no prompt)
1.  Press enter to confirm
1.  Once it is done, hit Ctrl-D again to boot, then wait

From this point on you'll always see the white screen when you turn on
the device. Press Ctrl-D to boot.

Older devices may have [device-specific instructions].

*Googlers*: If the device asks you to "enterprise enroll", click the X in the
top-right of the dialog to skip it. Trying to use your google.com credentials
will result in an error.

### Enable booting from USB

By default Chromebooks will not boot off a USB stick for security reasons.
You need to enable it.

1.  Start the device
1.  Press Ctrl-Alt-F2 to get a terminal. (You can use Ctrl-Alt-F1 to switch
    back if you need to.)
1.  Login as `root` (no password yet, there will be one later)
1.  Run `enable_dev_usb_boot`

### Install the test image onto your device

> **Note:** Do not log into this test image with a username and password you
> care about. The root password is public ("test0000"), so anyone with SSH
> access could compromise the device. Create a test Gmail account and use that.

1.  Plug the USB stick into the machine and reboot.
1.  At the dev-mode warning screen, press Ctrl-U to boot from the USB stick.
1.  Switch to terminal by pressing Ctrl-Alt-F2
1.  Login as user `chronos`, password `test0000`.
1.  Run `/usr/sbin/chromeos-install`
1.  Wait for it to copy the image
1.  Run `poweroff`

You can now unplug the USB stick.

### Connect device to Ethernet

Use your USB-to-Ethernet adapter to connect the device to a network.

*Googlers*: If your building has Ethernet jacks connected to the test VLAN
(e.g. white ports), use one of those jacks. Otherwise get a second Ethernet
adapter and see [go/shortleash] to reverse tether your Chromebook to your
workstation.

### Checking the IP address

1.  Click the status area in the lower-right corner
1.  Click the network icon
1.  Click the circled `i` symbol in the lower-right corner
1.  A small window pops up that shows the IP address

You can also run `ifconfig` from the terminal (Ctrl-Alt-F2).

---

## Deploying Chrome to the device

To deploy the build to a device/VM, you will need direct SSH access to it from
your computer. The scripts below handle everything else.

### Using deploy_chrome

The `deploy_chrome` script uses rsync to incrementally deploy Chrome to the
device/VM.

Specify the build output directory to deploy from using `--build-dir`. For the
[VM]:

```
(sdk) deploy_chrome --build-dir=out_${SDK_BOARD}/Release --device=localhost:9222
```

For a physical device, which must be ssh-able as user 'root', you must specify
the IP address using `--device`:

```
(sdk) deploy_chrome --build-dir=out_${SDK_BOARD}/Release --device=$IP_ADDR
```

> **Note:** The first time you run this you will be prompted to remove rootfs
> verification from the device. This is required to overwrite /opt/google/chrome
> and will reboot the device. You can skip the prompt with `--force`.

### Deploying Chrome to the user partition

It is also possible to deploy Chrome to the user partition of the device and
set up a temporary mount from `/opt/google/chrome` using the option `--mount`.
This is useful when deploying a binary that will not otherwise fit on the
device, e.g.:

*   When using `--nostrip` to provide symbols for backtraces.
*   When using other compile options that produce a significantly larger image.

```
(sdk) deploy_chrome --build-dir=out_$SDK_BOARD/Release --device=$IP_ADDR --mount [--nostrip]
```

> **Note:** This also prompts to remove rootfs verification so that
> /etc/chrome_dev.conf can be modified (see [Command-line flags and
> environment variables]). You can skip that by adding
> `--noremove-rootfs-verification`.

### Additional Notes:

*   The mount is transient and does not survive a reboot. The easiest way to
    reinstate the mount is to run the same deploy_chrome command after reboot.
    It will only redeploy binaries if there is a change. To verify that the
    mount is active, run `findmnt /opt/google/chrome`. The output should be:
```
TARGET             SOURCE                                      FSTYPE OPTIONS
/opt/google/chrome /dev/sda1[/deploy_rootfs/opt/google/chrome] ext4   rw,nodev,noatime,resgid=20119,commit=600,data=ordered
```
*   If startup needs to be tested (i.e. before deploy_chrome can be run), a
    symbolic link will need to be created instead:
    *   ssh to device
        *   `mkdir /usr/local/chrome`
        *   `rm -R /opt/google/chrome`
        *   `ln -s /usr/local/chrome /opt/google/chrome`
     *   `deploy_chrome --build-dir=out_${SDK_BOARD}/Release --device=$IP_ADDR
         --nostrip`
     *   The device can then be rebooted and the unstripped version of Chrome
         will be run.
*   `deploy_chrome` lives under `$CHROME_DIR/src/third_party/chromite/bin`.
    You can run `deploy_chrome` outside of a `chrome-sdk` shell.
*   You can automatically log back in after deploy by specfiying
    `--unlock-password=<password>`. If provided, `deploy_chrome` will wait for
    the unlock screen and input the specified password.

## Updating the ChromeOS image

In order to keep Chrome and ChromeOS in sync, the ChromeOS test image
should be updated weekly. See [Create a bootable USB stick] for a tip on
updating an existing test device if you have a ChromeOS checkout.

---

## Debugging

### Log files

[Chrome-related logs] are written to several locations on the device running a
test image:

*   `/var/log/ui/ui.LATEST` contains messages written to stderr by Chrome
    before its log file has been initialized.
*   `/var/log/chrome/chrome` contains messages logged by Chrome both before and
    after login since Chrome runs with `--disable-logging-redirect` on test
    images.
*   `/var/log/messages` contains messages logged by `session_manager`
    (which is responsible for starting Chrome), in addition to kernel
    messages when a Chrome process crashes.

### Command-line flags and environment variables

If you want to tweak the command line of Chrome or its environment, you have to
do this on the device itself.

Edit the `/etc/chrome_dev.conf` (device) file. Instructions on using it are in
the file itself.

### Custom build directories

This step is only necessary if you run `cros chrome-sdk` with `--nogn-gen`.

To create a GN build directory, run the following inside the chrome-sdk shell:

```
(sdk) gn gen out_$SDK_BOARD/Release --args="$GN_ARGS"
```

This will generate `out_$SDK_BOARD/Release/args.gn`.

*   You must specify `--args`, otherwise your build will not work on the device.
*   You only need to run `gn gen` once within the same `cros chrome-sdk`
    session.
*   However, if you exit the session or sync/update Chrome the `$GN_ARGS` might
    change and you need to `gn gen` again.

You can edit the args with:

```
(sdk) gn args out_$SDK_BOARD/Release
```

You can replace `Release` with `Debug` (or something else) for different
configurations. See [Debug builds].

[GN build configuration] discusses various GN build configurations. For more
info on GN, run `gn help` on the command line or read the [quick start guide].

### Debug builds

For cros chrome-sdk GN configurations, Release is the default. A debug build of
Chrome will include useful tools like DCHECK and debug logs like DVLOG. For a
Debug configuration, specify
`--args="$GN_ARGS is_debug=true is_component_build=false"`.

Alternately, you can just turn on DCHECKs for a release build. You can do this
with `--args="$GN_ARGS dcheck_always_on=true"`.

To deploy a debug build you need to add `--nostrip` to `deploy_chrome` because
otherwise it will strip symbols even from a debug build. This requires
[Deploying Chrome to the user partition].

See [Stack Traces] for some tips on getting stack traces at runtime
(not during a crash).

> **Note:** If you just want crash backtraces in the logs you can deploy a
> release build with `--nostrip`. You don't need a debug build (but you still
> need to deploy to a user partition).
>
> **Note:** You may hit `DCHECKs` during startup time, or when you login, which
> eventually may reboot the device. You can check log files in `/var/log/chrome`
> or `/home/chronos/user/log`.
>
> You can create `/run/disable_chrome_restart` to prevent a restart loop and
> investigate.
>
> You can temporarily disable these `DCHECKs` to proceed, but please file a
> bug for such `DCHECK` because it's most likely a bug.

### Remote GDB

> **Note:** You want `symbol_level=2` in your gn args to get everything (line
> numbers, function names, local variables, etc). Assuming you use `cros_gdb`
> you do *not* need `--nostrip` when deploying since gdb reads symbols from the
> local copy.

The `cros_gdb` wrapper automates most of the setup to get `gdb` attached to
Chrome and running.
Assuming you're in the chromium src folder, `$TARGET` is the hostname or ip:port
of your device and `$BOARD` is the target board:

**Shell-less workflow**:
`./third_party/chromite/bin/cros_gdb --board $BOARD --remote $TARGET --attach
browser -g="--eval-command=cd $(pwd)/out_$BOARD/Release"`

**Simple-chrome workflow**:
`(sdk) cros_gdb --board $BOARD --remote $TARGET --attach browser`
(inside the chrome-sdk shell, you may need to adjust the path to find sources).

`cros_gdb --help` to see more options (e.g. how to attach to a renderer process
instead of the main browser process).

#### Manual steps (no cros_gdb)

If you don't want to use cros_gdb (perhaps you want to use a different version
of gdb) you can set things up manually.

Core dumps are disabled by default. See [additional debugging tips] for how to
enable core files.

On the target machine, open up a port for the gdb server to listen on, and
attach the gdb server to the top-level Chrome process.

```
(device) sudo /sbin/iptables -A INPUT -p tcp --dport 1234 -j ACCEPT
(device) sudo gdbserver --attach :1234 $(pgrep chrome -P $(pgrep session_manager))
```

On your host machine (inside the chrome-sdk shell), run gdb and start the Python
interpreter:

```
(sdk) cd %CHROME_DIR%/src
(sdk) gdb out_${SDK_BOARD}/Release/chrome
Reading symbols from /usr/local/google2/chromium2/src/out_amd64-generic/Release/chrome...
(gdb) pi
>>>
```

> **Note:** These instructions are for targeting an x86_64 device. For now, to
> target an ARM device, you need to run the cross-compiled gdb from within a
> chroot.

Then from within the Python interpreter, run these commands:

```python
import os
sysroot = os.environ['SYSROOT']
board = os.environ['SDK_BOARD']
gdb.execute('set sysroot %s' % sysroot)
gdb.execute('set solib-absolute-prefix %s' % sysroot)
gdb.execute('set debug-file-directory %s/usr/lib/debug' % sysroot)
# "Debug" for a debug build
gdb.execute('set solib-search-path out_%s/Release/lib' % board)
gdb.execute('target remote $IP_ADDR:1234')
```

If you wish, after you connect, you can Ctrl-D out of the Python shell.

Extra debugging instructions are located at [debugging tips].

---

## Additional instructions

### Updating the version of the ChromeOS SDK

When you invoke `cros chrome-sdk`, the script fetches the version of the SDK
that corresponds to your Chrome checkout. To update the SDK, sync your Chrome
checkout and re-run `cros chrome-sdk`.

**IMPORTANT NOTES:**

*   Every time that you update Chrome or the ChromeOS SDK, it is possible
    that Chrome may start depending on new features from a new ChromeOS
    image. This can cause unexpected problems, so it is important to update
    your image regularly. Instructions for updating your ChromeOS image are
    above in [Set up the ChromeOS device]. This is not a concern for a
    downloaded VM.
*   Don't forget to re-configure your custom build directories if you have them
    (see [Custom build directories]).

### Specifying the version of the ChromeOS SDK to use

You can specify a version of ChromeOS to build against. This is handy for
tracking down when a particular bug was introduced.

```
(shell) cros chrome-sdk --board=$BOARD --version=11005.0.0
```

Once you are finished testing the old version of the chrome-sdk, you can
always start a new shell with the latest version again. Here's an example:

```
(shell) cros chrome-sdk --board=$BOARD --clear-sdk-cache
```

### Updating Chrome

```
(sdk) exit
(shell) git checkout main && git pull   # (or if you prefer, git rebase-update)
(shell) gclient sync
(shell) cros chrome-sdk --board=$BOARD --log-level=info
```

> **Tip:** If you update Chrome inside the chrome-sdk, you may then be using an
> SDK that is out of date with the current Chrome.
> See [Updating the version of the ChromeOS SDK] section above.


### Updating Deployed Files

`deploy_chrome` determines which files to copy in `chrome_util.py` in the
[chromite repo] which is pulled into `chrome/src/third_party/chromite` via DEPS.

When updating the list:

1.  Make changes to the appropriate list (e.g. `_COPY_PATHS_CHROME`).
1.  Be aware that deploy_chrome is used by the chromeos-chrome ebuild, so when
    adding new files make sure to set optional=True initially.
1.  Changes to chromite will not affect Simple Chrome until a chromite roll
    occurs.

### Using a custom ChromeOS build

If you are making changes to ChromeOS and have a ChromeOS build inside a
chroot that you want to build against, run `cros chrome-sdk` with the `--chroot`
option:

```
(shell) cros chrome-sdk --board=$BOARD --chroot=/path/to/chromiumos/chroot
```

### Running tests

Chrome's unit and browser tests are compiled into test binaries. At the moment,
not all of them run on a ChromeOS device. Most of the unit tests and part of
interactive_ui_tests that measure ChromeOS performance should work.

To build and run a chrome test on device (or VM),
```bash
(sdk) .../chrome/src $ cros_run_test --build --device=$IP --chrome-test -- \
out_$SDK_BOARD/Release/interactive_ui_tests \
    --dbus-stub \
    --enable-pixel-output-in-tests \
    --gtest_filter=SplitViewTest.SplitViewResize
```

Alternatively, manually build and use the generated `run_$TEST` scripts to run
like build bots:
```bash
(sdk) .../chrome/src $ autoninja -C out_$SDK_BOARD/Release interactive_ui_tests
(sdk) .../chrome/src $ out_$SDK_BOARD/Release/bin/run_interactive_ui_tests \
    --device=$IP \
    --dbus-stub \
    --enable-pixel-output-in-tests \
    --gtest_filter=SplitViewTest.SplitViewResize
```

To run tests locally on dev box, follow the [instructions for running tests on
Linux] using a separate GN build directory with `target_os = "chromeos"` in its
arguments. (You can create one using the `gn args` command.)

If you're running tests which create windows on-screen, you might find the
instructions for using an embedded X server in [web_tests_linux.md] useful.

### Setting a custom prompt

By default, cros chrome-sdk prepends something like '`(sdk link R52-8315.0.0)`'
to the prompt (with the version of the prebuilt system being used).

If you prefer to colorize the prompt, you can set `PS1` in
`~/.chromite/chrome_sdk.bashrc`, e.g. to prepend a yellow
'`(sdk link 8315.0.0)`' to the prompt:

```
PS1='\[\033[01;33m\](sdk ${SDK_BOARD} ${SDK_VERSION})\[\033[00m\] \w \[\033[01;36m\]$(__git_ps1 "(%s)")\[\033[00m\] \$ '
```
NOTE: Currently the release version (e.g. 52) is not available as an
environment variable.


### Googlers: Prebuilt minidump_stackwalk and other Breakpad tools

**Googlers**: If you work with minidumps and need tools such as
minidump_stackwalk, minidump_dump, and so on, you can install them
on you workstation:

```
(shell) sudo glinux-add-repo breakpad
(shell) sudo apt update
(shell) sudo apt-get install breakpad
```

Use `dpkg -L breakpad` to see other available tools.

## Testing a Chromium CL remotely on CrOS CQ

If you have a chromium/src CL that you suspect might have peculiar or risky
effects on a subsequent chromeos-chrome uprev, or if you'd like a bootable
CrOS image generated with your CL incorporated, you can use the
[chromeos-uprev-tester] browser trybot. This trybot will take your CL and run
it through CrOS's CQ, essentially dry-running an attempt to uprev
chromeos-chrome to that Chromium CL.

This has the benefit of running a much larger set of tests than Chrome's other
trybots provide. Additionally, for every board covered in CrOS's CQ during
uprevs, its builders will upload their resultant CrOS images to Google Storage.
This allows you to download locally an image built with your pending Chromium
change without having to compile anything locally.

To use the trybot, click the `CHOOSE TRYBOTS` button on Gerrit on any Chromium
CL and select the `chromeos-uprev-tester` builder. Its duration will be that of
an uprev attempt, which as of writing can be upwards of 12 hours long.

***note
**SIDE NOTE:**

Unlike all other Chrome/Chromium trybots, the chromeos-uprev-tester bot does
_not_ apply the patch on top of ToT during builds. Meaning: if a CL was uploaded
on a parent revision that's seven days old, the uprev-tester builds will compile
using that same seven day old revision.

Consequently, it's suggested that you simply click the `REBASE` button on a
Gerrit CL prior to triggering the chromeos-uprev-tester trybot to compile an
up-to-date browser.

***

## Bisecting Chrome on ChromeOS

[Prepare a DUT] and use the [Shell-less flow].

First find the cros version that introduces the bug.
Flash and test using built images until you identify the cros revision that
introduced the bug:

`cros flash $DUT xbuddy://remote/hana/R91-13844.0.0`

Identify the chrome version bump for that cros version bump using crosland
under 'src/third_party/chromiumos-overlay'.

https://crosland.corp.google.com/log/13843.0.0..13844.0.0

For example this cros version bump 13843.0.0 -> 13844.0.0
bumped chrome from 91.0.4442.0 -> 91.0.4443.0.

To rule out changes in cros deploy the pre-buggy version of cros to the dut:

`cros flash $DUT xbuddy://remote/hana/R91-13843.0.0`

Now start the git bisect.

```
git bisect start
git bisect good 91.0.4442.0
git bisect bad 91.0.4443.0
```

Manually iterate and test the bisect or write a script: [example](https://chromium-review.googlesource.com/c/chromiumos/platform/crosutils/+/2995880)

Manual bisection:

```
gclient sync
autoninja -C out_$BOARD/Release chrome
~/chromiumos/chromite/bin/deploy_chrome --build-dir=out_hana/Release --device=$DUT
ssh $DUT /usr/local/autotest/bin/autologin.py --url chrome://version
# test for the bug
git bisect good/bad
```

### Gotchas with bisect

Deploying chrome using this method does not wipe the device of all state, this
could cause issues with locating the cause of the bug.
For example shaders may be cached by the chrome version string and redeploying
chrome will not cause shader recompilation.
There is no golden bullet way to clear all dut state, some of these may help:

- EC reset (refresh+power)
- `rm -r /var/ ; reboot`
- `stop ui && rm -rf /home/chronos/Local\ State /home/chronos/.oobe_completed /var/lib/whitelist /home/.shadow && crossystem clear_tpm_owner_request=1 && reboot`
- `echo "fast safe" > '/mnt/stateful_partition/factory_install_reset' && reboot`

## Building Ash Chrome in the chroot

Simple Chrome can be a very quick and easy way to build Chrome for ChromeOS.
However, Simple Chrome is not used when building the Chrome that gets released
on ChromeOS. For such releases, Chrome is instead built in the chroot, similar
to any other package. It can be occasionally useful to build Chrome precisely
as it's built in releases, but with your own browser changes. This can be
achieved by following the steps in the [OS development guide] for a package, but
with some special Chrome-specific flags. The basic steps are:

```bash
$ cros_sdk setup_board --board=${BOARD}
$ cros workon --board=${BOARD} start chromeos-chrome chrome-icu
$ cros_sdk --chrome-root <path-to-chrome-checkout> cros_workon_make --board=${BOARD} chromeos-chrome
```

Some notes about the above:
*   If you run into Google Storage authentication issues, be sure to follow
    the gsutil [setup](https://www.chromium.org/chromium-os/developer-library/reference/tools/gsutil/) instructions.
*   `.gclient` in your Chrome checkout should not define `cros_boards` in `custom_vars`.
*   If you run into errors with `pkg-config`, try first running a full
    `cros build-packages --board=${BOARD}` invocation without
    `chromeos-chrome` and `chrome-icu` being cros workon'ed. That's been
    observed to fix/avoid the `pkg-config` errors when building Chrome from
    source in the chroot.
*   By default, the build will not run on RBE. To speed it up, you can try using
    `USE_REMOTEEXEC=true` in the `cros_workon_make` invocation. But
    authentication can be difficult in that case, so YMMV.
*   To fetch third party code and run hook scripts, please make sure to run
    `gclient sync` in your chrome checkout. Previously, chromeos-chrome ebuild
    was running gclient hooks when being built, but R126+ requires the user to
    runhooks themselves.

[Custom build directories]: #custom-build-directories
[Updating the version of the ChromeOS SDK]: #updating-the-version-of-the-chromeos-sdk
[Using a custom ChromeOS build]: #using-a-custom-chromeos-build
[custom gclient var]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/chromeos_build_instructions.md#additional-gclient-setup
[Chrome's builders]: https://www.chromium.org/chromium-os/developer-library/guides/development/chrome-commit-pipeline/
[Command-line flags and environment variables]: #command-line-flags-and-environment-variables
[Deploying Chrome to the user partition]: #deploying-chrome-to-the-user-partition
[Debug builds]: #debug-builds
[Create a bootable USB stick]: #create-a-bootable-usb-stick
[Set up the ChromeOS device]: #set-up-the-chromeos-device
[OS development guide]: https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide
[Chrome source code and depot_tools]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/linux/build_instructions.md
[instructions for running tests on Linux]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/linux/build_instructions.md#Running-test-targets
[update .gclient]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/chromeos_build_instructions.md#updating-your-gclient-config
[ChromeOS board name]: https://www.chromium.org/chromium-os/developer-information-for-chrome-os-devices
[GN build configuration]: https://www.chromium.org/developers/gn-build-configuration
[quick start guide]: https://gn.googlesource.com/gn/+/HEAD/docs/quick_start.md
[device-specific instructions]: https://www.chromium.org/chromium-os/developer-information-for-chrome-os-devices
[generic instructions]: https://www.chromium.org/a/chromium.org/dev/chromium-os/developer-information-for-chrome-os-devices/generic
[rootfs has been removed]: /chromium-os/developer-library/guides/device/developer-mode/#TOC-Making-changes-to-the-filesystem
[remounted as read-write]: https://www.chromium.org/chromium-os/how-tos-and-troubleshooting/debugging-tips#TOC-Setting-up-the-device
[additional debugging tips]: https://www.chromium.org/chromium-os/how-tos-and-troubleshooting/debugging-tips#TOC-Enabling-core-dumps
[chromite repo]: https://chromium.googlesource.com/chromiumos/chromite/
[issue 437877]: https://crbug.com/403086
[CrOS Flash page]: https://www.chromium.org/chromium-os/developer-library/reference/tools/cros-flash/
[VM]: https://www.chromium.org/chromium-os/developer-library/guides/containers/cros-vm/
[Running a Chrome Google Test binary in the VM]: /chromium-os/developer-library/guides/containers/cros-vm/#Run-a-Chrome-GTest-binary-in-the-VM
[go/shortleash]: https://goto.google.com/shortleash
[debugging tips]: https://www.chromium.org/chromium-os/how-tos-and-troubleshooting/debugging-tips
[chrome build instructions]: https://g3doc.corp.google.com/company/teams/chrome/linux_build_instructions.md
[api-keys]: https://www.chromium.org/developers/how-tos/api-keys
[install-build-deps.py]: https://chromium.googlesource.com/chromium/src/+/HEAD/build/install-build-deps.py
[reclient]: https://github.com/bazelbuild/reclient
[reclient-googlers]: https://goto.google.com/chrome-linux-build#set-up-remote-execution
[Chrome-related logs]: https://chromium.googlesource.com/chromium/src/+/lkgr/docs/chrome_os_logging.md
[crbug.com/360342]: https://crbug.com/360342
[crbug.com/403086]: https://crbug.com/403086
[web_tests_linux.md]: https://chromium.googlesource.com/chromium/src/+/refs/heads/main/docs/testing/web_tests_linux.md
[stack traces]: /chromium-os/developer-library/guides/debugging/stack-traces/
[chromeos-uprev-tester]: https://ci.chromium.org/p/chrome/builders/try/chromeos-uprev-tester
[Prepare a DUT]:#set-up-the-chromeos-device
[Shell-less flow]: #shell-less-flow
[ChromeOS Build Instructions]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/chromeos_build_instructions.md
