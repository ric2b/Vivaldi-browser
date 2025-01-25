---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: build-broken
title: Build broken
---

[TOC]

Help! Chrome no longer builds or works!?

## Chrome no longer builds?

There are many reasons why Chrome may no longer build, some more concrete than
others, and deciding which approach to take towards finding the root cause is at
the developer's discretion.

### Broken ToT?

One potential issue is that tip-of-trunk could be broken. Failures can be broad,
but they can also only affect a subset of builds. A reasonable approach for
debugging an issue is as follows:

*   Check the [tree status](https://chromiumos-status.appspot.com/).
    *   This is a great sanity check of the current tip-of-trunk status.
        However, just because the tree status is okay does not mean there are
        not tip-of-trunk issues.
*   Rule out local changes by syncing and building on a fresh branch.
*   Rule out external changes by checking if any related files were modified in
    a recent change.
    *   This step can be difficult since it might not always be clear what files
        are related.
*   Check if the issue can be reproduced by another person.
    *   If a colleague can reproduce the same issue it is very unlikely to be a
        local problem.

After working through the steps above, if you still believe that tip-of-trunk is
broken, you should start by opening a ticket on [crbug/](http://crbug/) that
describes the issue and known affected builds. Be sure to include the culprit
change in the ticket description if applicable and CC the current
[ChromeOS sheriffs](http://go/chromecals). For example, see
[this ticket](http://crbug/1221630).

#### If the culprit change is known

The culprit change should be reverted. This can be done by the submitter of the
change, by the sheriff, or by yourself. Depending on the availability of the
submitter it may make sense for you to revert the change yourself; take a look
at the
[existing documentation](http://go/cros-sheriff-ref#how-can-i-revert-a-commit)
on this, and feel free to reach out to the sheriff for help or guidance.

Finally, once the culprit change is reverted the failures should be verified as
fixed, and the ticket you opened should be marked as verified.

#### If the culprit change is not known

Try to identify an owner of the affected code and see if they can take the
ticket or point you in the right direction. If it is not clear who the owner
would be, work with the sheriff to figure out the next steps.

### Broken build configuration?

It is unlikely but possible that the build configuration has become broken for
an unclear reason, most likely after pulling from `origin/main`. This can result
in failures to build Chrome, and can be difficult to debug. The easiest solution
will be to nuke your configuration and regenerate it.

You can do this by simply deleting your build directory, regenerating your build
configuration, and re-running (auto)ninja. Finally, you will likely need to
`gclient sync` once more. Reference the
[Building Chrome](/chromium-os/developer-library/getting-started) section for more details around
these steps.

## Chrome builds and deploys, but I see a black screen or frozen “G” logo on the Chromebook?

There are a few debugging steps you can take:

1.  Check if you’ve deployed a build with DCHECKs on. It may be fatally quitting
    on failed DCHECKs.
2.  You should still be able to SSH into the machine when in this state. Consult
    [Viewing logs](/chromium-os/developer-library/guides/logging/logging) -- investigate logs and
    error messages to determine root cause.
    1. If you can't still SSH into the machine, it may be because its IP
       address has changed. In this case, you can check the DUT's IP address
       using the command `$ ifconfig` in the VT2 terminal.
    2. If you are still unable to SSH into the machine, flash a new image from
       go/goldeneye and a flashdrive. See
       [Setting up your Chromebook](/chromium-os/developer-library/getting-started) for details.
3.  Make sure your platform version isn't too old. Sometimes updating your
    platform version to `latest-dev` can pull in fixes and keep things inline
    with your Chrome build. See [Flashing ChromiumOS](/chromium-os/developer-library/guides/device/flashing-chromiumos)
    for details.
4.  This may happen if the args.gn file has incorrect arguments; it may have
    manually edited or created. Cleaning your `out*` folder is good way to fix
    this: use the command `$ gn clean out/Default` (or specify a different
    `out*` folder).
5.  If all else fails, read through or write to chat groups or email groups
    (e.g., g/chromeos-chatty-eng).

## `deploy-chrome` error: "cannot remount"

If you encounter an error like

```
mount: /: cannot remount /dev/mmcblk0p3 read-write, is write-protected.
```

when running `deploy-chrome`, your Chromebook screen is likely black, but you
should still be able to get into its terminal. Try the following:

1.  SSH into the Chromebook.
2.  Run `e2fsck` using the path provided in the error message. For example,

    ```
    e2fsck /dev/mmcblk0p3
    ```

3.  Answer "y" to the prompts.

4.  Reboot the Chromebook with `reboot`.

5.  Attempt to deploy again.
