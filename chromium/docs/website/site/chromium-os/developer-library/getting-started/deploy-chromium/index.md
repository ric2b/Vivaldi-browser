---
breadcrumbs:
- - /chromium-os/developer-library/getting-started
  - ChromiumOS > Getting Started
page_name: deploy-chromium
title: Deploy Chromium
---

## Step 0: Set up your Chromebook and verify that you can SSH to it

You can only deploy a custom chrome if you finished the steps in the previous
[Setup your development Chromebook](../setup-chromebook) section. Those steps only
need to be completed once for each Chromebook. Please make sure you finished
those steps before continuing on here.

As part of these steps, also ensure that you can successfully `ssh` into your
Chromebook; see previous instructions:
[SSH into your Chromebook](../setup-chromebook#ssh-into-your-chromebook). Recall
that that the password to the Chromebook is `test0000`, and that SSHing can be
performed like so:

### Working at the office

```shell
$ ssh root@${MYDEVICE}
```

### Working from home

```shell
[workstation]$ ssh root@localhost -p 2233
```

## Deploy Chrome to your Chromebook

You can now deploy the Chrome you built with autoninja. Run one of the following
command variations based on whether you are working from the office or from
home:

### Working at the office

```shell
[workstation]$ ./third_party/chromite/bin/deploy_chrome \
    --build-dir=out_${SDK_BOARD}/Release \
    --board=${SDK_BOARD} \
    --device=${MYDEVICE} \
    --compress=always # optional, but recommended to increase deploy speeds
```

### Working from home

```shell
[workstation]$ ./third_party/chromite/bin/deploy_chrome \
                 --build-dir=out_${SDK_BOARD}/Release \
                 --board=${SDK_BOARD} \
                 --device=localhost:2233 \
                 --compress=always # optional, but recommended
```

Note: You may see references later on in this doc to use `deploy_chrome` with
the arg `--device=${MYDEVICE}`. If you're WFH, just replace that with
`--device=localhost:2233`.

### After deploying Chrome

The `deploy_chrome` command deploys Chrome to the Chromebook via SSH, and
restarts Chrome (but the Chromebook won't actually reboot). If the command
fails, it will inform you. If it succeeds, your Chromebook’s screen will flash
as it starts the new Chrome, and then display the login UI.

SSH into the Chromebook afterwards to read logs.

```shell
[DUT]$ cat /var/log/chrome/chrome
```

See the [Logging documentation](../../guides/logging/logging#viewing-logs) for
more log sources to inspect.

## Help! deploy_chrome is complaining that my Chromebook doesn’t have enough space

Some Chromebooks have limited space in a particular part of their memory to
store a larger chrome executable, showing an error of the form:

`Not enough free space on the device. Required: XXX MiB, actual: XXX MiB.`

First, try cleaning your build directory with

```shell
gn clean out_${SDK_BOARD}/Release
```

Then, re-build and re-deploy. If this works, you're good to go.

If this issue was more than just a bloated build directory, you can get around
the storage limitation by deploying chrome to a partition with more space, and
mounting it to the expected location (/opt/google/chrome).

Access your Chromebook’s command line, either via its VT2 terminal or SSH, and
then create a deployment target directory:

```shell
$ mkdir /usr/local/chrome
```

Then from your workstation:

```shell
$ ./third_party/chromite/bin/deploy_chrome \
    --build-dir=out_${SDK_BOARD}/Release \
    --board=${SDK_BOARD} \
    --device=${MYDEVICE} \
    --target-dir=/usr/local/chrome \
    --mount-dir=/opt/google/chrome
```

Note: If the Chromebook is powered down after being deployed to
`/usr/local/chrome`, the symbolic link to that directory is destroyed -- once
the Chromebook powers back on, it will load whichever Chrome executable was
there before in `/opt/google/chrome`. This can easily cause you trouble if you
forget about it. Be aware!

## Up next

The final step in the development process is [testing](../test-chromium) your
changes.

<div style="text-align: center; margin: 3rem 0 1rem 0;">
  <div style="margin: 0 1rem; display: inline-block;">
    <span style="margin-right: 0.5rem;"><</span>
    <a href="/chromium-os/developer-library/getting-started/build-chromium">Build Chromium</a>
  </div>
  <div style="margin: 0 1rem; display: inline-block;">
    <a href="/chromium-os/developer-library/getting-started/test-chromium">Test Chromium</a>
    <span style="margin-left: 0.5rem;">></span>
  </div>
</div>