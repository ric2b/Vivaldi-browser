---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: noexec-shell-scripts
title: Shell scripts & noexec mounts
---

ChromeOS has added logic to the shells we ship (e.g. [dash] & [bash]) to detect
when code is being run from `noexec` partitions.
This can cause trouble for code that previously worked, or continues to work on
systems other than ChromeOS.

Here we'll dive into the technical details and how to address common problems.

[TOC]

## The Journey

### What are noexec mounts?

When creating mounts in Linux, you can add `noexec` to the options in order to
get a path where the kernel will reject attempts to execute code.
Instead, you'll get `EACCES` (Permission denied) errors whenever you try.

For example:

```sh
# /bin/hostname
localhost
# cp /bin/hostname /var/
# /var/hostname
-bash: /var/hostname: Permission denied
# mount /var -o remount,exec
# /var/hostname
localhost
```

### Why do we want noexec mounts?

noexec is extremely useful for security as it means we can strongly separate
read-only code from writable data.
This makes it harder for attackers as they won't be able to download arbitrary
programs from the network and execute them directly, or to copy the programs to
a writable location and then trick a privileged service into executing them.
Both of those are common techniques when attacking a system & building up an
[exploit chain].

In ChromeOS, programs need to be able save state somewhere (e.g. log files,
user preferences, network settings, etc...) so they persist across reboots.
If we were entirely read-only all the time, then users would have to reenter
all their settings everytime their system booted.

Thus we strive to have only one place where code may be executed: the read-only
rootfs that is cryptographically verified at all times.
All other paths on the system are mounted with `noexec` settings.

Now if an attacker downloads programs (even as root!) to writable locations like
`/var` or `/tmp` or `/home`, any attempts to run that code will be rejected.

### What about interpreted code?

So far we've talked about the scenario where the kernel executes the code.
In other words, it has an executable binary format (a.k.a. binfmt) handler that
is responsible for parsing & executing things directly.
This applies to [ELF] files, but what about interpreted code (a.k.a. scripts)?

While the kernel will process the [shebang] in scripts and enforce the noexec
setting correctly for them, that's where it stops.

So executing scripts directly fail (which is great!):

```sh
# printf '#!/bin/sh\necho hi\n' > /var/test.sh
# chmod a+rx /var/test.sh
# /var/test.sh
-bash: /var/test.sh: Permission denied
```

But executing scripts indirectly still works (which is bad!):

```sh
# sh /var/test.sh
hi
```

This scenario plays out for all programs that accept dynamic code at runtime.
So not only shell scripts, but also awk, Python, Perl, etc...

Which means in ChromeOS, we're somewhat back where we started: attackers are
able to download arbitrary shell scripts to a writable location and then run the
right language interpreter against it.

### What protection do interpreters have to offer?

The behavior in all shells today is as we describe above -- they will gladly
execute any scripts given to them regardless of where they live.
This isn't really a bug for them, it's simply not a use case they care about.

### How does ChromeOS handle this in general?

We start off by removing as many interpreters as possible from the OS.
By default, we block attempts to install packages like Python and Perl.

For many tools (e.g. [sed] and [mawk]), we enable their sandbox mode at build
time which allows us to keep them on the system and execute arbitrary scripts,
but only as pipeline tools: there is no support in the language for running
arbitrary programs or opening files on the system for reading/writing.
Basically all they can do is read stdin, transform the stream, and then write it
back out to stdout.

We recognize that disallowing shell scripts entirely is a significant hurdle to
overall development velocity.
We also have a bit of significant legacy code in the system which is still
written in shell code.
Thus we have policies like "shell scripts are OK for small/trivial uses, but if
it grows too large, it needs to be rewritten in a proper compiled language".

### How does ChromeOS handle shell scripts specifically?

Now we get to the heart of the matter :).
ChromeOS patches [dash] and [bash] to detect the origin of the code at runtime
before it will actually execute it.
If it detects the shell script lives on a noexec partition, it will abort!
So now we get closer parity to the rest of the system.

If we revisit one of our earlier examples that passed:

```sh
# sh /var/test.sh
sh: 0: Refusing to exec /var/test.sh
```

### But what about {XXX} edge case?

We're well aware that the current implementation is not foolproof.
It will correctly detect & catch attempts at direct execution, but it doesn't
detect indirect runtime evaluation.

For example, this still works:

```sh
# sh -c "$(cat /var/test.sh)"
hi
```

However, our goal with these changes isn't necessarily to be bulletproof
(although we will expand and catch more scenarios when feasible), but to catch
a lot of common and accidental mistakes.
So the fact that we don't detect 100% of every bad usage is not a good argument
for never detecting any misuse.

Keep in mind that not all (maybe not even most?) developers are experts when it
comes to writing shell code and possible implications of, what appears to be,
fairly harmless usage in any other system.

For example, developers are used to writing things like:

```sh
#!/bin/sh
# A boot script that helps initialize the device.

# Load the state from our last run.
. "/var/lib/foo/previous-settings"

# Change runtime behavior based on our program's specific settings.
echo "${A_PREVIOUS_SETTING}"
...do more...

# Save our state for next boot.
cat <<EOF >"/var/lib/foo/previous-settings"
A_PREVIOUS_SETTING="${A_PREVIOUS_SETTING}"
SOMETHING="${SOMETHING}"
EOF
```

Can you see anything wrong with this?
The answer is that this opens the system to persistent exploitation, and often a
persistent root exploit if the shell script is a service run during boot.
If an attacker managed to write a plain text file to that path, then that code
would fully execute as complete shell script!
Nothing in the `.` command (which is the same `source`) says that the file may
only contain variables.
It could just as easily be `exec /bin/sh /var/bad-script.sh` which means the
rest of our boot script would never execute!

Before you wonder, yes, developers have attempted to write code exactly like
this in ChromeOS and ship it to all our users.
The author simply had no idea that the shell code could be so powerful and
dangerous.
Thankfully, these get caught during code review, but that isn't guaranteed.

## How do I fix my code?

Now that we've covered the background, let's get into common scenarios that
developers will likely run into.

*** aside
It might be a bit heavy handed, but if you're running into problems with running
shell code from a noexec partition, then chances are good that your shell script
is already at the point where you shouldn't be using shell.
Please strongly consider switching languages, especially due to the fact that it
is rarely feasible to unittest shell scripts.
All code in ChromeOS should have proper unittest coverage so we can confidently
ship a stable & reliable system to our millions of users.
***

### How to save/restore settings?

For people who want to save & restore a set of "simple" variables (usually ones
with values like "0" or "1" or "true" or "false" or similar), there are a few
different possibilities.

#### Use separate files.

Instead of writing a key/value store to a shell script, use the filesystem as
your key/value store.
After all, filesystems are "just" databases!
This works when you have a small/limited number of settings.

So instead of a file saved at `/var/lib/foo/settings` like:

```
VAR=1
FOO=yes
```

Split them up into separate files:

* `/var/lib/foo/settings/VAR` will have the content `1`
* `/var/lib/foo/settings/FOO` will have the content `yes`

You can then read/write them as needed:

```sh
# Load the value and ignore errors if it doesn't exist.
VAR="$(cat /var/lib/foo/settings/VAR 2>/dev/null || :)"

# Save the value later on.
printf '%s' "${VAR}" >"/var/lib/foo/settings/VAR"
```

### How to run code in dev mode?

Dev mode is where users take their device and put it into a mode where they can
get full access to their device (i.e. unlock it).
In this case, it's common for developers to write their own personal scripts to
noexec paths and then try to directly run them.
This will no longer work.

However, in dev mode, ChromeOS already guarantees that `/usr/local` will be
created for users to do whatever they want.
This includes mounting it as executable.
So copy all your shell scripts there and run them directly without problems.

We also add `/usr/local/bin` to the shell's default `$PATH`, so you can put your
custom scripts there and execute them without having to use a full path.

```sh
# printf '#!/bin/sh\necho hi\n' > /usr/local/bin/test.sh
# chmod a+rx /usr/local/bin/test.sh
# which test.sh
/usr/local/bin/test.sh
# test.sh
hi
```

### How to run code in dev or test images?

The answer is the same as dev mode -- use `/usr/local` for all arbitrary code.

Historically we would would remount `/home` and `/tmp` as executable in test
images, but that must no longer be relied upon.
It creates a test system that does not match the behavior of the code that we
ship to all our users!

### How to run crouton?

[crouton] is affected in the same way as any other script. The [crouton README]
has been updated to detail the new recommended steps.


[dash]: https://en.wikipedia.org/wiki/Debian_Almquist_shell
[bash]: https://www.gnu.org/software/bash/
[ELF]: https://en.wikipedia.org/wiki/Executable_and_Linkable_Format
[exploit chain]: https://www.csoonline.com/article/3645449/exploit-chains-explained-how-and-why-attackers-target-multiple-vulnerabilities.html
[mawk]: https://invisible-island.net/mawk/
[sed]: https://www.gnu.org/software/sed/
[shebang]: https://en.wikipedia.org/wiki/Shebang_(Unix)
[crouton]: https://github.com/dnschneid/crouton
[crouton readme]: https://github.com/dnschneid/crouton/blob/HEAD/README.md
