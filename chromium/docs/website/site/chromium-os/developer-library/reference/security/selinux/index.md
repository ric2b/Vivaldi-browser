---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - Chromium OS > Developer Library > Reference
page_name: selinux
title: SELinux in ChromeOS
---

SELinux is a kernel security module that provides ability to write accessing
policies to archive mandatory access control.

The [SELinux talk] (internal only) describes how SELinux is used in ChromeOS.

In this documentation, it will briefly introduce
  - How SELinux play a role in ChromeOS;
  - How to write SELinux policy for ChromeOS;
  - How to troubleshooting SELinux in ChromeOS.

[TOC]

## Terms in This Documentation

This documentation contains many SELinux terms. They will be explained in this
section.

- `SELinux`: Short for `Security-Enhanced Linux`.
  - Provides MAC (Mandatory Access Control) to Linux.
  - Defines a context for each `object` no matter if it's a process, normal
    file, directory, link, a socket, etc.
  - Describes `actors` (things that initiate action on `objects`). Processes are
    the only actors used in ChromeOS SELinux.
  - When an actor requests to use a permission on an object, SELinux checks the
    predefined policy to see if it's allowed or not. If the policy does not
    allow the action, then the calls that require this permission are denied.
    Automatic transition upon creation or execution of a file is also
    possible.
- `security context`: Security context, also known as security label, is a
  string containing multiple parts, used to identify the process to look up
  security rules before granting or denying access to certain permissions.
  - `domain`: the `security context` for a process can be called as `domain`.
  - `parts`: The security context consists of 4 parts,
    `<user>:<role>:<type>:<range>`. For the syslog file `/var/log/messages`, it
    looks like `u:object_r:cros_syslog:s0`, and for a process like upstart, it
    looks like `u:r:cros_init:s0`.
    - `user`: identifies an SELinux user (not related to POSIX user). ChromeOS
      doesn't use multi-user. The only user is `u`.
    - `role`: identifies an SELinux role. ChromeOS doesn't use multi-role. The
      role for files is `object_r`, and for process (including `/proc/PID/*`)
      is `r`.
    - `type`: the most basic and important part in the security context.
      For ChromeOS, it's a string that uniquely identifies the process or
      file(s) for rules look-up (it names a context). Types should follow the
      [Naming Conventions](#Naming-Conventions).
    - `range`: contains combinations of security classes and security levels.
      Security classes are independent from each other, while one
      security level is dominated by another one. ChromeOS doesn't use
      multi-class security or multi-level security, but ARC container running
      Android program is using MCS and MLS. In `platform2/sepolicy` this is
      always `s0`.
- `attributes`: attribute is a named group of types.
- `rules`: rules defines whether an access request should be allowed, or logged,
  and how the security context will transit after the access.  Common rules to
  be used in ChromeOS (excluding ARC) SELinux policy are as follows. More
  details will be described in sections about how to write policies.
    - `allow`: `allow contextA contextB:class { permissions }`. Grants
      `contextA` `permissions` access to `class` (e.g. file, sock_file, port)
      under `contextB`. Either contextA or contextB can be changed to a group of
      contexts or `attributes` that can be assigned to multiple contexts.
    - `auditallow`: the same syntax as allow. It will log the access after it's
      been granted. auditallow-only will not grant the access.
    - `dontaudit`: the same syntax as allow. It will stop logging the given
      access after it's been denied.
    - `neverallow`: the same syntax as allow. It does not deny anything at
      runtime. It only performs compile-time check to see whether there's any
      conflict between `allow` and `neverallow`. Android has CTS to test the
      policy running active in the system break their neverallows or not.
    - `allowxperm`: `allowxperm contextA contextB:class permission args`. It
      checks the args, too, at permission request. The args are usually not file
      paths but flags, since file paths should already be reflected in the
      context.  `allowxperm`-only will not grant the access. It must be used in
      combination with `allow`. Like `allow`, `allowxperm` also has similar
      command in `auditallowxperm,` `dontauditxperm`, and `neverallowxperm`.
    - `type_transition`: defines how type will auto change to a different one.
      Common type_transitions are:
      - `type_transition olddomain file_context:process newdomain`: when process
        in olddomain executes file_context, the process auto-transits to
        newdomain.
      - `type_transition domain contextA:{dir file ...} contextB [optional
        name]`: when a process in `domain` creates a {dir file ...} (optionally
        named `name`) in `contextA`, the created files are auto-labelled as
        `contextB`. This is only true if the process didn't explicitly set the
        creation context. Without the type_transition, the created dir, file,
        etc. auto-inherits label from its parent.
- `class`: classes are defined in the kernel. They are names for certain kinds
  of object (process, file, socket, etc.).


For more details:

*   [Security Context](https://selinuxproject.org/page/NB_SC)
*   [Object Classes and Their Permissions](https://selinuxproject.org/page/ObjectClassesPerms)

## SELinux in ChromeOS boot process

SELinux has no presence until init (upstart) loads the policy, for example, in
the bootloader. This section will not discuss the stage before initially loading
the policy.

Please note: *SELinux is only enabled on ARC-enabled boards, or amd64-generic
based boards.*

1. init loads the selinux policy based on configs in /etc/selinux/config, and
   mounts the selinuxfs to /sys/fs/selinux. init will be assigned with initial
   contexts (kernel).

1. init re-execs itself, to trigger type_transition `type_transition kernel
   chromeos_init_exec:process cros_init`, which will auto-transits init into
   cros_init domain.

1. init starting up service. init will startup services, and kernel will
   auto-transits based on defined type transition rules. See next section for
   details.

### init starting up services

init will start services based on the config files in /etc/init/ and their
dependencies. The "service" here not only includes the daemon process service,
but also some pre-startup, or short-lived script.

#### Simple startup script embedded in init config file

Simple service startups are simply written in `<service-name>.conf` like

```
exec /sbin/minijail0 -l --uts -i -v -e -t -P /mnt/empty -T static \
    -b / -b /dev,,1 -b /proc \
    -k tmpfs,/run,tmpfs,0xe -b /run/systemd/journal,,1 \
    -k tmpfs,/var,tmpfs,0xe -b /var/log,,1 -b /var/lib/timezone \
    /usr/sbin/rsyslogd -n -f /etc/rsyslog.chromeos -i /tmp/rsyslogd.pid
```
in syslog.conf, or

```
script
  ARGS=""
  case ${WPA_DEBUG} in
    excessive) ARGS='-ddd';;
    msgdump)   ARGS='-dd';;
    debug)     ARGS='-d';;
    info)      ARGS='';;
    warning)   ARGS='-q';;
    error)     ARGS='-qq';;
  esac
  exec minijail0 -u wpa -g wpa -c 3000 -n -i -- \
    /usr/sbin/wpa_supplicant -u -s ${ARGS} -O/run/wpa_supplicant
end script
```
in wpa_supplicant.conf.

The earlier one, without a small script, kernel SELinux subsystem will
auto-transits the domain from `u:r:cros_init:s0` to `u:r:minijail:s0` upon
executing `/sbin/minijail0`, and then auto-transits to `u:r:cros_rsyslogd:s0`
upon executing `/usr/sbin/rsyslogd` so the AVC for domain `u:r:cros_rsyslogd:s0`
can be applied to the process. From that point in the process, any file access,
port usage, network usage, capability request, module load, etc. will be checked
against AVC rules with scontext (source context) being `u:r:cros_rsyslogd:s0`.

While the latter one, with a small script between `script` and `end script`,
init forks a subprocess (still under cros_init domain) to execute shell.
Upon executing shell, this will transit the subprocess to
`u:r:cros_init_scripts:s0` domain, some simple AVCs could be added to cover all
the simple embedded scripts together. Complex scripts or scripts needing
permissions more than file; directory read, write, or creation; or exec,
should be avoided in the simple script, and should use a separate script. Within
the script, it will auto-transits to other domains upon executing the service
program, directly (for example, `exec /usr/sbin/rsyslogd`) or indirectly (via
minijail0 the same as above).

#### Separate script to start the service

But there are some more complex service startup scripts, which are written in a
separate (shell) script. The init config file will look like `exec
/path/to/script.sh` or `exec /bin/sh /path/to/script.sh`

The earlier one is always preferred since it tells the kernel the exact script
being executed, so automatic domain transition can be feasible upon executing
the script, to not mix up permission requirements of the single script to the
whole init script.

The latter one should be **avoided** _if the script has complex permission
requirements_, like special capabilities, create device, modify sysfs, mount
filesystem, or load kernel modules. Using the latter approach will make the
kernel not able to distinguish which script is being executed, since the exec
syscall are always the same to exec `/bin/sh`, even the same with `script ...
end script` simple embedded script.

If using the earlier one in the init config file, it will auto transits to a
custom defined domain, let's say `u:r:cros_service_a_startup:s0`, AVC rules
will be defined for this special domain, and finally a type_transition rule will
transit it from `u:r:cros_service_a_startup:s0`  to the domain owning the
service itself, like `u:r:cros_service_a:s0`. In the new domain, it will confine
the rules for the service.

#### Pre-start, pre-shutdown, and post-stop

pre-start, pre-shutdown, or post-stop scripts are usually simple embedded
scripts like

```
pre-start script
  mkdir -p -m 0750
  /run/wpa_supplicant
  chown wpa:wpa /run/wpa_supplicant
end script
```
in wpa_supplicant.conf.

This can be confined together with `u:r:cros_init_scripts:s0` since

 - It's simple enough, and doing almost similar things as all other simple
   pre-start, pre-shutdown, or post-stop domains;
 - ChromeOS has system image verification to make sure everything under
   /etc/init is the same as original state.

Like startup script, there are still a very small number of services using an
external script file for the startup. For example `pre-start exec
/usr/share/cros/init/shill-pre-start.sh` in shill.conf. This can be either
separate domains if they involve complex permissions like mounting/unmounting
filesystems, loading/unloading kernel modules, or special capabilities.

## How to write SELinux policy for ChromeOS

SELinux policy contains definitions of classes, access vectors (list of
permissions), security contexts (types, users, roles, and ranges), access vector
rules, and transition rules.

For most developers the necessary classes, access vectors, users, roles, and
ranges have already been defined. The most common workflow will be defining new
rules only.

### Defining Types

Types can be defined in the following syntax

```
type <type_name>[, <attribute1>, <attribute2>, ...];
```

This defines a type named `<type_name>`, and optionally adds the attributes:
`<attribute1>`, `<attribute2>`, ...

Also, for an already defined type `<type_name>`, it can add an additional
attribute `<attributeN>` by using the following syntax:

```
typeattribute <type_name>, <attributeN>;
```

While attributes can be defined in

```
attribute <attribute>;
```

### File Contexts

#### System Image

File contexts for files in system image are defined in
`platform2/sepolicy/file_contexts/chromeos_file_contexts`.

Each line defines a path and its security context. For example,

```
/sbin/init u:object_r:chromeos_init_exec:s0
```

It defines the security context (label) for file `/sbin/init` to be
`u:object_r:chromeos_init_exec:s0`. The security context here must be a complete
security context containing user, role, type, and range. Type-only will not
work. ChromeOS files always use `u` as user, `object_r` as role, and `s0` as
range for files in system image.

The path can also be a regular expression. For example

```
/usr/share/zoneinfo(/.*)? u:object_r:cros_tz_data_file:s0
```

Defined file labels are labelled during `cros build-image` phase.  A simple
`emerge` invocation won't label files in the build directory.  `cros deploy`
doesn't label it either.

#### Runtime Files

Runtime files consist of persistent runtime files in stateful partition (for
example, /var/lib, /var/log), and volatile runtime files in tmpfs (for example,
/run).

##### At creation

Both runtime files need to be created at the correct security context.
Relabeling is prohibited in general, except for policy upgrades.

Creation label can be handled by either
[`setfscreatecon`(3)](https://manpages.ubuntu.com/manpages/bionic/en/man3/setfscreatecon.3.html)
from `libselinux`, or type transition rules.

Type transition is recommended since it doesn't require to modify the program to
be SELinux-aware. Developers should try their best to make sure files that need
type transition on creation, are created at a unique path to reduce the usage of
file name in the type transition rules. For example, only the daemon process
creates the corresponding directory like `/var/lib/<service>`,
`/var/log/<service>`, etc, not the startup script, nor some random tests.

```
filetrans_pattern(<domain>, <contextA>, <contextB>, file|dir|...);
filetrans_pattern(<domain>, <contextA>, <contextB>, file|dir|..., <file name>);
```

On the above macros, when `<domain>` creates a file|dir|... under
`<contextA>`, the created file|dir|... will be labelled as `<contextB>`. If
`<file name>` is provided, only the created file|dir|... with the exact name
will be labelled as `<contextB>`.

For example,

```
filetrans_pattern(cros_rsyslogd, cros_var_log, cros_syslog, file, "messages");
```

will label "messages" as `cros_syslog`, because it was created by a process
in `cros_rsyslogd` domain, under directory with `cros_var_log` type. Thus, the
created structure will look like

```
/var/log => u:object_r:cros_var_log:s0
/var/log/messages => u:object_r:cros_syslog:s0
```

##### Persistent label for upgrades or recoveries

For files under persistent path, e.g. /var/lib, or /var/log, the fullpath based
file label **MUST** also be defined in `chromeos_file_contexts` together with
system image.

This is to make sure when policy upgrades, the new label can be restored upon
startup script without the need to recreate the files.

### Access Vector Rules

### Type Transition

Transition rules control auto type transition upon creation of an object (file,
dir, sock_file, etc.), or executing an executable.

`type_transition` shares the same syntax for both file type and process type,

```
type_transition `source_type` `target_type` : `class` new_type [object name];
```

#### File Type Transition

For file type transition, when processes running in `source_type` create a
`class` (file, dir, etc) under `target_type`, the created object is labeled as
`new_type` by default.

The same example as above would be

```
type_transition cros_rsyslogd cros_var_log:file cros_syslog "messages";
```

`filetrans_pattern` macro wraps the type_transition rule and necessary AV rule
to one single macro to let creating object as a different type more easily.

#### Process Type Transition

For process transition, when a process running in `source_type` executes an
executable labelled as `target_type`, the process automatically transits to
`new_type`.

The example is:

```
type_transition minijail cros_anomaly_detector_exec:process cros_anomaly_detector;
```

When a process under minijail executes a file labelled as
`cros_anomaly_detector_exec`, the `after-exec` process will be running under
`cros_anomaly_detector` domain.

There's also a useful macro like `filetrans_pattern` for process type transition
that wraps not only type_transition rule but also corresponding AV rules, called
`domain_auto_trans`. The detail will be explained the later sections.

Besides type_transition rule, there are also some other type rules that are less
frequently used, it can be referred from [SELinux Project
Wiki](https://selinuxproject.org/page/TypeRules)

### Useful Macros in ChromeOS

- filetrans_pattern

  - Syntax:

    ```
    filetrans_pattern(source_type, target_type, new_type, class);
    filetrans_pattern(source_type, target_type, new_type, class, object_name);
    ```

  - Explanation:

    When a process running in `source_type` creates an object in `class` (file,
    dir, etc), in directory labelled as `target_type`. If the object name
    matches `object_name`, the created object is automatically labelled as
    `new_type`.

    `source_type`, or `target_type` can be an attribute.

    `object_name` is optional. If `object_name` is not provided, all objects
    with the `class` created under `target_type` by `source_type` will be
    `new_type`, no matter what name the new object is.

    This macro will also grant necessary access for the creation, it will allow
    - `source_type` to create a `class` as `new_type`.
    - `source_type` to `add_name` in a directory labelled as `target_type`

  - Example:

    ```
    filetrans_pattern(cros_rsyslogd, cros_var_log, cros_syslog, file, "messages");
    filetrans_pattern(chromeos_startup, cros_var, cros_var_log, dir, "log");
    filetrans_pattern(cros_rsyslogd, tmpfs, cros_rsyslogd_tmp_file, file);
    filetrans_pattern({cros_session_manager cros_browser}, cros_run, arc_dir, dir, "chrome");
    filetrans_pattern(cros_browser, arc_dir, wayland_socket, sock_file, "wayland-0");
    ```

- domain_auto_trans

  - Syntax:

    ```
    domain_auto_trans(source_domain, exec_type, new_domain);
    ```

  - Explanation:

    When a process running in `source_domain`, executes a `file` labelled as
    `exec_type`, it becomes a process running in `new_domain`.

    The macro will also grant necessary access for the execution, it will allow
    - `source_domain` to `execute` `exec_type`.
    - to use `exec_type` as the entrypoint of `new_domain`.

  - Example:

    ```
    domain_auto_trans(minijail, cros_rsyslogd_exec, cros_rsyslogd);
    # source_domain chromeos_domain is an attribute.
    domain_auto_trans(chromeos_domain, minijail_exec, minijail);
    domain_auto_trans(cros_init, cros_unconfined_exec, chromeos);
    ```

- from_minijail_preload

  - Syntax:

    ```
    from_minijail_preload(new_domain, exec_type)
    ```

  - Explanation:

    This covers the common special case of minijail executing a file labelled as
    `exec_type` and thereby transitioning to `new_domain`.  It is equivalent to
    `domain_auto_trans(minijail, exec_type, new_domain)`, but makes the
    intention more obvious.  See
    [minijail_te_macros](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/sepolicy/policy/chromeos/minijail_te_macros)
    for more minijail-specific macros.

### Naming Conventions

ChromeOS policies will be combined with Android policies before compiling into
a final monolithic policy. Therefore care should be taken to ensure names don't
conflict with Android policies.

We use the following naming conventions to reduce the possibilities of conflicts.

1. Labels created and used before June 2018, including files and directories in
   the stateful partition, are most likely being used in multiple Android
   branches, and can be difficult to remove. To reduce the chance of breakage,
   these files and directories remain on original label even if it doesn't fit
   into the naming conventions.

1. minijail domain is `u:r:minijail:s0` or `u:r:<something>_minijail:s0`. Most
   minijails should fall in the earlier one since that is the label used for
   minijail processes started from init or init scripts.

1. All ChromeOS files or processes should have its type prefixed with `cros_`
   unless it's described in previous rules.

1. Individual executables that desire automatic domain transition on execution
   must have their type suffixed with `_exec`. Usually these executables are
   labelled as `u:object_r:cros_<something>_exec:s0`

1. Regarding runtime files

  1. `/var/a/b/c/...` except `/var/run/*` (as /var/run is a bind-mount of /run),
  should be labelled in type `cros_var_a_b_c`. For example, `/var/lib/chaps`,
  should be labelled as `u:object_r:cros_var_lib_chaps:s0`.

  1. `/run/a/b/c/...` should be labelled as `cros_run_a_b_c`.

  1. The rule for choosing `c` at which level is not enforced. It's usually
  chosen by a level that you want to isolate access. `c` should be at least the
  same depth as `/run/<service-name>` or `/var/{lib, spool}/<service-name>`, but
  can be deeper if a special isolation of some files is necessary.

  1. A simple pid or temporary state file in random tmpfs, can be labelled
  with type `cros_<something>_tmpfile` or `cros_<something>_pid_file`, and the
  type must have an attribute `cros_tmpfile_type`.

1. Regarding domains

  In general, each service should have its own domain, named in format of
  `u:r:cros_<service-name>:s0`.

1. Regarding attributes

  1. The prefix rule still applies to attributes.

  1. Attributes for files must suffix with `_type`, for example,
     `cros_tmpfile_type`, and `cros_labeled_dev_type`.

  1. Attributes for domains must suffix with `_domain` or `domain`, for example,
     `cros_miscdomain`, `cros_bootstat_domain`. Suffixing with `_domain` is
     preferred over `domain`.

  1. There's one special attribute for domain, named `chromeos_domain`. All
     domains outside ARC container should have this attribute.

### Usage with minijail

Minijail is a tool combined with a library and a wrapper program to apply
different kind of restrictions (cgroups, caps, seccomps, etc), and
namespaces(mountns, pidns, IPCns, etc) in a correct way.

The major problem that the minijail wrapper program faces on SELinux is:

    By default, minijail uses a preload library to hook `__libc_start_main` to
    apply all kinds of restrictions.
    This puts all the privileges that minijail needs into post-exec, where the
    kernel can not distinguish the boundary between minijail and the main
    program without an explicit setcon(3).
    While setcon(3) is unlikely to be possible for our minijail usage since it
    usually attempts to mount procfs readonly.

Of course, granting many unnecessary privileges to the domain is discouraged
since an exploited process will be allowed to do what minijail could do
(mknod, mount filesystems, etc). And we want to reduce the attack surface if
possible.

#### /sbin/minijail0

Minijail wrapper has a static mode that does all the enforcement and lockdowns
pre-exec. Due to lack of ambient caps in the past and seccomp issues, minijail
developed a preload mode that runs as default for shared-linked ELFs that use a
preload library to do the lockdowns post-exec.

But static mode introduces another issue: seccomp.

- If your seccomp filters doesn't have `execve` allowed, minijail0 cannot even
  execute the target program. One workaround for execve is to allow execve but
  it's not what we want even if minijail has isolated many other resources.
- seccomp requires either `sys_admin` capabilities or `no_new_privs` bit.
  Granting `sys_admin` just to install seccomp filter is a terrible idea. But
  `no_new_privs` prevents SELinux domain transition to arbitrary domains
  pre-4.14 kernel.

A common workaround for the two problems above is to install seccomp filter
post-execve, just like what a preload library has been doing. A way to solve
this could be to have embedded minijail, for quickly installing seccomp rules
post-exec since installing seccomp rules doesn't need special capabilities on
SELinux.

##### Example with minijail0 wrapper

For programs using minijail wrapper `/sbin/minijail0`, the following practice is
recommended to isolate pre-minijail and post-minijail as much as possible.

1.  Use `-T static` static mode. This puts all the lockdown steps in the
    minijail0 process.

1.  Use `--ambient` if you have `-c`. Capabilities are not preserved across
    `execve` without ambient caps. You should use `--ambient` whenever possible,
    otherwise your caps won't be preserved to the actual process with `-T
    static`.

1.  Put seccomp into an inner preload minijail. It will look like this to
    launch your program

    ```
    minijail0 -T static --ambient -c 0x999 --somethingelse -- \
    /sbin/minijail0 -n -S rules.seccomp -- /path/to/your/program
    ```

SELinux domain remains on minijail when applying all other restrictions. Upon
first execve to execute inner minijail0, there's no domain transition. And the
inner minijail simply executes the target program with preload library, when
the domain transition happens before setting nnp bit.

#### Libminijail

TODO

### Writing SELinux policy for a daemon

In this section, we'll take an example of steps to confine and enforce tcsd.

1.  Define executables

    tcsd has one executable at `/usr/sbin/tcsd`.

    1.  We define the type in `sepolicy/policy/chromeos/file.te` like
        [this](https://chromium.googlesource.com/chromiumos/platform2/+/a5eb780d85b892367a696ff07f0cd8eb1777495d/sepolicy/policy/chromeos/file.te#63)

        `type cros_tcsd_exec, file_type, exec_type, cros_file_type,
        cros_system_file_type;`

    1.  We define the context for /usr/sbin/tcsd in
        `sepolicy/file_contexts/chromeos_file_contexts` like
        [this](https://chromium.googlesource.com/chromiumos/platform2/+/a5eb780d85b892367a696ff07f0cd8eb1777495d/sepolicy/file_contexts/chromeos_file_contexts#75)

        `/usr/sbin/tcsd u:object_r:cros_tcsd:exec:s0`

1.  Define domains

    Define domains and transitions like
    [this](https://chromium.googlesource.com/chromiumos/platform2/+/511fe108bd9b9dcfe3ea51665871c58ea669bfbc/sepolicy/policy/chromeos/tpm/cros_tcsd.te)

    ```
    type cros_tcsd, chromeos_domain, domain;
    permissive cros_tcsd;
    domain_auto_trans(cros_init, cros_tcsd_exec, cros_tcsd);
    ```

    The 1st line defines a new type `cros_tcsd` to be a `chromeos_domain` and a
    `domain`. These two attributes are mandatory for ChromeOS SELinux policies.

    The 2nd line put the domain into permissive, so any actions performed by
    this domain will be audited, but not denied. Initially having the domain in
    permissive mode will allow you to see all the potentially denied actions
    instead of stopping at the first one. You'll need `USE="selinux_develop"`
    use flag to make ChromeOS kernel to print permissive audit logs.

    The 3rd line defines an automatic domain transition, when any process at
    domain `cros_init`, executes a binary labelled as `cros_tcsd_exec`, it
    automatically transits to domain `cros_tcsd`. The macro will create
    corresponding `type_transition` and `allow` rules.

    It's time to verify if your process is running under the correct domain.

    - `cros-workon-$BOARD start selinux-policy` for the sepolicy change to take
      effect. Also `cros build-image` and `cros flash` is needed to update the
      selinux contexts of the files.

    - If it's a daemon process, simply `ps auxZ | grep tcsd | grep -v grep`. It
      will display the process matching `tcsd` with its pid, user, command line,
      and domain, etc.

    - If it's not a daemon process, but shortlived, you can verify it by
      - printing `/proc/self/attr/current` in the process to know its domain, or
      - you can observe the audit log from either dmesg, /var/log/messages, or
        journald, to grep `scontext=u:r:cros_tcsd:s0`. Unless you program is
        simply doing some math, therefore doesn't violate any existing SELinux
        rules, you should be able to see some permissive audit logs if you have
        `USE="selinux_develop"` use flag enabled.

1.  Update SELinux tests

    __New SELinux tests should be written in tast instead of autotest.__

    Tests for SELinux file labels and process domains are located at
    `src/platform/tast-tests/local_tests/security`

    Usually, you'll need to update `selinux_files_system.go` and
    `selinux/processes_test_internal.go`.

    In the example case, we'll need to add
    `
    {Path: "/usr/sbin/tcsd", Context: "cros_tcsd_exec", IgnoreErrors: true},
    ` in selinux_files_system.go, and
    `
    {exe, "/usr/sbin/tcsd", matchRegexp, "cros_tcsd", zeroProcs, ""},
    ` in selinux/processes_test_internal.go

    The files check checks file at `/usr/sbin/tcsd` has labels
    `u:object_r:cros_tcsd_exec:s0`, without recursion, ignore if file doesn't
    exist.

    The domain check checks a process with `/proc/<pid>/exe` to be
    `/usr/sbin/tcsd` (a.k.a any process running tcsd binary) to be `cros_tcsd`
    domain. The domain check can also match by cmdline.

1.  Write actual rules

    After creating a permissive domain for your daemon, now you need to write
    actual rules before making the daemon domain SELinux enforcing.
    Follow these steps to add the needed access rules:

    - Build and flash a new image with the USE flag ‘selinux_develop’. You need
    this flag to log permissive denials.

    - Run all the tests that are related to your daemon on the test device.

    - Check the SELinux violations logged at /var/log/audit/audit.log on your
    test device and add the required access rules to the policy as explained in
    [How to read the denials in audit logs].

    - You can use 'audit2allow' script instead to define the access rules. Run
    'audit2allow’ script on the audit.log file in your chroot:

    ```
    audit2allow -xp /build/$BOARD/etc/selinux/arc/policy/policy.30 -i /path_to_audit_log/audit.log
    ```

    - Add the rules created by the audit2allow to the policy.

    - Use macros where possible and group the rules that apply to the same file
    context. Macros are defined in [base/imported/global_macros],
    [base/imported/te_macros] and [chromeos/te_macros]. Find the appropriate
    macro that includes all the access vectors reported by audit2allow. For
    example if audit2allow generates the following allow rule:

    ```
    allow <DomainA> <ContextA>:file { getattr open read ioctl lock map }
    ```

    Use 'r_file_perms' macro instead:

    ```
    allow <DomainA> <ContextA>:file r_file_perms
    ```

    - emerge and deploy the updated policy and run the tests again. You might
    want to remove /var/log/audit/audit.log file before running the tests to
    avoid any confusion.

    If you receive the same SELinux violation log despite adding the required
    access rule this means you need to add an additional rule related to the
    same violation. Right place to start the investigation is the audit logs at
    '/var/log/audit/audit.log' and system log messages at '/var/log/messages'
    together. Identify the step causing the violation. Examples could be the
    need to define an ioctl or a key search permission requirement or you may
    need to change how the file context is defined.

    - Once the policy is complete for the specific board you are testing the
    policy on, you need to make sure policy doesn't fail for other boards as
    well. There are macros to restrict the scope of the access rules for
    different cases. 'has_arc' or 'is_arc_vm' are the two examples for these
    macros. For example, /home/root/hash/android-data/data directory only
    exists if ARC is enabled on the device. Therefore if you need to define an
    access rule for the file context 'media_rw_data_file' you need to use
    ‘has_arc’ macro.

    - If SELinux logs report a violation against an unlabeled or an unconfined
    context define the context properly. Run the specific test triggering this
    rule and identify the specific step to find out the unlabeled/unconfined
    object. Label them properly following the steps defined in [File Contexts].

    __Testing after adding new rules:__

    - emerge and deploy selinux-policy after adding the new rules. Don't forget
    to run `cros-workon-$BOARD start selinux-policy` before
    emerging the selinux-policy. And repeat the tests.

    - Domain is ready to become enforcing once there are no more SELinux
    violations logged at /var/log/audit/audit.log for your domain (or when
    audit2allow doesn't generate any rules for your domain anymore).

    - Remove 'permissive cros_tcsd' line to make the domain enforcing.

    - If you've added or updated any file context you need to build a new image.
    Even if you haven't changed any file context you still need to build a new
    image without the USE flag 'selinux_develop' since the daemon will be
    running in an enforcing domain (remember this flag is to log permissive
    denials). if there is any missing access rule for the daemon that will
    be logged since the daemon will be running in an enforcing domain.

    - Repeat the tests. All the tests need to pass and no more violations should
    be logged.

    - If any of the tests fail and SELinux violations are logged for your
    daemon, that is possibly because some of the violation logs were overridden
    on audit.log when you were running the tests earlier. In this case you may
    need to run the tests in smallers test sets. Or you can use a simple script
    to filter out the logs you are looking for. A sample script could be like
    this:

    ```
    #!/bin/bash

    touch selinux_rules
    chmod 666 selinux_rules
    for (( i=1; i<=360; i++ ))
    do
    scp DUT://var/log/audit/audit.log ./audit.log
    audit2allow -p /build/puff/etc/selinux/arc/policy/policy.30 -i ./audit.log | grep 'allow cros_cryptohomed' | tee -a selinux_rules
    echo $i
    sleep 30
    done
    ```

    This script copies the audit.log file from a puff board, converts the
    violation logs to allowrules, greps only the rules that are part of
    cros_cryptohome domain policy and dumps them to a separate file. Repeat the
    previous steps until all the tests are passing and no more violations are
    logged on audit.log for your daemon.


    - Before uploading the updated policy, make sure unit tests pass as well.
    Android neverallow rules apply to ChromeOS since ChromeOS and Android
    share the same kernel when Android is running in the ARC container. These
    neverallow rules are not merged to the policy and don't take effect at
    compile time. They are rather injected into the selinux-policy unit tests
    and that's why you need to run the unit tests to test against the Android
    neverallow rules:

    ```
    FEATURES=test emerge-DUT-with-ARC-Container selinux-policy
    ```

    - Note that the neverallow rules are not imported by the VM unittests like
    amd64-generic or betty-pi-arc boards, so you need to use a physical ARC
    container device build to run the unit tests for the updated policy.

    After updating the policy, go back to previous step to update SELinux tests
    for stateful files too.

    If your process relates to after-login behavior, you may also need to update
    `selinux_files_arc.go` and `selinux_files_non_arc.go`. Non-ARC specified
    process should present in both. If your process relates to files in home
    directory, simply modify `selinux/files_test_internal_home.go` instead.


1.  Enforce your domain

    When you're sure your policy fully covers the expected behavior (only
    behavior used by ChromeOS). You can remove the `permissive cros_tcsd` from
    the policy.


## Troubleshooting

### How to rule out SELinux a possible cause of a problem

Let's assume you write your own program, or modified a program. Suppose it
doesn't work along with the system, you're sure you didn't implement anything
wrong, and suspect it could be SELinux denying some operations.

There're three approaches to identify an potential SELinux problem.

1.  From M76, ChromeOS uses auditd to receive audit events from the kernel, and
    write corresponding audit messages to `/var/log/audit/audit.log`. During the
    early
    boot stages before auditd starts, `[kauditd]` will still write audit
    messages into syslog. Developers are supposed to examine both
    `/var/log/messages` and `/var/log/audit/audit.log` if they don't know at
    which stage a denial could occur.

    - pre-auditd: syslog will be logged to `/var/log/messages` and systemd-journald. You can
      read the log by reading `/var/log/messages` or by executing `journalctl`.
      Please note the messages file could be rotated to
      `/var/log/messages.{1,2,3,4,...}` if the system is running long term.
    - post-auditd: auditd will receive audit events from kernel via
      audit netlink socket, and write to `/var/log/audit/audit.log`. Auditd will
      handle log rotation to rotate the logs to
      `/var/log/audit/audit.log.{1,2,3,4}`.

    You should be able to find `permissive=0` in above log locations. If you saw
    some denials with `permissive=1`, it doesn't mean it's denied.
    `permissive=1` only mean this access it not allowed by policy, but SELinux
    is still allowing it because the domain is not enforced.

1.  A quick command could test whether your program works by putting the whole
    system permissive. By executing `setenforce 0` as root in developer mode,
    you can put the whole system permissive. You'll be able to test if your
    program comes to work.

1.  If you program is a daemon process which fails so early before you can have
    a console access, you could change the SELinux config file located at
    `/etc/selinux/config` to
    ```
    SELINUX=permissive
    SELINUXTYPE=arc
    ```
    Please keep `SELINUXTYPE=arc` unchanged, and only changing `SELINUX=` line
    to `permissive`.  Please don't change it to `disabled` otherwise your system
    may fail to boot since init will halt when it fails to load an SELinux
    policy.

Approach 2 and 3 to put the whole system permissive won't give you any useful
information on what's wrong. Audit log won't print even the policy says to deny
this because the whole system is permissive. It only gives you a true or false
answer, you'll need the audit logs to find out exact what the problem is. We'll
talk more at the how to debug section.

### How to read the denials in audit logs

An AVC audit message looks like

```
audit: type=1400 audit(1550558262.594:5434): avc:  denied  { read } for
pid=26768 comm="cat" name="messages" dev="dm-0" ino=40
scontext=u:r:cros_ssh_session:s0 tcontext=u:object_r:cros_syslog:s0 tclass=file
permissive=1
```

We'll walk through this audit log as an example.

- `type=1400` means it's an AVC audit log. In most cases, you're looking at this
  kind of audit logs, so you can ignore this.
- `denied` means this permission usage is denied. The other result here could be
  `granted`, `granted` is only printed if the AVC rule has **auditallow** rules
  like `auditallow domainA domainB:file read`.
- `{ read }` means the permission requested is `read`, there could be many
  different kind of permissions, like `open`, `execute`, `append`, `name_bind`,
  `unlink`, etc. The permission inside `{}` could be more than one in one audit
  message, like `{ read open }`.
- `pid=26768` means the pid of the process.
- `comm="cat"` means the program command is cat. It's identical (but truncated)
  to /proc/PID/comm
- `name="messages"`, `dev="dm-0"`, `ino=40` are related information. Details in
  [the SELinux Wiki](https://selinuxproject.org/page/NB_AL).
- `scontext=u:r:cros_ssh_session:s0` means the source context for this
  permission request. If it's a process, it means the domain for the acting
  process.
- `tcontext=u:object_r:cros_syslog:s0` means the target context for this
  permission request. The tcontext could be file labels if it's a file (open,
  read, write, create, etc), process domain (use fd, netlink socket read/write,
  /proc/PID/ to read process attributes, unix socket sendto, etc), filesystem
  type (associate cros_var_lib on labeledfs, associate logger_device on
  devtmpfs, etc), and etc.
- `tclass=file` means the class type of the acting target. For example, in this
  case, it's `file`, it could be `udp_socket`, `fd`, `capability`,
  `netlink_kobject_uevent_socket`, etc.
- `permissive=1` means current audit is permissive or not. If it's permissive,
  it's not really getting denied, otherwise it really denies this permission
  request.

For more details, it can be referred from

[SELinux Project Wiki: NB AL](https://selinuxproject.org/page/NB_AL)

[Red Hat Documentation: Raw Audit
Messages](https://access.redhat.com/documentation/en-us/red_hat_enterprise_linux/6/html/security-enhanced_linux/sect-security-enhanced_linux-fixing_problems-raw_audit_messages)

[Gentoo Wiki: Where to find SELinux permission denial
details](https://wiki.gentoo.org/wiki/SELinux/Tutorials/Where_to_find_SELinux_permission_denial_details)

[CentOS Wiki: Troubleshooting
SELinux](https://wiki.centos.org/HowTos/SELinux#head-02c04b0b030dd3c3d58bb7acbbcff033505dd3af)

### How to debug your SELinux policy

#### Analyzing audit logs

The most important and fundamental way to debug your policy is to read the audit
log.

In the previous audit log example, we know it's "`cat`" process in
`cros_ssh_session` domain was denied to read file "`messages`" in device `dm-0`
labelled as `cros_syslog`.

There're the main things to look at in the audit logs.

- Process (`actor` in SELinux terminology)
  - Which process (pid / name);
  - Which context (scontext);

- Target (`object`)
  - Which target?
  - Which class. process, file, sock_file, port, etc.?

By looking at the log, the main thinkabout would be:

- Are the process and target at the correct context? No? => Fix the context.
  - File label:
    - In system image: add the context to
      `platform2/sepolicy/file_contexts/chromeos_file_contexts`, and
      `platform2/sepolicy/sepolicy/chromeos/file.te`.
    - In stateful partition:
      - (Recommended and Easiest) Introduce a new file type in
        `.../sepolicy/chromeos/file.te`, and use `filetrans_pattern` macro to
        allow auto assigning labels upon file creation.
      - Introduce a new file type in `.../sepolicy/chromeos/file.te`, and modify
        the program to set correct creation label.
        [setfscreatecon(3)](https://manpages.ubuntu.com/manpages/bionic/en/man3/setfscreatecon.3.html)
  - Process domain: fix the executable file label, and use `domain_auto_trans`
    macros if possible.
  - Port context, etc: you're probably already an advanced SELinux policy writer
    if you met this point. You can refer to [SELinux Project
    Wiki](https://selinuxproject.org/), for example
    [portcon](https://selinuxproject.org/page/NetworkStatements#portcon)
- Is the action really needed? No? => Fix the program to eliminate the action.

  Examples of mostly unneeded actions:
  - relabelfrom/relabelto: only some startup script should need this.
  - capability dac_override: in most cases, it could be avoided by reordering
    chown / actual read-write.
  - mount/mounton: except for some startup script, or programs using libminijail.
    This should be avoided. For programs using minijail0 wrapper, `-T static`
    mode is strongly recommended to leave all the high privileged permissions to
    minijail.

##### selinux_violation files in /var/spool/crash

/var/spool/crash contains crash reports to be uploaded by `crash_sender` to
crash.corp. Usually it stores data like core dumps and metadata when a program
has crashed. But some other anomalies (e.g. selinux violation, service death,
kernel warnings, etc) also take advantage of the existing crash reporting
mechanism. See [Crash
Reporter](https://www.chromium.org/chromium-os/packages/crash-reporting/faq#TOC-Crash-Reporter)
to know more about how crash reporting works.


As mentioned, we take advantage of existing crash reporting mechanism for
SELinux violation collection for enforced domain. Since any unpermitted access
will trigger an audit event, to reduce the chance it fills up 32 reports pool,
we sample audit message at a probability of 0.1% before writing to crash pool
for acknowledged users on user build. But for developer build, we still write
all audit events dumped to syslog to crash pool.

If you see any `selinux_violation`\* in `/var/spool/crash`, it doesn't mean that
something has crashed. It only means an audit event has occured. SELinux doesn't
kill any process violating the policy, it just forces the corresponding syscall
to return `-EACCESS` (permission denied). In most cases, you don't need to care
about what's being stored in /var/spool/crash for SELinux violations. If you
need, `/var/log/messages` or `/var/log/audit/audit.log` should provide you the
information you need.

#### Inspecting the runtime state

- File labels: `ls -Z file` or `ls -Zd directory`
- Process domains: `ps -Z`, `ps auxZ`, `ps -Zp <PID>`
- Current domain: `id -Z`
- Run in a different domain: `runcon <context> <program> <args...>` for example,
  `runcon u:r:cros_init:s0 id -Z`. The transition from current domain to new
  domain must be allowed to let this work. Currently, either `cros_ssh_session`
  or `frecon` is running permissive, it should always work if you're executing
  in the console, or via ssh.

#### Update the policy

After understanding why the denials occurred by reading the log, policy may need
updating to fix the problem.

##### Locate the policy

In general, ChromeOS policy lives in `sepolicy` directory in
`chromiumos/platform2` project, which is `src/platform2/sepolicy` in the repo
tree checkout.

A quick grep on the scontext will locate the where it is defined, and most of
its policies.

For example, if we want to change `cros_ssh_session`:

```
$ grep cros_ssh_session . -R
./policy/chromeos/dev/cros_ssh_session.te:type cros_ssh_session, domain, chromeos_domain;
./policy/chromeos/dev/cros_ssh_session.te:permissive cros_ssh_session;
./policy/chromeos/dev/cros_ssh_session.te:typeattribute cros_ssh_session netdomain;
./policy/chromeos/dev/cros_sshd.te:domain_auto_trans(cros_sshd, sh_exec, cros_ssh_session);
./policy/chromeos/file.te:filetrans_pattern(cros_ssh_session, cros_etc, cros_ld_conf_cache, file, "ld.so.cache~");
./policy/chromeos/log-and-errors/cros_crash.te:-cros_ssh_session
```

We can see it's defined in `cros_ssh_session.te`, which means most of our
changes should live in that file.

##### Searching the compiled policy file

`sesearch` is an excellent tool to search inside a compiled policy. You can use
this tool to search what is allowed, what denials are not logged, what grants
are logged, and type transitions, etc.

on Debian-based systems (or gLinux), `sudo apt install setools` will install
this tool.

You can search a policy file, say, `policy.30`, in following examples:

```
# Search all allow rule with scontext to be cros_ssh_session or attributes
cros_ssh_session attributes to, tcontext to be cros_sshd or attributes cros_sshd
attributes to with class to be process
$ sesearch policy.30 -A -s cros_ssh_session -t cros_sshd -c process
# Search all type transitions with scontext to be exactly minijail
$ sesearch policy.30 -T -s minijail -ds
```

`man sesearch` will provide all the options to search `allow`, `auditallow`,
`dontaudit`, `allowxperm`, etc, with filters on scontext, tcontext, class,
permissions.

##### Put domain to permissive

Sometimes, during debugging, you may not to want to put the system permissive.
You can put only one domain permissive.

1. Locate the policy file as above.

1. Simply add `permissive <domain type>`, for example, `permissive
cros_ssh_session` will put `cros_ssh_session` to permissive.

This will only put the given domain to permissive, and everything with the
permissive domain (scontext) will not actually be denied.

But please note: some operations may indicate other permissions at runtime. For
example, file creation will check
`{ associate } scontext=file_type tcontext=fs_type class=filesystem `, so these
kinds of denials may occur.

##### Writing policy fix

1. Identify whether labeling files is needed. If yes, label the files either in
   file_contexts or via type_transition.

1. Fix the program or add `dontaudit` rule to prevent from spamming logs if it
   shouldn't be allowed.

1. Write allow rule or allowxperm rule based on denials seen, and the behavior
   analysis of the program.

   1. for one-time program-specific permission requests, simply `allow[xperm]
      scontext tcontext:class perms [args for allowxperm];`
      `scontext`, `tcontext`, and `perms` can be plural in format like `{ a b c
      }`

   1. for permission requests that may apply to other programs, create an
      attribute and attribute current domain to it. And write corresponding
      rules for that attribute.

   1. use m4 macros wisely, we have many useful macros like `r_file_perms`,
      `rw_file_perms`, `create_file_perms`, `filetrans_pattern`, and
      `domain_auto_trans`.

For more details in writing policy, please refer to previous sections about
writing policies.

#### Useful build flags for debug

1. `USE="selinux_develop"`: log permissive denials and make sure log is almost
   not suppressed by printk limit.

1. `USE="selinux_experimental"`: build with SELinux mode in permissive by
   default. This is equivalent to manually changing `SELINUX=permissive` in
   `/etc/selinux/config`

1. `USE="selinux_audit_all"`: remove all the `dontaudit` rule before compiling
   to the final monolithic policy. There're some should-be-denied access with
   `dontaudit` rules, so denials don't spam the log. But you may want to see
   them sometimes during development or debugging process.

For Googlers, there's a nice introduction presentation slides how debugging
SELinux policies to refer to though it's for Android, at
[go/sepolicy-debug](https://goto.google.com/sepolicy-debug)

[SELinux talk]: https://drive.google.com/file/d/1zT0IH3fg2KoTx0phU78aj6pEscHPIqAq/view
[File Contexts]: https://chromium.googlesource.com/chromiumos/docs/+/main/security/selinux.md#File-Contexts
[How to read the denials in audit logs]: https://chromium.googlesource.com/chromiumos/docs/+/main/security/selinux.md#How-to-read-the-denials-in-audit-logs

[base/imported/global_macros]: https://source.corp.google.com/chromeos_public/src/platform2/sepolicy/policy/base/imported/global_macros
[base/imported/te_macros]: https://source.corp.google.com/chromeos_public/src/platform2/sepolicy/policy/base/imported/te_macros
[chromeos/te_macros]: https://source.corp.google.com/chromeos_public/src/platform2/sepolicy/policy/chromeos/te_macros
