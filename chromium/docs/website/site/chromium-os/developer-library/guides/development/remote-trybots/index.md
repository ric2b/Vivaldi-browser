---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: remote-trybots
title: Remote Trybots
---

*** note
**NOTE**: **Sorry, but this tool is only available to Google employees.**
***

Remote trybots allow you to perform a trybot run of multiple configs/boards in
parallel on remote servers. A trybot allows you to emulate a normal build run
with a set of your changes. The changes are applied to tip of tree (ToT).

It supports building both Gerrit CLs as well as changes you've committed
locally but haven't yet uploaded.

See the [remote tryjobs list][Tryjobs List] for a list of running jobs.

[TOC]

## Getting Started

Run the following to see a list of common configs:

```bash
$ cros tryjob --list
```

See the full set of options:

```bash
$ cros tryjob --help
```

*** note
**NOTE**: Googlers, the first time you attempt to start a tryjob, it will
initiate the sign-in flow. If you accidentally use your `@chromium.org` account,
you can run `cipd auth-logout && cipd auth-login` to switch to your
`@google.com` account.
***

## Build a Gerrit CL

```bash
$ cros tryjob -g [*]<cl_1> [-g ...] config [config...]
```

Substitute `[config [config...]]` with a space-separated list of your desired
configs. Prepend a `*` to the CL ID to indicate it's an internal CL. The CL ID
can be a Gerrit Change-ID or a change number.

### Example

```bash
$ cros tryjob -g 4168 -g *1234 caroline-llvm-toolchain-tryjob caroline-release-tryjob
```

This runs a tryjob for the `caroline-llvm-toolchain` and `caroline-release`
configs in parallel. It applies two CL's:

1.  An external CL using a Gerrit change number.
1.  An internal CL using a Gerrit change number.

In the case where a CL has several patches associated with it, the latest
patch is used.

## Build a local change

```bash
$ cros tryjob -p <project1>[:<branch1>] [-p ...] config [config..]
```

Specify the name of the project (not the path, look for `Project:` in
`repo info`) and optionally the project branch. If no branch is specified the
current branch of the project will be used.

### Example

```bash
$ cros tryjob -p chromiumos/chromite -p chromiumos/overlays/chromiumos-overlay:my_branch amd64-generic-unittest-stress
```

This patches in any commits in project chromiumos/chromite's current branch and
on branch `my_branch` in project `chromiumos/overlays/chromiumos-overlay`.

## Testing on a release branch

To test patches for a release branch (i.e., R79, R80) use the `--branch` (`-b`)
option with `-g`:

```bash
(outside) $ ~/chromiumos/chromite/bin/cros tryjob -b release-R79-12607.B -g 1906723 nocturne-release-tryjob
```

***note
**NOTE**: When triggered a release branch, you must run `cros` from a ToT
checkout and not just the local checkout, which is why the above command shows
the full path.
***

## Create an official release with a trybot

The `--production` flag allows triggering an official release build. You should
consult with the [ChromeOS CI Bobby][goldeneye] before using.

Trigger a release build using ToT for the `hatch` board:

```bash
$ cros tryjob --production hatch-release
```

Trigger a release build using the R79 branch (`release-R79-12607.B`) for the
`hatch` board:

```bash
$ cros tryjob --production --branch release-R79-12607.B hatch-release
```

## Finding your trybot runs

When you launch your tryjob, it will print a link showing where to find your
run.

For example:

```bash
$ cros tryjob -g 288223 caroline-release-tryjob
Verifying patches...
Submitting tryjob...

Tryjob submitted!
To view your tryjobs, visit:
  https://ci.chromium.org/b/12345678
```

You can also search for jobs created by a
[specific user (email address)][Tryjobs User].

## Flashing a trybot image

If you want to flash an image built in trybot to your DUT you can use an
xbuddy reference like so:

1. Get the value of "artifact_link" in "Output Properties" of your job.

2. Form the xbuddy reference with part of that value:
   `xbuddy://remote/${ARTIFACT}`.

For example:

```bash
cros flash ${DUT} xbuddy://remote/eve-release-tryjob/R89-13729.34.0-b4804932/test
```

[Gerrit]: https://chromium-review.googlesource.com/
[Tryjobs List]: https://cros-goldeneye.corp.google.com/chromeos/legoland/builderSummary?buildConfig=&builderGroups=tryjob
[Tryjobs User]: https://cros-goldeneye.corp.google.com/chromeos/legoland/builderSummary?buildConfig=&builderGroups=tryjob&email=tomhughes%40chromium.org
[`chromite/config/config_dump.json`]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/config/config_dump.json
[`chromite/config/chromeos_config.py`]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/config/chromeos_config.py
[goldeneye]: http://go/goldeneye
