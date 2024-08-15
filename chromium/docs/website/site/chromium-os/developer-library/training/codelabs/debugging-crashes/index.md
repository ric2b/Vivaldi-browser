---
breadcrumbs:
- - /chromium-os/developer-library/training
  - ChromiumOS > Training
page_name: debugging-crashes
title: Debugging Crashes
---

This codelab will walk you through how to debug a crash in Chromium. First we
create the crash, then walk through where to find logs related to the crash, and
finally how to get more information to determine the root cause (using
symbolized build logs, stack traces, and gdb). At the end of this codelab you
will know the first steps to take anytime you need to debug a crash in Chrome!
If you want more information about debugging chrome, see
[Debugging](/chromium-os/developer-library/guides/debugging#debugging-crashes-with-gdb).

[TOC]

## Getting started

First, let's make sure you are ready to start debugging crashes. Prior to doing
this codelab, you should have completed the following sections:
[Hardware](/chromium-os/developer-library/getting-started/hardware-requirements),
[Chromebook Setup](/chrome/chromeos/system_services_team/dev_instructions/g3doc/setup_chromebook.md),
[Checkout Chromium](/chromium-os/developer-library/getting-started/checkout-chromium),
and
[Building, deploying and testing](/chrome/chromeos/system_services_team/dev_instructions/g3doc/developing.md).
Once you have done all that, you're ready to start at step 1!

## 1. Creating a crash

Crashes can happen for a variety of reasons, some of the most common are segv
(segmentation faults), UAF (use after free) and CHECKKs. Today we will debug a
crash caused by a CHECK but the same principles can be applied to debugging any
crash. There are two primary types of CHECKKs: 1) CHECK: will cause a crash both
in both development and production environments. 2) DCHECK will cause a crash in
development environments but in production it becomes a no-op instead. By
default DCHECKS are enabled in your builds. To create a crash, we will insert a
CHECK into our code and then navigate to the page which runs that code.

1.  Check out a new branch in `chromium/src` called `crash-debugging` using the
    command:

    ```shell
    $ git new-branch crash-debugging
    ```

    For more information on checking out a new branch, see
    [Development workflow with Git](/chrome/chromeos/system_services_team/dev_instructions/g3doc/git_chromium_workflow.md)

2.  Using whatever editor you prefer, navigate to
    `chrome/browser/ui/webui/settings/ash/fast_pair_saved_devices_handler.cc`
    and find the function `HandleLoadSavedDevicePage()`.

3.  Now let's add a CHECK that we know is going to fail. Find the line in
    `HandleLoadSavedDevicePage()` where we set `loading_saved_devices_page =
    true;`, and
    [directly above that line](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/webui/settings/ash/fast_pair_saved_devices_handler.cc;l=101),
    add the line

```
CHECK(loading_saved_device_page_);
```

This will cause a crash when Chrome tries to load the page which happens anytime
the user navigates to the page. So all we need to do to hit the crash, is
navigate to the Saved Devices page which is described in the next section!

## 2. Hitting your crash

Next we will build and deploy your binary without symbols and hit the crash.

1.  Build and deploy your `crash-debugging` branch using the instructions in the
    [Building, deploying and testing](/chrome/chromeos/system_services_team/dev_instructions/g3doc/developing.md).
2.  Log in on your test Chromebook (also referred to as DUT or Device Under
    Test).
3.  Navigate to the Settings Page -> Bluetooth -> Devices saved to my account
    Expected Behavior: The Chromebook will crash when you click on the "Devices
    saved to my account" settings row (see screenshot below).
    ![Navigating to the Saved Devices page](saved_devices.png)
    Figure 1: [Bluetooth Setting Page](saved_devices.png)

## 3. Debugging without symbols

Now let's look at the logs to see what happened! There are two ways to look at
logs.

Option A: on your DUT, open your chrome browser and navigate to
`file:///var/log/chrome/chrome.PREVIOUS`.

Option B: ssh into your DUT using the
[directions here](/chrome/chromeos/system_services_team/dev_instructions/g3doc/developing.md#step-0-set-up-your-chromebook-and-verify-that-you-can-ssh-to-it)
and view the same log using:

```
$ less /var/log/chrome/chrome.PREVIOUS
```

Log statements are recorded sequentially so the oldest logs are at the top and
the most recent ones are at the bottom. Scroll to the bottom of the file (if you
used option B, you can just hit `shift + G` to skip to the bottom). Then scroll
up until you start to see a stack trace. You'll notice that instead of seeing
function names in the stack trace, there are rows like:

```
#0 0x59903e819bb2 (/opt/google/chrome/chrome+0xa6a3bb1)
#1 0x59903e7ff353 (/opt/google/chrome/chrome+0xa689352)
#2 0x59903852280c (/opt/google/chrome/chrome+0x43ac80b)
#3 0x59903e70c3ee (/opt/google/chrome/chrome+0xa5963ed)
#4 0x599044fae6fe (/opt/google/chrome/chrome+0x10e386fd)
#5 0x599037fd3210 (/opt/google/chrome/chrome+0x3e5d20f)
#6 0x59903c62f340 (/opt/google/chrome/chrome+0x84b933f)
#7 0x59903bd59883 (/opt/google/chrome/chrome+0x7be3882)
#8 0x59903830ff4d (/opt/google/chrome/chrome+0x4199f4c)
#9 0x59903806102a (/opt/google/chrome/chrome+0x3eeb029)
#10 0x599038060e31 (/opt/google/chrome/chrome+0x3eeae30)
```

This is because the current binary was built without symbols. Therefore the
stack trace is not going to tell us very much. However, We are still able to see
what was logged. Scroll up to just above the stack trace and you should see the
line:

```
2023-02-15T01:30:25.767403Z FATAL chrome[7261:7261]: [fast_pair_saved_devices_handler.cc(102)] Check failed: loading_saved_device_page_.
```

This tells us that the CHECK we added was what failed and the line number it
failed on, that is vital information when debugging a crash. However, this will
only be shown if the crash was caused by a CHECK or DCHECK.

Note: If you were debugging a real crash like a segfault or a UAF you will need
to catch the crash as it happens with `gdb`. Using `gdb` is covered in step 5.

Step 2 was meant to simulate crashing during development or production, and the
next thing to do is capture a stack trace via debugging with symbols to get more
information.

## 4. Building and Deploying with symbols

Building with symbols takes longer but gives developers significantly more
details for debugging issues. To build and deploy with symbols we'll use the
instructions in
[Debugging](/chromium-os/developer-library/guides/debugging/debugging#debugging-crashes-with-gdb).

To build chromium with symbols follow the instructions on
[Building chrome with symbols](/chromium-os/developer-library/guides/debugging/debugging#building-chrome-with-symbols).
Then to deploy with symbols, follow the instructions on
[Deploying Chrome with symbols](/chromium-os/developer-library/guides/debugging/debugging#deploying-chrome-with-symbols).

## 5. Intercepting and debugging crashes with `gdb`

Generally the most straightforward way to debug all crashes is to catch them as
they happen, and inspect them, with `gdb`. This will provide a stack trace for
all types of crashes whereas looking at logs only shows stack traces for CHECKKs.

Note: If you are debugging a real crash, you'll want to skip straight to using a
symbolized build and debugging with `gdb`. The purpose of this codelab is to
walk through multiple difference circumstances of encountering crashes, but when
investigating a crash **ALWAYS** use a symbolized build.

It’s okay if you have no background with `gdb`, as the following steps explain
how to set it up and its basic usage. For more in-depth information see the
[Debugging](/chromium-os/developer-library/guides/debugging/debugging#debugging-a-crash-after-it-happens).
**Once you have deployed a symbolized Chrome executable to your Chromebook (see
section
[Building and deploying Chrome with symbols](#5-deploying-chrome-with-symbols)
above)**, sign in to your DUT, then using your corp chromebook, `ssh` into your
DUT, and start `gdb` using the following command:

```shell
$ gdb --pid=$(pgrep chrome$ -P $(pgrep session_manager))
```

This starts `gdb`, attaching it to the browser process. It can take several
seconds to start `gdb`, and you may see a lot of errors like:

`warning: Could not find DWO CU obj/third_party/…`

Ignore these warnings and wait for `gdb` to prompt you for instruction. At this
point you will notice the Chrome UI is frozen; `gdb` has attached to the browser
process and halted it.

Type `help` in the `gdb` prompt to see common commands. Most of the time, you’ll
just want to type:

```shell
(gdb)$ continue
```

or

```shell
(gdb)$ c
```

to continue program execution. To see `gdb` in action, once again navigate to
the Saved Devices page, and induce the crash. Notice how `gdb` halts the Chrome
UI on the crash.

```shell
(gdb)$ backtrace
```

or

```shell
(gdb)$ bt
```

This will provide you a symbolized stack trace like:

```
#0 0x59a83a50add2 base::debug::CollectStackTrace()
#1 0x59a83a4efb93 base::debug::StackTrace::StackTrace()
#2 0x59a834069603 logging::LogMessage::~LogMessage()
#3 0x59a83a3f7fce logging::LogMessage::~LogMessage()
#4 0x59a833ac4957 logging::CheckError::~CheckError()
#5 0x59a8419a0eda ash::settings::FastPairSavedDevicesHandler::HandleLoadSavedDevicePage()
#6 0x59a833ad7939 _ZNKR4base17RepeatingCallbackIFvRKN3net9DnsConfigEEE3RunES4_
#7 0x59a83825de37 content::WebUIImpl::Send()
#8 0x59a837972c6b content::mojom::WebUIHostStubDispatch::Accept()
#9 0x59a833e399de mojo::InterfaceEndpointClient::HandleIncomingMessageThunk::Accept()
#10 0x59a833b67e84 mojo::MessageDispatcher::Accept()
#11 0x59a833b67c93 mojo::InterfaceEndpointClient::HandleIncomingMessage()
#12 0x59a834a12ea9 IPC::(anonymous namespace)::ChannelAssociatedGroupController::AcceptOnEndpointThread()
```

If you don't see clear names and method signatures like this, you did not
correctly symbolize Chrome, reach out to your onboarding buddy (or anyone on the
team) for assistance.

## 6. Debugging with symbols using logs

If you know you are dealing with a crash caused by a CHECK, you can also find
the stack trace in the logs of a symbolized build. Now that you have deployed
with symbols, go ahead and follow the
[instructions to hit the crash](#2-hitting-your-crash) and look at the logs
again. You'll notice that the stack trace shows function names now and you can
see (on frame 5) that the crash happened in the `HandleLoadSavedDevicePage()`
function.

Note: Crash stacks are only logged if the crash was caused by a CHECK, other
crashes will not be logged.

```
#0 0x59a83a50add2 base::debug::CollectStackTrace()
#1 0x59a83a4efb93 base::debug::StackTrace::StackTrace()
#2 0x59a834069603 logging::LogMessage::~LogMessage()
#3 0x59a83a3f7fce logging::LogMessage::~LogMessage()
#4 0x59a833ac4957 logging::CheckError::~CheckError()
#5 0x59a8419a0eda ash::settings::FastPairSavedDevicesHandler::HandleLoadSavedDevicePage()
#6 0x59a833ad7939 _ZNKR4base17RepeatingCallbackIFvRKN3net9DnsConfigEEE3RunES4_
#7 0x59a83825de37 content::WebUIImpl::Send()
#8 0x59a837972c6b content::mojom::WebUIHostStubDispatch::Accept()
#9 0x59a833e399de mojo::InterfaceEndpointClient::HandleIncomingMessageThunk::Accept()
#10 0x59a833b67e84 mojo::MessageDispatcher::Accept()
#11 0x59a833b67c93 mojo::InterfaceEndpointClient::HandleIncomingMessage()
#12 0x59a834a12ea9 IPC::(anonymous namespace)::ChannelAssociatedGroupController::AcceptOnEndpointThread()
```

Great job! You now know the basics of debugging a crash multiple ways! Go ahead
and delete this branch. Feel free to come back to this code lab when you are
debugging a real crash!

