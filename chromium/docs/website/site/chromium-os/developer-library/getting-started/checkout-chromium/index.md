---
breadcrumbs:
- - /chromium-os/developer-library/getting-started
  - ChromiumOS > Getting Started
page_name: checkout-chromium
title: Checkout Chromium
---

Some development tasks may require working in the `chromium` repository, the
`chromiumos` repository, or both. If you only need to work in the `chromiumos`
repository, you can jump ahead to the [Checkout
ChromiumOS](/chromium-os/developer-library/getting-started/checkout-chromiumos)
page.

This page details the steps to create the initial checkout of the `chromium`
repository, including configuring the checkout and providing authentication if
you are syncing from the internal repository.

## Create a directory to host the Chromium checkout

```
$ mkdir chromium && cd chromium
```

All commands relevant to the Chromium checkout from here assume the present
working directory is the directory you created above and have this prefix:

```
chromium$ <command>
```

## Checkout the `chromium` repository

`fetch` is a binary provided by depot_tools. `--nohooks` tells fetch not to run
any post-fetch executions steps which are not necessary at this stage. This step
can take around 30 minutes depending on your connection speed.

```
chromium$ fetch --nohooks chromium
```

## Checkout the `chrome-internal` repository

If you have access to the `chrome-internal` repository (e.g., Googlers and
other authorized partners) in order to build a ChromeOS-branded binary, follow
the steps below to configure the checkout and provide authentication.

### Configure gclient args

<a href="https://www.chromium.org/developers/how-tos/depottools/#gclient"
target="_blank">gclient</a> is the tool used to download code and resources from
the various repositories (e.g., the <a
href="https://chromium.googlesource.com/chromium/src.git"
target="_blank">chromium</a> Git repository) necessary to build ChromiumOS.
The `.gclient` configuration file provides variables which affect which
repositories are synced and how they are set up.

To configure your gclient checkout to include the `chrome-internal` repository,
edit your `chromium/.gclient` file to include the `checkout_src_internal`
variable. The `cros_boards` variable pulls down board-specific code. Finally,
the `target_os` variable pulls down ChromeOS-specific code.

```
solutions = [
  {
    "url": "https://chromium.googlesource.com/chromium/src.git",
    "managed": False,
    "name": "src",
    "custom_deps": {},
    "custom_vars": {
      "checkout_src_internal": True,
      "cros_boards": "eve:puff:scarlet",
    },
  },
]
target_os = ['chromeos']
```

Note that in this example we've specified the boards `eve`, `puff`, and
`scarlet` separated by colons. Whenever you get a new test device be sure to add
or set the board name here and re-sync.

### Authenticate to the internal repository

Navigate to the <a href="https://chrome-internal-review.googlesource.com/"
target="_blank">Chrome internal review page</a> and login **with your
access-enabled account** (@google.com for Googlers). Then authenticate to the
internal repository by navigating to the <a
href="https://chrome-internal.googlesource.com/new-password"
target="_blank">googlesource.com authentication portal</a>, authenticating
**with your access-enabled account**, and executing the provided command in your
shell. This command creates an authentication cookie for Git such that you do
not need to continually provide authentication to access these resources.

### Authenticate to CIPD

<a
href="https://chromium.googlesource.com/chromium/src/+/89.0.4389.114/docs/cipd.md"
target="_blank">CIPD</a> (Chrome Infrastructure Package Deployment) is a
repository which hosts versioned binaries. The ChromeOS source checkout includes
several dependencies hosted on CIPD. Authenticate to CIPD using the following
command (provided by depot_tools):

```
$ cipd auth-login
```

## Initial `gclient sync`

`gclient sync` downloads the external dependencies that chromium (public and
internal) need but don't host (e.g., external libraries like Abseil). These
external dependencies are hosted in <a
href="https://source.chromium.org/chromium/chromium/src/+/main:third_party/"
target="_blank">third_party/</a>.

The first `gclient sync` command you'll run uses the `--nohooks` flag; you
shouldn't need this flag for subsequent uses of gclient sync. You'll need to run
`gclient sync` again each time you pull down from the chromium main git branch
(because new dependencies will have been introduced). Read more about this
subsequent `gclient sync` usage in the (Development workflow with git page -
TODO(jhawkins)).

```
chromium$ gclient sync --nohooks
```

The initial sync of a checkout can take a significant amount of time (up to an
hour).

## Up next

Now that you have the code, the next step is to
[build](/chromium-os/developer-library/getting-started/build-chromium) the
Chromium binary.

<div style="text-align: center; margin: 3rem 0 1rem 0;">
  <div style="margin: 0 1rem; display: inline-block;">
    <span style="margin-right: 0.5rem;"><</span>
    <a href="/chromium-os/developer-library/getting-started/setup-chromebook">Setup Chromebook</a>
  </div>
  <div style="margin: 0 1rem; display: inline-block;">
    <a href="/chromium-os/developer-library/getting-started/build-chromium">Build Chromium</a>
    <span style="margin-left: 0.5rem;">></span>
  </div>
</div>
