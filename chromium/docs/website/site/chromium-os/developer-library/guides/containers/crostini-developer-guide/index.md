---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: crostini-developer-guide
title: Crostini developer guide
---

If you just want to use Linux, you should read the [Running Custom
Containers Under ChromeOS](/chromium-os/developer-library/guides/containers/containers-and-vms/) doc. This doc is all about
how the crostini is made, not how to use it. :)

[TOC]

## What is all this stuff?

Note: this diagram is not exhaustive, but covers the major services and how they
interact.

![Crostini services diagram](/chromium-os/developer-library/guides/containers/crostini-developer-guide/crostini_services.png)

Googlers: update this image at [go/termina-rpc]

## Where does the code live?

| Name                                   | Affects                  | Repo                                 | ebuild                                                                                           |
|----------------------------------------|--------------------------|--------------------------------------|--------------------------------------------------------------------------------------------------|
| 9s                                     | Host                     | [platform2/vm_tools/9s]              | dev-rust/s9                                                                                      |
| chunnel                                | Host, Termina            | [platform2/vm_tools/chunnel]         | chromeos-base/chunnel, chromeos-base/termina_container_tools                                     |
| cicerone                               | Host                     | [platform2/vm_tools/cicerone]        | chromeos-base/vm_host_tools                                                                      |
| concierge                              | Host                     | [platform2/vm_tools/concierge]       | chromeos-base/vm_host_tools                                                                      |
| container .debs, Termina build scripts | Container                | [platform/container-guest-tools]     | N/A                                                                                              |
| [cros_im]                              | Container                | [platform2/vm_tools/cros_im]         | N/A                                                                                              |
| crostini_client                        | Host                     | [platform2/vm_tools/crostini_client] | chromeos-base/crostini_client                                                                    |
| [crosvm]                               | Host                     | [platform/crosvm]                    | chromeos-base/crosvm                                                                             |
| [garcon]                               | Termina, Container       | [platform2/vm_tools/garcon]          | chromeos-base/vm_guest_tools, chromeos-base/termina_container_tools                              |
| LXD                                    | Termina                  | [github/lxc/lxd]                     | app-emulation/lxd                                                                                |
| maitred                                | Termina                  | [platform2/vm_tools/maitred]         | chromeos-base/vm_guest_tools                                                                     |
| metric_reporter                        | Container                | [platform2/vm_tools/metric_reporter] | chromeos-base/crostini-metric-reporter, chromeos-base/vm_guest_tools                             |
| [seneschal]                            | Host                     | [platform2/vm_tools/seneschal]       | chromeos-base/vm_host_tools                                                                      |
| [sommelier]                            | Termina, Container       | [platform2/vm_tools/sommelier]       | chromeos-base/sommelier, chromeos-base/termina_container_tools                                   |
| system_api                             | Host                     | [platform2/system_api]               | chromeos-base/system_api                                                                         |
| [tremplin]                             | Termina                  | [platform/tremplin]                  | chromeos-base/tremplin                                                                           |
| VM protobufs                           | Host, Termina, Container | [platform2/vm_tools/proto]           | chromeos-base/vm_protos                                                                          |
| vm_syslog                              | Host, Termina            | [platform2/vm_tools/syslog]          | chromeos-base/vm_guest_tools, chromeos-base/vm_host_tools                                        |
| [vsh]                                  | Host, Termina, Container | [platform2/vm_tools/vsh]             | chromeos-base/vm_host_tools, chromeos-base/vm_guest_tools, chromeos-base/termina_container_tools |

[cros_im]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/cros_im/README.md
[crosvm]: https://chromium.googlesource.com/chromiumos/platform/crosvm/+/HEAD/README.md
[garcon]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/garcon/README.md
[github/lxc/lxd]: https://github.com/lxc/lxd
[platform/container-guest-tools]: https://chromium.googlesource.com/chromiumos/containers/cros-container-guest-tools/
[platform/crosvm]: https://chromium.googlesource.com/chromiumos/platform/crosvm/
[platform/tremplin]: https://chromium.googlesource.com/chromiumos/platform/tremplin/
[platform2/system_api]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/system_api
[platform2/vm_tools/9s]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/9s
[platform2/vm_tools/chunnel]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/chunnel
[platform2/vm_tools/cicerone]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/cicerone
[platform2/vm_tools/concierge]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/concierge
[platform2/vm_tools/cros_im]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/cros_im
[platform2/vm_tools/crostini_client]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/crostini_client
[platform2/vm_tools/garcon]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/garcon
[platform2/vm_tools/maitred]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/maitred
[platform2/vm_tools/metric_reporter]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/metric_reporter
[platform2/vm_tools/proto]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/proto
[platform2/vm_tools/seneschal]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/seneschal
[platform2/vm_tools/sommelier]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/sommelier
[platform2/vm_tools/syslog]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/syslog
[platform2/vm_tools/vsh]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/vsh
[seneschal]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/seneschal/README.md
[sommelier]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/sommelier/README.md
[tremplin]: https://chromium.googlesource.com/chromiumos/platform/tremplin/+/HEAD/README.md
[vsh]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/vm_tools/vsh/README.md

## How do I build/deploy/test my change?

### General prerequisites

*   Follow the [ChromiumOS Developer Guide](/chromium-os/developer-library/guides/development/developer-guide/) for setup.
*   Device with test image in developer mode.

Ensure you are able to SSH to the device:

```bash
(inside) $ export DEVICE_IP=123.45.67.765 # insert your test device IP here
(inside) $ ssh ${DEVICE_IP} echo OK
```

For the rest of this document, it will be assumed that the `BOARD` environment
variable in `cros_sdk` is set to the board name of your test device as explained
in the [Select a board](/chromium-os/developer-library/guides/development/developer-guide/#Select-a-board)
section of the ChromiumOS Developer Guide.

Crostini requires a signed-in, non-guest user account to run. You can either use
a test Google account, or run `/usr/local/autotest/bin/autologin.py -d` on a
test image to log in with a fake testuser profile.

### Host service changes

To begin working on a change to one of the host services (see
[Where does the code live?]), use the `cros_workon` command:

```bash
(outside) $ cros workon --board=${BOARD} start ${PACKAGE_NAME}
```

Now that the package(s) are `cros_workon start`-ed, they will be built from
source instead of using binary prebuilts:

```bash
(outside) $ cros_sdk emerge-${BOARD} ${PACKAGE_NAME}
```

Then deploy the package to the device for testing:

```bash
(outside) $ cros deploy ${DEVICE_IP} ${PACKAGE_NAME}
```

Most VM services on the host have upstart conditions `start on started
vm_concierge` and `stop on stopped vm_concierge`. Since restarting only one
daemon could lead to inconsistent state across all services, it's best to shut
them all down, then start them again with a clean slate.
Stopping the `vm_concierge` service on the host stops all running VMs, along
other VM services on the host. Starting `vm_concierge` will trigger other
VM services to start as well.

```bash
(device) # stop vm_concierge && start vm_concierge
```

### Guest service changes

The guest packages that run inside the `termina` VM are built for two special
ChromeOS boards: `tatl` (for x86 devices) and `tael` (for arm devices). These
VM images are distributed as part of the `termina-dlc` DLC package.

To determine the guest board type, run `uname -m` on the device.

| `uname -m` | termina board                        |
|------------|--------------------------------------|
| `x86_64`   | `(outside) $ export GUEST_BOARD=tatl` |
| `aarch64`  | `(outside) $ export GUEST_BOARD=tael` |

First, `cros_workon --board=${GUEST_BOARD} start` each guest package you are
modifying (see [Where does the code live?]):

```bash
(outside) $ cros workon --board=${GUEST_BOARD} start ${PACKAGE_NAME}
```

#### Deploying individual packages

This is the equivalent of `cros deploy` for host OS packages, it will deploy
modified files for a package. Note that this just copies files and won't e.g.
deploy dependencies. Each time you sync you'll usually need to deploy a full
image first, then can deploy individual packages.

Build the package like a normal CrOS board:

```bash
(outside) $ cros_sdk cros_workon_make --install --test --board=${GUEST_BOARD} \
           ${PACKAGE_NAME}
```

Then deploy it like a normal CrOS package, except using a different tool.
`--restart-services` will shut down all running VMs so your changes are picked
up

```bash
(inside) $ ../../chromite/contrib/guestos/deploy_to_termina \
           --board ${GUEST_BOARD} ${DEVICE_IP} ${PACKAGE_NAME} --restart-services
```

#### Deploying a full image

This is the equivalent of `cros flash` for a host OS image, it will rebuild and
redeploy everything and is needed for some changes (e.g. adding new users), but
is slower.

Build the guest image like a normal CrOS board:

```bash
(outside) $ cros build-packages --board=${GUEST_BOARD}
(outside) $ cros build-image --board=${GUEST_BOARD} test
```

This image is installed into the host image by the `termina-dlc` package, and
can be built and deployed like the host service changes above:

```bash
(outside) $ cros workon --board=${BOARD} start termina-dlc
(outside) $ cros_sdk emerge-${BOARD} termina-dlc
# Shut down any running VMs before deploying for changes to get picked up.
$ ssh ${DEVICE_IP} -- restart vm_concierge
(outside) $ cros deploy ${DEVICE_IP} termina-dlc
```

After `cros deploy` completes, newly-launched VMs will use the test DLC with the
updated packages.

#### Deploying a new kernel

When testing changes to the `termina` kernel, the process is similar to a full
image deploy as above, but the kernel will also need to be rebuilt. For more
information specifically about configuration, etc, please reference the
[ChromiumOS Documentation]. The current kernel version may be found in the
[termina configuration] but for the purposes of this documentation we assume
the current (as of writing) 6.1 kernel.

To build the kernel:
```bash
(outside) cros workon --board=${GUEST_BOARD} start chromeos-kernel-6_1
(outside) # use emacs, vim, vscode, or magnetized needle directly on storage medium to effect kernel changes
(outside) cros_sdk cros_workon_make --install --test --board=${GUEST_BOARD} chromeos-kernel-6_1
```

After which you can follow the `termina-dlc` instructions above to deploy the
full image.

### Container changes

Packages can end up in the container by two mechanisms:

1.  Native Debian packages (.debs) are preinstalled in the container, and
    upgraded out-of-band from the rest of ChromeOS by [APT].

2.  Packages built from Portage in ChromeOS are copied into
    `/opt/google/cros-containers` in Termina by the `termina_container_tools`
    ebuild. These are updated with the Termina VM image.

When working on Debian packages, the .debs should be copied to the crostini
container and installed with `apt:`

```bash
# A leading "./" or other unambiguous path is needed to install a local .deb.
(penguin) $ apt install ./foo.deb
```

Portage-managed packages should be treated like other [Guest service changes].
However, the `termina_container_tools` package is not `cros_workon`, so it
must be manually emerged to propagate changes into
`/opt/google/cros-containers`. The following example uses `sommelier`:

```bash
(outside) $ cros_sdk emerge-${GUEST_BOARD} sommelier               # build for Termina
(outside) $ cros_sdk emerge-${GUEST_BOARD} termina_container_tools # copy into /opt
```

Once `termina_container_tools` is manually rebuilt, the `termina-dlc` flow will
work as normal.

#### Building tools directly in the container

Sommelier and cros\_im can be built and run directly from within the container.
Other guest tools do not yet support this.

##### Sommelier

Build with meson/ninja and install with `update-alternatives`:

```bash
sudo apt install clang meson libwayland-dev cmake pkg-config libgbm-dev libdrm-dev libxpm-dev libpixman-1-dev libx11-xcb-dev libxcb-composite0-dev libxkbcommon-dev libgtest-dev python3-jinja2
git clone https://chromium.googlesource.com/chromiumos/platform2
cd platform2/vm_tools/sommelier
meson build && cd build && ninja

sudo update-alternatives --install /usr/bin/sommelier sommelier $(pwd)/sommelier 2
```

As sommelier runs as a service you might want to manually kill already running
sommelier processes so they respawn with the custom sommelier, or simply
restart the container. You can then use gdb to attach to and debug running
sommelier instances.

To switch between the custom sommelier and the sommelier
from termina, run `sudo update-alternatives --config sommelier`.

##### cros\_im

See [cros\_im/README.md][cros_im] for details.

## Running VM executables off of a device

It's possible to run binaries built for the termina VM from the chromium OS
chroot, which can be useful for debugging or testing. Assuming you already have
a chromium OS chroot set up and have built the tatl board, you can run inside
the chroot:

```bash
../platform2/common-mk/platform2_test.py --board tatl [--run_as_root] [command to run]
```

For example, to run LXD, you could run inside the chroot:

```bash
../platform2/common-mk/platform2_test.py --board tatl --run_as_root env LXD_DIR=/path/to/lxd/data lxd
../platform2/common-mk/platform2_test.py --board tatl --run_as_root env LXD_DIR=/path/to/lxd/data lxd waitready
../platform2/common-mk/platform2_test.py --board tatl --run_as_root env LXD_DIR=/path/to/lxd/data lxc [lxc subcommands go here]
```

This is not limited to x86_64 boards either, `platform2_test.py` will
automatically set up QEMU to run ARM binaries if you ask it to run binaries from
an ARM board. This is likely to be very slow, however.

Note that this is not equivalent to actually running the VM, since only the
command you run will be performed. Depending on the exact set up you need to
test, this may not be sufficient.

## Remote Access

For all the below commands, replace USER_HASH with the CrOS user hash. You can
set this with the following command:

```bash
export USER_HASH=$(ssh $DEVICE -- dbus-send --system \
    --dest=org.chromium.SessionManager --print-reply --type=method_call \
    /org/chromium/SessionManager \
    org.chromium.SessionManagerInterface.RetrievePrimarySession \
    | awk 'NR==3'  | cut -d "\"" -f2)
```

### Termina

You can log into a running termina from a remote device using vsh via ssh:

```sh
ssh $DEVICE -t -- vsh --vm_name=termina --owner_id=$USER_HASH --user=root -- LXD_DIR=/mnt/stateful/lxd LXD_CONF=/mnt/stateful/lxd_conf
```

To log in as chronos instead of root remove the '--user-root'. The `LXD_*` lines
are to set up the environment so you can talk to LXD.

Copying files is a bit hackier, but you can use the following:

```sh
# Local to VM
cat $FILE | ssh $DEVICE -- vsh --vm_name=termina --owner_id=$USER_HASH --user=root -- cp /proc/self/fd/0 $DESTINATION

# VM to Local
ssh $DEVICE -- vsh --vm_name=termina --owner_id=$USER_HASH -- cat $FILE > $DESTINATION
```

If you want to copy a directory, sorry, you'll have to do it file-by-file or tar
it first.

### Container

You can log into the container similarly to the above by running

```bash
ssh $DEVICE -t -- CROS_USER_ID_HASH=$USER_HASH vmc container termina penguin
```

Since root-in-termina has access to the container filesystem you can copy files
into the container the same way, with `$DESTINATION` being somewhere inside
`/mnt/stateful/lxd/storage-pools/default/containers/penguin/rootfs/`.

### Container via SSH

The above are somewhat limited (e.g. can only copy one file at a time). You can
also set up sshd inside Crostini and use it for ssh, scp, etc.

1. Delete /etc/ssh/sshd_not_to_be_run
2. Create ~root/.ssh/authorized_keys with the contents of
   <https://chromium.googlesource.com/chromiumos/chromite/+/main/ssh_keys/testing_rsa.pub>
3. Modify /etc/ssh/sshd_config as follows:
    a. Set port to something other than 22 (since host ssh takes that) e.g. 8222
    b. Change PermitRootLogin to yes
4. Restart sshd: sudo systemctl restart ssh
5. Inside Crostini settings enable port forwarding for the port you selected
   e.g. 8222
6. If you’re connecting to your DUT via a tunnel instead of directly set up
   another tunnel for this port

You can now connect to Crostini the same as you would the host.

## Where to find logs

### Chrome

See [Chrome Logging on ChromeOS](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/chrome_os_logging.md).

### Host Services

Host services typically log to `/var/log/messages`. See also
[Logging on ChromeOS](logging.md).

### Termina Services

Several Termina services (maitred, tremplin, and lxd) log to `crosvm`'s log.
This can be found at
`/run/daemon-store/crosvm/<user hash>/log/<base64-encoded VM name>.log` on the
host. The base64 encoding of `termina` is `dGVybWluYQ==`, so Termina's crosvm
log is at `/run/daemon-store/crosvm/<user hash>/log/dGVybWluYQ==.log`.

Logs are rotated daily to `.log.1` and so on.

### Container Services

garcon, sommelier, and other container services log via journald.
Their logs can be viewed by running `journalctl` inside the container.

## Attaching a debugger

### Host Services

Host service changes can be debugged the same as any other host service. See the
[main developer guide](/chromium-os/developer-library/guides/development/developer-guide/#Remote-Debugging) for details.

### Termina Services

In theory you could build gdb into the termina image and use that, or build sshd
and gdbserver in and somehow use that. If anyone does manage to debug termina
services please update this.

### Container Services

You can install and run gdb inside the container for most things, if you get an
error try `sudo gdb`. Anything that's built inside the CrOS chroot can be
debugged using `gdb-$BOARD`, the same as a host service. This requires some setup
but will get you symbols and source code.

**One-time setup**:

1.  Enable ssh into the container using the instructions
    [above](#container-via-ssh). Note that due to security requirements port
    forwarding needs to be turned back on in the Crostini settings each time you
    restart the device.
2.  Install gdbserver and binutils in the container:
    `sudo apt install gdbserver binutils`

Now you can follow the instructions in the
[main developer guide](/chromium-os/developer-library/guides/development/developer-guide/#Remote-Debugging) to debug
chroot-built services inside Crostini the same as you would on the host (except
use tatl/tael as your board).

**Extra-steps for services running under ld-linux**:

gdb cannot automatically load symbols for any of the binaries in
/opt/google/cros-containers; for example, Sommelier. This is because these
binaries use ld-linux to load alternative versions of dynamic libraries. We can
work around this by loading symbols manually.

For each library or binary that's not recognised run the following in a shell
on the device (tip: gdb has a shell command):

```bash
base=$(cat /proc/$PID/maps | grep $BINARY_PATH | head -n1 | cut -d- -f1)
offset=$(readelf -WS $BINARY_PATH | grep \.text) | tr -s ' ' | cut -d' ' -f6)
echo Command is "add-symbol-file $BINARY_PATH 0x$base+0x$offset"
```

Then run the given command in GDB to load symbols for `$BINARY_PATH`.

[APT]: https://en.wikipedia.org/wiki/APT_(software)
[go/termina-rpc]: http://go/termina-rpc
[Where does the code live?]: #repo-table
[ChromiumOS Documentation]: https://www.chromium.org/chromium-os/how-tos-and-troubleshooting/kernel-configuration/
[termina configuration]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/overlays/project-termina/profiles/base/make.defaults
