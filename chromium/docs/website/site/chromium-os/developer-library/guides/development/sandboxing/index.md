---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: sandboxing
title: Sandboxing ChromeOS system services
---


[TOC]

In ChromeOS, OS-level functionality (such as configuring network interfaces)
is implemented by a collection of system services and provided to Chrome over
D-Bus. These system services have greater system and hardware access than
the Chrome browser.

Separating functionality like this aims to prevent malicious websites from
gaining access to OS-level functionality. If Chrome were able to directly
control network interfaces, then a compromise in Chrome would give an attacker
almost full control over the system. For example, by having a separate network
manager, we can reduce the functionality exposed to an attacker to just querying
interfaces and performing pre-determined actions on them.

ChromeOS uses a few different mechanisms to isolate system services from Chrome
and from each other. We use a helper program called Minijail (executable
`minijail0`). In most cases, Minijail is used in the service's init script. In
other cases, the [Minijail library] is used if a service wants to apply
restrictions to the programs that it launches, or to itself.

These different sandboxing mechanisms are described in the [ChromeOS sandboxing
talk] (internal only).

## The “forbidden intersection”

The forbidden intersection is:

*   Running as root, or with `CAP_SYS_ADMIN`, *and*
*   Running in the init PID and mount namespaces, *and*
*   Running without a Seccomp policy and a Landlock policy, or without an
    enforcing SELinux domain.

You **must** avoid the forbidden intersection by having at least one of,
preferably more than one of, and ideally all of:

*   Running as a non-root user with no `CAP_SYS_ADMIN` — [User IDs]
*   Running in a non-init mount and PID namespace, or in a new PID namespace
    with a Landlock policy — [Namespaces] or [Landlock]
*   A Seccomp policy and a Landlock policy, OR running with an SELinux policy
    in an enforcing domain — [Seccomp filters] and [Landlock], or [SELinux].
    Landlock can provide filesystem access restrictions similar to SELinux,
    but it must be combined with Seccomp to escape the forbidden intersection.

You don't normally need both Seccomp and SELinux but for very security-sensitive
workloads this can be required.

### Note on SELinux support

SELinux should be used exclusively when other sandboxing approaches are not
feasible. Examples of this include:

*   Services that need to run as root or with `CAP_SYS_ADMIN`, or
*   Services that require privileged access to the filesystem:
    *   Writing to root-owned files or directories, or
    *   Mounting paths where existing sandboxing tools cannot restrict
    mount paths.

## Best practices for writing secure system services

Just remember that code has bugs, and these bugs can be used to take control
of the code. An attacker can then do anything the original code was allowed to
do. Therefore, code should only be given the absolute minimum level of
privilege needed to perform its function.

Aim to keep your code lean, and your privileges low. Don't run your service as
root. If you need to use third-party code that you didn't write, you should
definitely not run it as root.

Use the libraries provided by the system/SDK. In ChromeOS,
[libchrome] and [libbrillo] (née libchromeos) offer a lot of functionality to
avoid reinventing the wheel, poorly. Don't reinvent IPC; use D-Bus or Mojo.
Don't open listening sockets; connect to the required service.

Don't (ab)use shell scripts. Shell script logic is harder to reason about and
[shell command-injection bugs] are easy to miss. If you need functionality
separated from your main service, use programs written in a primary programming
language like C++ or Rust, not shell scripts. Moreover, when you execute them,
consider further restricting their privileges.

It is strongly recommended to [enable CFI] for code for code not written in a
memory safe language whenever it handles untrusted inputs or runs without all
three techniques mentioned in the Forbidden intersection section.

## Just tell me what I need to do

*   Create a new user (and optionally a new group) for your service.
    See [CrOS user & group management][account management].
*   Use Minijail to run your service as the user (and group) created in the
    previous steps. See [Minijail configuration].
*   If your service fails, you might need to grant it capabilities. See
    [Capabilities].
*   Use as many namespaces as possible. See [Namespaces].
*   If possible, use `-n` to set [`no_new_privs`].
*   Consider reducing the kernel attack surface exposed to your service by
    using Seccomp filters. See [Seccomp filters].
*   Add your sandboxed service to the [security.SandboxedServices] test.
*   Enable the `cfi` and `thinlto` USE flags.

## Minijail configuration

Minijail can be configured via command-line arguments or a
[configuration file][minijail config]. Prefer using a configuration
file, as it's easier to read and can include comments.

Minijail configuration files should be installed to
`/usr/share/minijail/`. An upstart service can invoke minijail with a
configuration file like this:

```
exec minijail0 --config /usr/share/minijail/my_service.conf -- /usr/bin/my_service
```

Example configuration:

```conf
% minijail-config-file v0

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Enable network, ipc, cgroup, pid, and uts namespaces.
e
l
N
p
uts

# Minimal mount namespace.
profile=minimalistic-mountns
# If you don't need /dev, use minimalistic-mountns-nodev instead.

# Set no_new_privs.
n

# Enable seccomp policy.
S = /usr/share/policy/my_service-seccomp.policy

# Set user and group.
u = my_service
g = my_service

# Set up mounts. These will be specific to your service, these are just common examples:

# Mount a tmpfs at /dev and /run.
mount = /dev,/dev,tmpfs,MS_NODEV|MS_NOEXEC|MS_NOSUID,mode=755,size=10M
mount = /run,/run,tmpfs,MS_NODEV|MS_NOEXEC|MS_NOSUID,mode=755,size=10M

# Allow syslog.
bind-mount = /dev/log

# Allow D-Bus.
bind-mount = /run/dbus

# Allow mojo.
bind-mount = /run/mojo,,1

# Allow DNS resolution.
bind-mount = /run/dns-proxy
bind-mount = /run/shill

# Allow UMA metrics.
bind-mount = /var/lib/metrics,,1
```

## User IDs

The first sandboxing mechanism is user IDs (UIDs). We try to run each service as
its own UID, different from the root user, which allows us to restrict what
files and directories the service can access, and also removes a big chunk of
system functionality that's only available to root.

See [CrOS user & group management][account management] for details of adding a new user.

To set the user, add `u = <username>` to your service's minijail configuration.

### chronos-access membership requires SELinux

The forbidden intersection notwithstanding, if a service accesses user data by
having its UID in the `chronos-access` group, it *must* run with an enforcing
[SELinux] domain. These services are accessing data owned by the chronos user,
which could be malicious. A compromised Chrome browser could modify or corrupt
chronos-owned data and attempt to escalate privileges by exploiting or confusing
a service accessing this data.

[SELinux] is useful because it provides finer-grained control over what the
service is allowed to do. Exploitation in these cases doesn't happen via memory
corruption. Instead, the attacker will set up user data to confuse the service
and trick it into performing valid operations on the wrong filesystem objects.
This is usually referred to as a [confused deputy attack]. For example, a
service might be tricked into mounting what it believes is a USB drive. In
reality, however, it ends up mounting a virtual image on top of an existing file
or directory, bypassing our write-XOR-execute restrictions.

Seccomp is not a great option to prevent this type of attack because it's not
granular enough. In the previous example, since the service *is* allowed to
perform mounts, the `mount(2)` system call has to be allowed. Seccomp does not
have the ability to filter path arguments to system calls, so it would be
impossible to restrict path arguments to the mount call. [SELinux], on the other
hand, allows us to restrict a service to only perform operations (like `mount`
calls) on specific paths in the filesystem.

## Capabilities

Some programs, however, require some of the system access usually granted only
to the root user. We use [Linux capabilities] for this. Capabilities allow us
to grant a specific subset of root's privileges to an otherwise unprivileged
process. The link above has the full list of capabilities that can be granted
to a process. Some of them are equivalent to root, so we avoid granting those.
In general, most processes need capabilities to configure network interfaces,
access raw sockets, or performing specific file operations. Capabilities are
passed to Minijail using the `-c` switch. `permission_broker`, for example,
needs capabilities to be able to `chown(2)` device nodes.

From
[`permission_broker.conf`](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/permission_broker/permission_broker.conf):

```bash
start on starting system-services
stop on stopping system-services
respawn

# Run as <devbroker> user.
# Grant CAP_CHOWN and CAP_FOWNER.
exec minijail0 -u devbroker -c 'cap_chown,cap_fowner+eip' -- \
    /usr/bin/permission_broker
```

Capabilities are expressed using the format that [`cap_from_text(3)`] accepts.

## Namespaces

Many resources in the Linux world can be isolated now such that a process has
its own view of things. For example, it has its own list of mount points, and
any changes it makes (unmounting, mounting more devices, etc...) are only
visible to it. This helps keep a broken process from messing up the settings
of other processes.

For more in-depth details, see the [namespaces overview].

In ChromiumOS, we like to see every process/daemon run under as many unique
namespaces as possible. Many are easy to enable/rationalize about: if you don't
use a particular resource, then isolating it is straightforward. If you do
rely on it though, it can take more effort.

Here's a quick overview. Use the command line option if the description below
matches your service (or if you don't know what functionality it's talking
about -- most likely you aren't using it!).

*   `--profile=minimalistic-mountns`: This is a good first default that enables
    mount and process namespaces. This only mounts `/proc` and creates a few
    basic device nodes in `/dev`. If you need more things mounted, you can use
    the `-b` (bind-mount) and `-k` (regular mount) flags.
*   `--uts`: Just always turn this on. It makes changes to the host / domain
    name not affect the rest of the system.
*   `-e`: If your process doesn't need network access. This also isolates
    netlink and [UNIX _abstract_ sockets].  Note: D-Bus and syslog use _named_
    UNIX sockets, so they will still be usable (as long as you bind-mounted
    them).
*   `-l`: If your process doesn't use SysV shared memory or IPC.
*   `-p`: If your process doesn't interact with other process PIDs (other than
    child processes).
*   `-N`: If your process doesn't need to modify common [control groups
    settings].

### When does a process need to run in the init mount or PID namespace?

Almost all processes do not need to run in the init namespace. Please contact
chromeos-security@ for a consultation if you believe that your process needs to
run in the init mount or PID namespace.

### Passing common resources

When using many namespaces to isolate a service, there are some resources
that the service still reasonably should be able to access.

> If bind-mounting on top of /run, you need to mount a *tmpfs* /run:
>
> ```bash
> -k 'none,/run,tmpfs,MS_NODEV|MS_NOEXEC|MS_NOSUID,mode=755,size=10M'
> ```
>
> If bind-mounting on top of /sys, you need to mount a *tmpfs* /sys:
>
> ```bash
> -k 'none,/sys,sysfs,MS_NODEV|MS_NOEXEC|MS_NOSUID,mode=755,size=10M'
> ```

*   syslog: If using `-d` to mount a minimal /dev, you can pass access to the
    syslog daemon by using `-b /dev/log`. If your process mounts all of /dev,
    you need to use `-b /run/systemd/journal` since /dev/log is a symlink to
    /run/systemd/journal/dev-log. In either case, you do not need to specify the
    writable flag to `-b` for this to work. This will work across all namespaces
    (including `-e` network).
*   D-Bus: You can access the system D-Bus by using `-b /run/dbus`.
    You do not need to specify the writable flag to `-b` for this to work.
    This will work across all namespaces (including `-e` network and `-p` PID).
*   Nameservers: If you need to resolve hostnames (DNS), you'll need to pass
   `-b /run/dns-proxy` as that daemon maintains the `/etc/resolv.conf` file.

### When does a process need to run in the Chrome mount namespace?

> Note: Before utilizing these namespaces, please consult with the
> chromeos-security@ team to make sure it's used correctly.

ChromeOS Guest sessions run in an isolated mount namespace bound to the
path `/run/namespaces/mnt_chrome`. This means Chrome runs in the non-init
mount namespace at the path and Cryptohome mounts user profile
directories and Daemon stores in this namespace. Regular sessions, on the
other hand, don't have this session isolation yet.

Any process that needs to access user data during a Guest session must run
in the Chrome mount namespace. For regular sessions however the namespace
isolation is not active. The namespace exists for all sessions, however regular
user sessions setup the user home directories in the root mount namespace to
handle data propagation between ARCVM, Linux VM and other system parts.
Therefore processes must not enter the mount namespace during a regular user
session.

Most processes that access user data utilize daemon stores, which are already
mounted in the Chrome mount namespace. However, if a new process needs to
access user data from user cryptohome by explicitly entering the Chrome mount
namespace by calling [setns(2)] or [nsenter(1)], it can do so by querying
the state of the session isolation from the browser process. Here is an example
[CL](https://crrev.com/c/4179664) for this approach.

## Mount propagation type guidance

When creating a new mount or entering a new mount namespace, an important
consideration is the mount propagation mode of the mount. Some background on
mount types can be found in [Linux kernel mount documentation], but a brief
summary is:

*   Shared (`MS_SHARED`) — allows mount/unmount events to flow in both
    directions between namespaces
*   Mounts-flow-in (`MS_SLAVE`) — only allows mount events to flow in from the
    parent namespace
*   Private (`MS_PRIVATE`) — allows no flow of mount events
*   Unbindable (`MS_UNBINDABLE`) — same as private + you also cannot
    bind-mount to this mount point

For how to change the mount propagation mode when entering a new mount namespace
see the section for `-K[mode]` in [`minijail0(1)`].

### When does a mount need to be shared?

Mounts need to be shared if and only if mount/unmount events need to flow
between namespaces in **both** directions. If mounts only need to flow from one
namespace to the other, then they must be shared on the parent namespace but can
be mounts-flow-in on the child namespace. Making mounts shared increases the
possibilities for interaction between processes that could undermine security of
the system and user namespace separation.

## Seccomp filters

Removing access to the filesystem and to root-only functionality is not enough
to completely isolate a system service. A service running as its own UID and
with no capabilities has access to a big chunk of the kernel API. The kernel
therefore exposes a huge attack surface to non-root processes, and we would like
to restrict what kernel functionality is available for sandboxed processes.

The mechanism we use is called [Seccomp-BPF]. Minijail can take a policy file
that describes what syscalls will be allowed, what syscalls will be denied, and
what syscalls will only be allowed with specific arguments. The full description
of the policy file language can be found in [the `syscall_filter.c` source].

Abridged policy for
[`mtpd` on amd64 platforms](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/mtpd/mtpd-seccomp-amd64.policy):

```
# Copyright 2012 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
read: 1
ioctl: 1
write: 1
timerfd_settime: 1
open: 1
poll: 1
close: 1
# Don't allow mmap with both PROT_WRITE and PROT_EXEC.
mmap: arg2 in ~PROT_EXEC || arg2 in ~PROT_WRITE
mremap: 1
munmap: 1
# Don't allow mprotect with PROT_EXEC.
mprotect: arg2 in ~PROT_EXEC
lseek: 1
# Allow socket(domain==PF_LOCAL) or socket(domain==PF_NETLINK)
socket: arg0 == 0x1 || arg0 == 0x10
# Allow PR_SET_NAME from libchrome's base::PlatformThread::SetName()
prctl: arg0 == 0xf
```

Any syscall not explicitly mentioned, when called, results in the process
being killed. The policy file can also tell the kernel to fail the system call
(returning -1 and setting `errno`) without killing the process:

```
# execve: return EPERM
execve: return 1
```

> **NOTE**: `mmap` and `mprotect` both have argument filters to prevent
> writeable executable memory since that makes certain classes of attacks much
> easier. In most cases `mprotect` does not need `PROT_EXEC`, but you might have
> to use `arg2 in ~PROT_EXEC || arg2 in ~PROT_WRITE` just like `mmap` in cases
> where child processes are executed and need to dynamically link shared
> libraries or the code implements a JIT compiler.

### Generating Seccomp policies using audit (on 4.14+ kernels)

On kernels 4.14 and above we can use the new `SECCOMP_RET_LOG` return value to
make policy generation easier. On these kernels, the `-L` Minijail option will
use `SECCOMP_RET_LOG` as the return value for blocked syscalls: those not listed
in the policy or whose arguments don't match the policy. Instead of killing the
process on a blocked syscall, the kernel will log the otherwise blocked syscall
but will effectively allow it.

The advantage of this mechanism versus what we have available in pre-4.14
kernels is that instead of having to add syscalls to the policy one by one, you
can run the process with `-L`, get a list of all the syscalls not included in
the policy, review them, and automatically generate or augment a policy, all
in one step.

> **NOTE**: Minijail's `-L` flag requires minijail to be built with
> `USE=cros-debug`. Generally, this means that it will not work out of the box
> in prebuilt (e.g Goldeneye, CPFE, etc.) OS images.

Our recommended way of using this functionality is to start with an empty
policy which will cause all syscalls to be logged-but-allowed. The resulting
audit logs (at `/var/log/audit/audit.log*`) can then be parsed with the
[generate_seccomp_policy.py script] to automatically generate a policy. There's
a bit of extra setup required and some associated caveats. Please see the
detailed instructions at `generate_seccomp_policy.py`'s README section on
**[using Linux audit logs to generate policy]**.

This mechanism can also be combined with the `strace`-based mechanism below:
run the process to be sandboxed under `strace`, generate a base policy using
the policy generation script, and then refine it using `-L`.

### Generating a seccomp policy using strace

This is the old and familiar way of generating policies by inspecting syscalls
using `strace`. It does not have any kernel version dependencies and also does
not require a minijail build with `USE=cros-debug`. Similar to audit logs above,
the [generate_seccomp_policy.py script] can accept strace logs from an
unsandboxed process to generate a policy.

> **NOTE**: When creating `strace` logs for arm64, make sure you're running it
> in arm64 userland as most devices that support arm64 kernels run 32-bit arm
> userland by default. The image running on the device should be built for
> 64-bit arm userland e.g. kevin device can run the image built for 32-bit arm
> userland with the `--board=kevin` flag and run the image for 64-bit arm
> userland built with the `--board=kevin64` flag. You can run `file -L /bin/sh`
> command to check which environment you're running on.

#### Generate and pre-process the strace log

```bash
strace -f -o strace.log <program>
```

When sandboxing a dynamically-linked executable, Minijail will default to using
`LD_PRELOAD` to install the seccomp filter. This will install the filter
*after* glibc initialization, so remove the syscalls related to glibc
initialization to obtain a smaller filter (and a tighter sandbox). Those are
normally everything up to and including the following:

```
rt_sigaction(SIGRTMIN, {<sa_handler>, [], SA_RESTORER|SA_SIGINFO, <sa_restorer>}, NULL, 8) = 0
rt_sigaction(SIGRT_1, {<sa_handler>, [], SA_RESTORER|SA_RESTART|SA_SIGINFO, <sa_restorer>}, NULL, 8) = 0
rt_sigprocmask(SIG_UNBLOCK, [RTMIN RT_1], NULL, 8) = 0
getrlimit(RLIMIT_STACK, {rlim_cur=8192*1024, rlim_max=RLIM64_INFINITY}) = 0
brk(NULL)                               = <addr>
brk(<addr>)                             = <addr>
```

If you want to collect `strace` logs for an existing service and you already
have a test device set up, the following steps can help especially if you are
extending an existing policy:

1)   Mount the root-fs read/write. `mount -o remount,rw /`
2)   Edit the init config file in `/etc/init`
     *   Temporarily disable seccomp if it is present.
     *   Add `strace -f -o /tmp/strace.log` before Minijail is invoked. Note
         that this will include all the `minijail0` syscalls as well, but you
         can exclude them later.
3)   Reboot or reload the config `initctl reload-configuration` and restart the
     service.
4)   Run tests or perform actions that exercise the features of the service.
5)   Collect the resulting file `/tmp/strace.log`.

#### Generate policy using strace.log

```bash
~/chromiumos/src/platform/minijail/tools/generate_seccomp_policy.py strace.log > $PROGRAM_NAME.policy
```

### Testing and troubleshooting

*   Test the policy:
    *   `minijail0 -S seccomp.policy -L <cmd>`
    *   If the policy is incomplete:
        *   (Kernel >=4.14) Minijail/audit will log all offending syscalls to
            `/var/log/audit/audit.log` (as discussed above).
        *   (Kernel <4.14) Minijail will crash and log the first offending
            syscall in `/var/log/messages`.

*   You should ensure that your service is executing as many of its code paths
    as possible when executing `strace` or auditing, in particular error paths.
    In some cases it may be easier to add syscalls manually (for instance,
    `abort`) rather than forcing execution of those paths.

*   When you're collecting strace logs to create a Seccomp policy, make sure
    you're in the targeted userland. You can check the environment by running
    `file -L /bin/sh` command.

*   When not using the `-n` Minijail flag, privilege-dropping syscalls happen
    after the filter is installed, and they won't show up in normal program
    execution because they're called by Minijail itself. Because the audit log
    method uses Minijail, *this is only applicable to the `strace` method*. The
    following syscalls need to be added to the policy:
    *   `setgroups(2)`, `setresgid(2)`, and `setresuid(2)` for dropping root.
    *   `capget(2)`, `capset(2)`, and `prctl(2)` for dropping capabilities.
    *   Normally, it's just easier to use the `-n` flag (`no_new_privs`),
        which prevents the sandboxed process from obtaining new privileges and
        is therefore a good addition for sandboxing.
*   Sometimes Minijail will fail to compile the seccomp filter with an error
    similar to:

```
WARNING minijail0[32315]: libminijail[32315]: trailing garbage after constant: 'LOOP_GET_STATUS64'
WARNING minijail0[32315]: libminijail[32315]: compile_atom: /usr/share/policy/e2fsck-seccomp.policy(13): invalid constant 'LOOP_GET_STATUS64'
WARNING minijail0[32315]: libminijail[32315]: could not allocate filter block
WARNING minijail0[32315]: libminijail[32315]: compile_filter: compile_file() failed
ERR minijail0[32315]: libminijail[32315]: failed to compile seccomp filter BPF program in '/usr/share/policy/e2fsck-seccomp.policy'
```

   * This means that one of the constant parameters provided to a syscall could
     not be resolved: e.g. `ioctl: arg1 == LOOP_GET_STATUS64`.
   * To fix this, look up the hex value of the constant and substitute the
     constant e.g. `ioctl: arg1 == 0x4C05`.
   * Minijail resolves these constants based on headers that are in
     [gen_constants-inl.h].

*    Some syscalls may be triggered more deterministically on VMs than in tests
     on real hardware (e.g. `clock_gettime` and `gettimeofday`). Developers
     should be on the lookout for such failures and the Seccomp policies should
     be adjusted accordingly.

#### Finding failing syscalls

If a process violates its seccomp policy, it'll be terminated with `SIGSYS` (bad
system call) and you'll see a message like this in `/var/log/messages`:

```
WARNING minijail0[1415]: libminijail[1415]: child process 1417 had a policy violation (/usr/share/policy/foo.policy)
```

There are a couple of ways to find out what syscall caused the violation:

*   **If you can deploy a debug build of Minijail** (the
    `chromeos-base/minijail` package built with `USE="cros-debug"`) to the
    device, and add `-L` to the Minijail command. Information on failing
    syscalls will be logged to `/var/log/audit/audit.log`. (Kernel version 4.14
    or above is required for this to work.) Note that `-L` also stops policy
    violations from crashing the process, so execution will succeed.

    Many other things are logged to audit.log, so to locate the messages
    related to your binary, look for messages starting `type=SECCOMP`. For
    example, here's a violation message caused by the `true` command making
    syscall 157:

    ```
    type=SECCOMP msg=audit(1641860367.246:224): auid=0 uid=0 gid=0 ses=2 subj=u:r:minijail:s0 pid=13686 comm="true" exe="/usr/bin/coreutils" sig=0 arch=c000003e syscall=157 compat=0 ip=0x7a28525bde35 code=0x7ffc0000
    ```

    (Note that longer command names may be truncated in the `comm=` field.)

*   **If you have a core dump or minidump** (such as the `.core` file found in
    `/var/spool/crash/`, or the minidump attached to a crash report), you can
    open it in a debugger and dump the register values (e.g. with `info
    registers` in gdb or `register read` in lldb). Check the [syscall calling
    conventions] for your architecture to determine which registers contain the
    syscall number and arguments. (Note: 64-bit ARM systems (arm64, aarch64) run
    userspace programs in 32-bit mode, so you'll want to follow the arm calling
    conventions in that case.)

Once you have the syscall number, find out the name of the syscall by looking it
up our in online [syscalls table] or in `minijail0 -H` (run on the same
architecture as the program you're debugging). You can then add the syscall to
your policy.

If you do not want to allow an entire syscall, you can only allow certain
parameters, e.g. `ioctl: arg1 == FDGETPRM`. You can find the values of these
parameters from a core or minidump using a debugger, as described above.

### Installing and applying the generated policy

The policy file needs to be installed in the system, so we need to add it to
the ebuild file. For example:

[`mtpd-9999.ebuild`](https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/chromeos-base/mtpd/mtpd-9999.ebuild)

```bash
# Install seccomp policy file.
insinto /usr/share/policy
use seccomp && newins "mtpd-seccomp-${ARCH}.policy" mtpd-seccomp.policy
```

And finally, the policy file has to be passed to Minijail, using the `-S`
option. Again, using mtpd as an example:

[`mtpd.conf`](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/mtpd/mtpd.conf)

```bash
# use minijail (drop root, set no_new_privs, set seccomp filter).
# Mount /proc, /sys, /dev, /run/udev so that USB devices can be
# discovered.  Also mount /run/dbus to communicate with D-Bus.
exec minijail0 -i -I -p -l -r -v -t -u mtp -g mtp -G \
  -P /mnt/empty -b / -b /proc -b /sys -b /dev \
  -k tmpfs,/run,tmpfs,0xe -b /run/dbus -b /run/udev \
  -n -S /usr/share/policy/mtpd-seccomp.policy -- \
  /usr/sbin/mtpd -minloglevel="${MTPD_MINLOGLEVEL}"
```

## Securely mounting cryptohome daemon store folders

Some daemons store user data on the user's cryptohome under
`/home/.shadow/<user_hash>/mount/root/<daemon_name>` (or equivalently
`/home/root/<user_hash>/<daemon_name>`) and
`/home/.shadow/<user_hash>/mount/root/.cache/<daemon_name>` (or equivalent
`/home/root/<user_hash>/.cache/<daemon_name>`). For instance, Session Manager
stores user policy under `/home/root/<user_hash>/session_manager/policy`. This
is useful if the data should be protected from other users since the user's
cryptohome is only mounted (and therefore decrypted) when the user logs in. If
the user is not logged in, it is encrypted with the user's password.

However, if a daemon is already running inside a mount namespace (`minijail0 -v
...`) when the user's cryptohome is mounted, it does not see the mount since
mount events do not propagate into mount namespaces by default. This propagation
can be achieved, though, by making the parent mount a shared mount and the
corresponding mount inside the namespace a shared or `MS_SLAVE` mount. See
[shared subtrees].

To set up a cryptohome daemon store folder that propagates into your daemon's
mount namespace, add this code to the `src_install` section of your daemon's
ebuild:

```bash
local daemon_store="/etc/daemon-store/<daemon_name>"
dodir "${daemon_store}"
fperms 0700 "${daemon_store}"
fowners <daemon_user>:<daemon_group> "${daemon_store}"
```

This directory is never used directly. It merely serves as a secure template for
the `chromeos_startup` script, which picks it up and creates
`/run/daemon-store/<daemon_name>` and `/run/daemon-store-cache/<daemon_name>` as
shared mounts.

Next, move the user/group setup to `pkg_setup()` since `pkg_preinst()`, where
this is usually done, runs after `src_install()`:

```bash
pkg_setup() {
    # Has to be done in pkg_setup() instead of pkg_preinst() since
    # src_install() needs <daemon_user> and <daemon_group>.
    enewuser <daemon_user>
    enewgroup <daemon_group>
    cros-workon_pkg_setup
}
```
In your daemon's init script, the default is to mount with `MS_SLAVE`
for minijail0 (`MS_PRIVATE` for libminijail) unless `-K<mode>` is passed
when creating your mount namespace. Be sure not to mount all of `/run`. Make
sure to mount with the `MS_REC` flag to propagate any already-mounted cryptohome
bind mounts into the mount namespace.

```bash
minijail0 -v -Kslave \
          -k 'tmpfs,/run,tmpfs,MS_NOSUID|MS_NODEV|MS_NOEXEC' \
          -k '/run/daemon-store/<daemon_name>,/run/daemon-store/<daemon_name>,none,MS_BIND|MS_REC' \
          -k '/run/daemon-store-cache/<daemon_name>,/run/daemon-store-cache/<daemon_name>,none,MS_BIND|MS_REC' \
          ...
```

During sign-in, when the user's cryptohome is mounted, Cryptohome creates
`/home/.shadow/<user_hash>/mount/root/<daemon_name>`, bind-mounts it to
`/run/daemon-store/<daemon_name>/<user_hash>` and copies ownership and mode from
`/etc/daemon-store/<daemon_name>` to the bind target. Since
`/run/daemon-store/<daemon_name>` is a shared mount outside of the mount
namespace and a `MS_SLAVE` mount inside, the mount event propagates into the
daemon. Ditto for `/home/.shadow/<user_hash>/mount/root/.cache/<daemon_name>`
and `/run/daemon-store-cache/<daemon_name>`

Your daemon can now use `/run/daemon-store/<daemon_name>/<user_hash>` to store
user data once the user's cryptohome is mounted. Note that even though
`/run/daemon-store` is on a tmpfs, your data is actually stored on disk and not
lost on reboot.

`/run/daemon-store-cache/<daemon_name>/<user_hash>` works similarly as above,
however contents inside this directory are automatically cleaned up when device
disk space is low, for both logged-in and logged-out users. This is the ideal
directory to store cache contents.

**Be sure not to write to the folder before the cryptohome is mounted**.
Consider listening to Session Manager's `SessionStateChanged` signal or similar
to detect mount events. Note that `/run/daemon-store/<daemon_name>/<user_hash>`
might exist even though cryptohome is not mounted, so testing existence is not
enough (it only works the first time).

The `<user_hash>` can be retrieved with cryptohome's `GetSanitizedUsername`
D-Bus method.

The following diagram illustrates the mount event propagation:

![Mount propagation diagram](sandboxing_daemon_store.png "Mount propagation
diagram")

## Landlock unprivileged filesystem access control

Landlock is a [Linux Security Module] that helps manage filesystem access,
notably even for unprivileged processes. Minijail supports options to help
manage a Landlock policy.

Policies consist of an allowlist of paths, and the specific permissions for a
given path. Minijail includes the following flags to help set up a policy:

*   `--fs-default-paths`: This is recommended for most services, and provides
    access to basic system resources, such as the ability to execute shared
    object libraries in `/lib64`.
*   `--fs-path-ro`: Provides read-only access for a path.
*   `--fs-path-rx`: Provides read and execute access for a path.
*   `--fs-path-rw`: Provides read and basic write access for a path.
*   `--fs-path-advanced-rw`: Provides read and advanced write access for a
    path. In most cases, basic write access is sufficient, but if you need
    special capabilities such as creating symlinks, use this option. The full
    set of additional capabilities is:
    *   `LANDLOCK_ACCESS_FS_MAKE_CHAR`
    *   `LANDLOCK_ACCESS_FS_MAKE_DIR`
    *   `LANDLOCK_ACCESS_FS_MAKE_REG`
    *   `LANDLOCK_ACCESS_FS_MAKE_SOCK`
    *   `LANDLOCK_ACCESS_FS_MAKE_FIFO`
    *   `LANDLOCK_ACCESS_FS_MAKE_BLOCK`
    *   `LANDLOCK_ACCESS_FS_MAKE_SYM`

### Creating useful policies

The objective of Landlock is reducing process interactions via the filesystem.
As such, creating an overly broad policy that includes
RW access to all of `/run` and `/var` would substantially diminish the security
benefits of Landlock.

Instead, allow a minimal set of paths that you need. For example, if you
need to access D-Bus, consider allowing `/run/dbus`.

### Inode-based operation

Policies are based on the state of the filesystem when a policy is applied,
rather than a string comparison against path names. Internally, Landlock
looks at the inodes that exist when the sandbox is entered, so if you need
to create new files or directories you’ll want to specify a Landlock policy
that includes RW access one directory level above.

### Limitations of Landlock

Landlock cannot be used if the sandboxed process needs to modify its
filesystem topology, specifically via `mount(2)` or `pivot_root(2)`. For
additional background, see the official [Landlock documentation].

### Example Landlock config

Below is an example Landlock config file, for a process that needs access to
D-Bus and needs to write to a file in `/var/lib/example_daemon`. If the config
file is named `example_daemon.conf`, you can pass it to Minijail using
`--config=example_daemon.conf`.

```
% minijail-config-file v0

# Filesystem access rules.
fs-default-paths
fs-path-rw = /run/dbus
fs-path-rw = /var/lib/example_daemon

# Other Minijail options....
```

## Minijail wrappers (deprecated)

The [Minijail wrappers] are currently deprecated. They were designed to allow
mocking of individual Minijail configuration settings but we concluded that this
was the wrong level to mock Minijail. The mocks were fragile and wordy. A better
way to mock Minijail is to just abstract away the entire sandboxed process
execution. An example of this can be found in the [SandboxedProcess class] in
debugd.

## Enforcing Control Flow Integrity

[Control Flow Integrity] is a compiler feature for making certain memory
corruption bugs much more difficult to exploit for code execution. Examples
include type confusion, heap use-after-frees, heap buffer overflows, heap double
frees, etc. Violations trigger crashes through SIGILL.

CFI is already [enabled in debugd](https://crrev.com/c/4611534), and you can use
that CL as an example for your service. Please note that CFI isn't free so there
is a ~1% performance cost plus a potential binary size increase. However,
enabling LTO is a prerequisite of CFI which typically leads to a ~3% performance
gain and in some cases leads to a binary size decrease.

Control flow integrity support for ChromeOS is available through
[cros-sanitizers.eclass] which is already inherited for all [ebuild]s that
inherit [platform.eclass]. This means that for most first party [ebuild]s you
just need to set the appropriate USE flags and make sure everything still works.
For things that break with CFI enabled you can add them to a [cfi-ignore.txt]
file. This will be automatically applied if it is placed in the ebuild's `files`
directory.

## Troubleshooting

Since CFI might break certain atypical usage of function pointers and type
casting, it may be necessary to troubleshoot CFI related crashes. A good
starting point is to enable the diagnostic and recovery modes of CFI and run the
package's unit tests for example:

```bash
FEATURES=test USE='cfi cfi_diag cfi_recover thinlto' emerge-${BOARD} debugd
```

CFI violations will look something like:
```
ASAN error detected:
/build/amd64-generic/usr/include/gtest/gtest-matchers.h:234:12: runtime error: control flow integrity check for type 'bool (const testing::internal::MatcherBase<const std::string &> &, const std::string &, testing::MatchResultListener *)' failed during indirect function call
(/usr/lib64/libgtest.so.1.13.0+0x603f4): note: (unknown) defined here
/build/amd64-generic/usr/include/gtest/gtest-matchers.h:234:12: note: check failed in /var/cache/portage/chromeos-base/debugd/out/Default/debugd_testrunner, destination function located in /usr/lib64/libgtest.so.1.13.0
    #0 0x55fd85b5d9e8 in testing::internal::MatcherBase<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&>::MatchAndExplain(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&, testing::MatchResultListener*) const /build/amd64-generic/usr/include/gtest/gtest-matchers.h:234:12
    #1 0x55fd85b6106a in testing::internal::MatcherBase<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&>::Matches(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&) const /build/amd64-generic/usr/include/gtest/gtest-matchers.h:240:12
...
```

At this point it is up to you to determine if a source change is justified or if
it is ok to add the particular function or source file to a [cfi-ignore.txt].
Generally, it is preferable not to exempt code from CFI unless there is a good
reason such as when the code is only for unit test support.

[ChromeOS sandboxing talk]: https://drive.google.com/file/d/1hJOcKaj8FK2sDLX5rSJcmgjEylviVlb_/view
[Minijail wrappers]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/libbrillo/brillo/minijail/
[Minijail library]: https://android.googlesource.com/platform/external/minijail/+/HEAD/libminijail.h
[User IDs]: #user-ids
[Minijail configuration]: #minijail-configuration
[Capabilities]: #capabilities
[Namespaces]: #namespaces
[Landlock]: #landlock-unprivileged-filesystem-access-control
[Seccomp filters]: #seccomp-filters
[enable CFI]: #enforcing-control-flow-integrity
[ebuild]: /chromium-os/developer-library/guides/portage/ebuild-faq/
[cros-sanitizers.eclass]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/eclass/cros-sanitizers.eclass
[platform.eclass]: https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/third_party/chromiumos-overlay/eclass/platform.eclass
[UNIX _abstract_ sockets]: https://man7.org/linux/man-pages/man7/unix.7.html
[security.SandboxedServices]: https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/HEAD/src/go.chromium.org/tast-tests/cros/local/bundles/cros/security/sandboxed_services.go

[SELinux]: https://www.chromium.org/chromium-os/developer-library/reference/security/selinux
[libchrome]: /chromium-os/developer-library/guides/infrastructure/libchrome/
[libbrillo]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/libbrillo
[shell command-injection bugs]: https://en.wikipedia.org/wiki/Code_injection#Shell_injection
[ChromeOS user accounts README]: https://www.chromium.org/chromium-os/developer-library/reference/build/account-management
[Linux capabilities]: https://man7.org/linux/man-pages/man7/capabilities.7.html
[capability.h]: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/include/uapi/linux/capability.h
[`cap_from_text(3)`]: https://man7.org/linux/man-pages/man3/cap_from_text.3.html
[namespaces overview]: https://man7.org/linux/man-pages/man7/namespaces.7.html
[control groups settings]: https://man7.org/linux/man-pages/man7/cgroups.7.html
[`minijail0(1)`]: https://google.github.io/minijail/minijail0.1.html
[minijail config]: https://google.github.io/minijail/minijail0.5#configuration-file
[Linux kernel mount documentation]: https://www.kernel.org/doc/Documentation/filesystems/sharedsubtree.txt
[Seccomp-BPF]: https://www.kernel.org/doc/Documentation/prctl/seccomp_filter.txt
[`syscall_filter.c` source]: https://android.googlesource.com/platform/external/minijail/+/HEAD/syscall_filter.c
[generate_seccomp_policy.py script]: https://android.googlesource.com/platform/external/minijail/+/HEAD/tools/generate_seccomp_policy.py
[using Linux audit logs to generate policy]: https://android.googlesource.com/platform/external/minijail/+/HEAD/tools/README.md#using-linux-audit-logs-to-generate-policy
[shared subtrees]: https://www.kernel.org/doc/Documentation/filesystems/sharedsubtree.txt
[syscalls table]: /chromium-os/developer-library/reference/linux-constants/syscalls/
[syscall calling conventions]: /chromium-os/developer-library/reference/linux-constants/syscalls/#calling-conventions
[gen_constants-inl.h]: https://android.googlesource.com/platform/external/minijail/+/HEAD/gen_constants-inl.h
[confused deputy attack]: https://en.wikipedia.org/wiki/Confused_deputy_problem
[Linux Security Module]: https://en.wikipedia.org/wiki/Linux_Security_Modules
[Landlock documentation]: https://docs.kernel.org/userspace-api/landlock.html#current-limitations
[SandboxedProcess class]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/debugd/src/sandboxed_process.h
[setns(2)]: https://man7.org/linux/man-pages/man2/setns.2.html
[nsenter(1)]: https://man7.org/linux/man-pages/man1/nsenter.1.html
[Control Flow Integrity]: https://clang.llvm.org/docs/ControlFlowIntegrity.html
[cfi-ignore.txt]: https://clang.llvm.org/docs/SanitizerSpecialCaseList.html#format
[`no_new_privs`]: https://docs.kernel.org/userspace-api/no_new_privs.html
[account management]: /chromium-os/developer-library/reference/build/account-management/
