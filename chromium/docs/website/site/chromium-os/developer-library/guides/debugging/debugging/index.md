---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: debugging
title: Debugging tips and tricks
---

## Building and deploying Chrome with symbols

**Note: the two following sections,
[Debugging crashes with gdb](#debugging-crashes-with-gdb) and
[Print debugging: stack traces](#print-debugging-stack-traces), require you to
follow this section as a prerequisite.**

### Symbolized Chrome: What and why?

Crash debugging via `gdb` or stack trace printing both require that debugging
symbols be packaged with the executable you are trying to debug. However, the
chrome executable on the Chromebook usually has its debugging symbols stripped
by `deploy_chrome`, because symbols significantly increase the binary size.
There are two ways to deploy chrome with symbols:

*   Using `--nostrip` to include all symbols.
    *   Required for using `gdb`, also works for printing traces in code.
    *   Increases the binary size to >1GB
    *   This is too large for the rootfs and must be deployed to the user
        partition.
    *   Slows deploy times and slows down the system, particularly on low-end
        devices.
    *   Does not survive a reboot of the test device.
*   Using `--strip-flags=-S` to only keep required symbols.
    *   Works for printing stack traces, but does not include enough info for
        `gdb`.
    *   Small enough to fit on rootfs.

### Building Chrome with symbols

In your gn args, ensure you have the following arguments set:

```shell
$ gn args out_${SDK_BOARD}/Release/
```

```
is_chrome_branded = true
is_official_build = false
exclude_unwind_tables = false
enable_profiling = true
symbol_level = 1
```

And then run the usual [build command](developing.md#building-chrome):

```shell
$ autoninja -C out_${SDK_BOARD}/Release chrome
```

Note: You can set `symbol_level=2` for more detailed debugging and
`dcheck_always_on=true` to test [DCHECKs](developing.md#turn-onoff-dchecks).

Note: Here is more information on
[gn configurations](https://www.chromium.org/developers/gn-build-configuration)
and symbolized builds.

### Deploying Chrome with symbols

#### Full symbols (recommended)

##### Step 0. Create another directory for the larger binary

If this is the first time you're deploying symbolized Chrome to your DUT (or
perhaps you just powerwashed it), create the following directory on the DUT:

```shell
$ ssh root@$MYDEVICE mkdir /usr/local/chrome
```

##### Step 1. Deploying

Deploy chrome from your work machine with new arguments (this will take fairly
long because you're deploying a larger binary):

```shell
$ ./third_party/chromite/bin/deploy_chrome \
    --build-dir=out_${SDK_BOARD}/Release \
    --device=${MYDEVICE} \
    --target-dir=/usr/local/chrome \
    --mount-dir=/opt/google/chrome \
    --nostrip \
    --compress=always # optional, but recommended
```

Note: If the Chromebook is powered down after being deployed to
`/usr/local/chrome`, the symbolic link to that directory is destroyed -- once
the Chromebook powers back on, it will load whichever Chrome executable was
there before in `/opt/google/chrome`. This can easily cause you trouble if you
forget about it. Be aware!

#### Limited symbols (usually not recommended)

Instead of `--nostrip`, you can optionally use `--strip-flags=-S`. This will
result in a smaller binary with limited symbols (therefore faster to deploy).

This binary will not contain enough symbols for `gdb`, but will contain enough
symbols for printing stack traces.

The resulting binary can generally fit on the DUT's `rootfs`, allowing you to
skip creating the `/usr/local/chrome` directory on the DUT and linking to it via
the `--target-dir` and `--mount-dir` flags. However, if you hit an error where
your DUT is out of memory and unable to deploy, please instead follow the "Full
symbols" instructions.

Deploy chrome with these arguments in order to deploy with limited symbols:

```shell
./third_party/chromite/bin/deploy_chrome \
    --build-dir=out_${SDK_BOARD}/Release \
    --device=${MYDEVICE} \
    --strip-flags=-S \
    --compress=always # optional, but recommended
```

## Debugging crashes with `gdb`

Note: For a codelab on debugging crashes in ChromeOS, see go/cros-crash-codelab.

Note: Check out the Telemetry team's
[CrOS Crash Playbook](http://go/cros-crash-playbook). It's an excellent resource
for debugging crashes.

### Intercepting crashes in real time

The most straightforward way to understand crashes is to catch them as they
happen, and inspect them, with `gdb`. It’s okay if you have no background with
`gdb`, as the following steps explain how to set it up and its basic usage.

**Once you have deployed a symbolized Chrome executable to your Chromebook (see
section
[Building and deploying Chrome with symbols](#building-and-deploying-chrome-with-symbols)
above)**, `ssh` into your Chromebook, ensure that chrome is running (it should
be after using `deploy_chrome`), and start `gdb`:

```shell
$ gdb --pid=$(pgrep chrome$ -P $(pgrep session_manager))
```

This starts `gdb`, attaching it to the chrome process. It can take several
seconds to start `gdb`, and you may see a lot of errors like:

`warning: Could not find DWO CU obj/third_party/…`

Ignore these warnings and wait for `gdb` to prompt you for instruction. At this
point you will notice the Chrome UI is frozen; `gdb` has attached to the chrome
process and halted it.

Type `help` in the `gdb` prompt to see common commands. Most of the time, you’ll
just want to type:

```shell
(gdb)$ continue
```

or just

```shell
(gdb)$ c
```

to continue program execution.

`gdb` will halt the program again on a crash. At this point, you can view the
stack trace with:

```shell
(gdb)$ backtrace
```

or

```shell
(gdb)$ bt
```

This will provide you a symbolized stack trace like:

```
#0  __GI_raise (sig=sig@entry=6) at ../sysdeps/unix/sysv/linux/raise.c:49
#1  0x00007c8c77a264d9 in __GI_abort () at abort.c:79
#2  0x00005be78ab76bd5 in base::debug::BreakDebuggerAsyncSafe() ()
#3  0x00005be7851efddf in logging::LogMessage::~LogMessage() ()
#4  0x00005be78aad670e in logging::LogMessage::~LogMessage() ()
#5  0x00005be78e7e6503 in proximity_auth::MessengerImpl::HandleMessage(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&) ()
#6  0x00005be7879c6497 in chromeos::secure_channel::ClientChannel::NotifyMessageReceived(std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char> > const&) ()
#7  0x00005be7879cf957 in chromeos::secure_channel::mojom::MessageReceiverStubDispatch::Accept(chromeos::secure_channel::mojom::MessageReceiver*, mojo::Message*) ()
#8  0x00005be7854e5e53 in mojo::InterfaceEndpointClient::HandleIncomingMessageThunk::Accept(mojo::Message*) ()
#9  0x00005be7853493af in mojo::MessageDispatcher::Accept(mojo::Message*) ()
#10 0x00005be7853491f5 in mojo::InterfaceEndpointClient::HandleIncomingMessage(mojo::Message*) ()
```

If you don't see clear names and method signatures like this, you did not
correctly symbolize Chrome. Go repeat the steps above, and keep an eye out for
the pitfalls mentioned in the Notes.

With these signature names, you can now begin to track down the source of the
crash. Reach out to your TL or the wider team for help diagnosing crashes after
this point.

### Debugging a crash after it happens

You may not always be able to have `gdb` attached to Chrome when a crash occurs.
For example:

*   After flipping a feature flag on `chrome://flags` and pressing "Restart
    Chrome", you may notice the message "Flags are ignored because of too many
    consecutive startup crashes." (This means that your build hit a fatal error
    when running with that flag arrangement, such as a crash, CHECK or DCHECK).
    Using `gdb` as described above isn't feasible because it's the *next* Chrome
    process instance that crashed, not the one you flipped the flag in.

*   Some crashes occur as soon as the Chrome process starts, e.g., in the login
    screen after boot or in OOBE. Unless you're superhuman, you're not going to
    be able to issue a `gdb` interrupt between Chrome process start and Chrome
    crash.

*   Some crashes occur in other processes beside the main Chrome process. You
    can find the `pid` of that process and attach to it, or use the (easier)
    method below.

In all these cases, it's recommended that you diagnose the crash after it
happens, instead of intercepting it.
[See this document for detailed instructions](https://docs.google.com/document/d/1QRrh3ipnW9PWI9bJzqK_y_A_pOfA7otAoKM5rV1mYFo/edit#)
on how to log/grab a core dump and read it with `gdb` to retrieve a stack trace.

## Print debugging: stack traces

It’s often the case that you would like to learn where a particular piece of
code is being called from -- but that code could have dozens or even hundreds of
callers. Instead of tediously auditing each of them, you can immediately
pinpoint the original source of a call by using StackTrace.

Sample snippet:

```c++
// foo.cc
#include "base/debug/stack_trace.h"

void functionOfInterest() {
  base::debug::StackTrace().Print();
}

void anotherFunction() {
  LOG(ERROR) << "Unexpected path: " <<  base::debug::StackTrace().ToString();
}
```

Your symbolized stack traces should be viewable in the chrome log, located at
`/var/log/chrome/chrome`.

Note: generating a stack trace does noticeably impact performance of Chrome, so
don’t be shocked to see some mild stuttering when they are generated.

See
[How to get a stack trace at runtime for debugging purposes](https://www.chromium.org/chromium-os/developer-library/guides/debugging/stack-traces)
for more troubleshooting.

## Debugging JS BrowserTest failures

If you’re writing `browser_tests` involving JavaScript or TypeScript and find
yourself getting stuck with debugging, you can use Chrome DevTools.

First, on your workstation or corp laptop, add `-L 9222:localhost:9222` to the
"SSH Arguments" field on the configuration page of the Secure Shell App.

![](images/debugging_ssh.png){style="display:block;margin:auto;width:800px"}
<div align="center">
  Figure 1:
[Add `-L 9222:localhost:9222` to your SSH configuration.](https://screenshot.googleplex.com/BWVDUgHgQckD2so.png)
</div>

Next, run your `browser_tests` using the following command:

```shell
$ testing/xvfb.py ./out/Default/browser_tests --gtest_filter=YourTestName* \
                              --enable-pixel-output-in-tests \
                              --use-gpu-in-tests \
                              --wait-for-debugger-webui \
                              --ui-test-action-timeout=10000000 \
                              --remote-debugging-port=9222
```

Note: testing/xvfb.py will disable the virtual desktop from appearing. To enable
the virtual desktop in the browser debugger, please ensure that the
`--enable-pixel-output-in-tests` argument is added to the command.

Once you run the above command, go to your browser and navigate to
**chrome://inspect/**. Find the test process in the **Devices > Remote Target**
section, and click the "inspect" link.

![](images/debugging_chrome_inspect.png){style="display:block;margin:auto;width:800px"}
<div align="center">
  Figure 2:
[Open chrome://inspect and click the "inspect" link for your test process.](https://screenshot.googleplex.com/6zSXiMEuA375Kgq.png)
</div>

This should open a new DevTools window. To run the test, go to the console tab
in the DevTools window and run "go()."

![](images/debugging_devtools.png){style="display:block;margin:auto;width:800px"}
<div align="center">
  Figure 3:
[In the DevTools window, type and run the command "go()" in the Console tab.](https://screenshot.googleplex.com/8rSsYTVXsZ9vXGe.png)
</div>

Additional tips with the debugger:

-   To find your test file, press Ctrl+P from anywhere in DevTools and start
    typing the name of your file. When you find it, press enter to open it in
    the Sources tab.
-   Click on the line number of your test code to create a breakpoint.
    (Alternatively, add a `debugger;` line where you’d like to break).
-   You can inspect the HTML elements as you run your test, click on "Elements"
    on the top bar and there you can view the HTML file that you’re testing.
-   If you hover your mouse over a JS variable, it’ll give you its value. Or you
    can use the console to query the element.

If running browser_tests over SSH, or you get an error similar to:

`[FATAL:ozone_platform_x11.cc(198)] Check failed: gfx::GetXDisplay(). Missing X
server or $DISPLAY`

Use the testing/xvfb.py script: this allows the tests to run without a real
display attached: `$ testing/xvfb.py out/Default/browser_tests`

## Running DevTools on the lock screen or OOBE

If you’re debugging a problem in a webui surface which doesn’t have dev tools
available, the following steps will allow you to open it.

-   Run the following command on the test device to enable remote debugging.

```shell
$ echo '--remote-debugging-port=9222' >> /etc/chrome_dev.conf
```

-   Reboot the test device.

```shell
$ reboot
```

-   On your workstation or corp laptop, open an SSH tunnel to the DUT:

```shell
$ ssh root@${MYDEVICE} -L 9222:localhost:9222
```

-   Navigate to chrome://inspect, add http://localhost:9222 to network targets
    and start debugging! See more details about the debugging tool
    [here](https://docs.google.com/document/d/1SzaQPZgwb-SmOnUcBvRICJut_Zj85_sva_skrTMsM5U/edit?usp=sharing).

To enter OOBE on a test device that's already completed OOBE.

-   Run the following command on the test device

```shell
stop ui
rm -fr /home/.shadow/* /var/lib/whitelist/* /home/chronos/Local\State
crossystem clear_tpm_owner_request=1
reboot
```

To simulate OOBE on Chrome-Linux build

-   Add the following command line options when running the Chrome binary.

```shell
./out/Default/chrome --login-manager --remote-debugging-port=9223 \
--user-data-dir=/tmp/foo
```

## Bisecting

You may during the course of working on one thing discover that another behavior
of ChromeOS has regressed. You should of course file a P1 bug immediately, but
what next? Usually the easiest and most mechanical way of tracking down the
source of a regression is bisection.

Before you begin bisection:

-   Be certain that your reproduction of the regression is deterministic and can
    be reproduced 100% of the time (or at least be determined to be present with
    a high level of certainty).
-   Determine if the regression is in Chrome/chromium, or in chromiumos (AKA the
    platform).
-   Determine the last known version of chromium or chromiumos where the
    regression was not present (the "last known good").

### Bisecting chromium

It’s very easy and mechanical to bisect chromium: we can use `git`’s bisect
tool.
[Detailed documentation about git bisect is found here](https://git-scm.com/docs/git-bisect),
but the following notes will walk you through the essential commands of the
usual process.

Assuming the regression is present on ToT (tip of tree, most recent commit of
main) and you know the Commit SHA-1 Hash of the "last known good" commit, you
set up the bisect process like so:

```shell
$ git checkout main && git pull origin main
```

```shell
$ git bisect start
```

Mark ToT as "first known bad:"

```shell
$ git bisect bad
```

```shell
$ git bisect good <Commit Hash of "last known good">
```

Git will choose a commit in between these two, at which point you need to tell
git if the regression is present at that commit or not. Usually this requires
building and inspecting Chrome, so that process will look like this:

Sync needed since we’ve moved through the commit history:

```shell
$ gclient sync
```

```shell
$ autoninja -C out_${SDK_BOARD}/Release chrome && \
   ./third_party/chromite/bin/deploy_chrome \
   --build-dir=out_${SDK_BOARD}/Release --device=${MYDEVICE} \
   --compress=always # optional, but recommended
```

**[check if the regression is present]**

Mark bad if regression is present and good if regression is not present

```shell
$ git bisect {good|bad}
```

Repeat these steps until git narrows down and finds the culprit CL.

When you’re done bisecting, be sure to mark as such:

```shell
$ git bisect reset
```

### Bisecting chromiumos (platform)

Bisecting chromiumos is unfortunately much more complicated than bisecting
chromium; the chromiumos project is composed of hundreds of git repos, so using
git bisect is not possible.
[Current solutions are discussed in this group thread](https://groups.google.com/a/google.com/forum/?utm_medium=email&utm_source=footer#!msg/chromeos-chatty-eng/yZw1FuQeqA4/eUtvdCOOAAAJ).

## Debugging Prefs

### Profile Prefs

To inspect the values of profile-based prefs visit `chrome://prefs-internals`.
This will show the currently signed-in user's prefs as one giant JSON file.

### Local State

To inspect the values of local state prefs, visit `chrome://local-state`. By
default, most pref values are filtered out on ChromeOS, but you can flip the
value of the `ENABLE_FILTERING` macro at the top of
[components/local_state/local_state_utils.cc](https://source.chromium.org/chromium/chromium/src/+/main:components/local_state/local_state_utils.cc)
to remove this filtering on your own builds.

## Debugging cross-device features

You can view logs for the cross-device features (Instant Tethering, Smart Lock,
or Android Messages integration) at chrome://proximity-auth.

You can view logs for Nearby Share and other features that use the Nearby
Connections library by visiting chrome://nearby-internals. Reach out to the
Cross-device team for more information.

## Debugging Bluetooth

These sections might not be useful to everyone; they are particularly useful
when working on Bluetooth or cross-device features.

### Nordic "nRF Connect" BLE Android app

To verify BLE advertisements and inspect their service data, I recommend the
"nRF Connect for Mobile" Android App, a third-party app available on the play
store. This app can also be used to generate arbitrary advertisements if you're
working on a feature that involves BLE scanning. Reach out to hansberry@ for
help.

### btmgmt and bluetoothd

In the past, there have been issues in the Bluetooth stack with advertisements
correctly registering and unregistering. You can inspect the state of Bluetooth
advertisements using btmgmt on the Chromebook. To check how many advertisement
are registered, run:

```shell
$ btmgmt advinfo
```

To manually clear them, run:

```shell
$ btmgmt clr-adv
```

After doing this, you’ll want to restart the Bluetooth "device", i.e.
bluetoothd. Whenever you restart Bluetooth, you should also always restart
chrome:

```shell
$ restart bluetoothd && restart ui
```

### Collecting Bluetooth Logs

This section describes collecting verbose Bluetooth logs, dbus logs, and btmon
logs. For more information about Bluetooth logs, check out
[this documentation](https://g3doc.corp.google.com/company/teams/chromeos/subteams/platforms/bluetooth/debugging.md#logging).

[Before repro] Setup for collecting logs:

Step 1: Sign into your Chrome account (this is important otherwise the verbose
logging resets)

Step 2: On the DUT terminal, start btmon logs:

```shell
$ btmon -w /var/log/btmon.log
```

Step 3: On your corp laptop, ssh into the DUT

Step 4: Enable verbose kernel and bluez logs:

```shell
$ dbus-send --system --print-reply --dest=org.bluez /org/chromium/Bluetooth org.chromium.Bluetooth.Debug.SetLevels byte:1 byte:1
```
This setting resets on reboot, and needs to be applied in a chrome session,

Step 5: On your corp laptop ssh’d into the DUT, start dbus logs:

```shell
$ dbus-monitor --system > /var/log/dbus.log
```

*Repro the bug.*

[After repro] Collecting logs:

Step 1: In both terminals, control c to stop the btmon and dbus collection

Step 2: In the DUT terminal, use the command `generate_logs`. This packages up
everything under /var/log (including the dbus and btmon logs from above)

```shell
localhost ~ # generate_logs
2023-01-20T17:55:09.188966Z INFO generate_logs: [generate_logs.cc(96)] Gathering logs, please wait
2023-01-20T17:56:11.478898Z WARNING generate_logs: [bus.cc(644)] Bus::SendWithReplyAndBlock took 62289ms to process message: type=method_call, path=/org/chromium/debugd, interface=org.chromium.debugd, member=DumpDebugLogs
2023-01-20T17:56:11.479113Z INFO generate_logs: [generate_logs.cc(100)] Logs saved to /tmp/debug-logs_20230120-095509.tgz
```
Step 3: The terminal will say where the logs are saved and what the logs are
named.Copy them over to your downloads on your DUT.

Your hash will be different, and you can find the hash by going to
`cd /home/user` and `ls` to see what your hash will be. This is using mine:

```shell
$ cp /tmp/debug-logs_20230120-094809.tgz /home/user/278487735245a0c7255572b1704abfeb4b446408/MyFiles/Downloads/debug-logs.tgz
```

Step 4: From your files app, nearby share the tarball to your corp computer to
analyze. See [Bluetooth 101](cros_101.md) for insight on analyzing Bluetooth.
