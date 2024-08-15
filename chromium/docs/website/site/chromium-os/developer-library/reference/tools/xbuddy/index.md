---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: xbuddy
title: xBuddy for Devserver
---

xBuddy (short for "cros buddy", and "x" is a rotated cross) allows you to obtain
ChromiumOS images or other artifacts off Google Storage and reference locally
built images. When used with devserver (or scripts that use it), it can also be
used to update your ChromiumOS machine to any image.

[TOC]

## xBuddy Paths

Each xBuddy path contains four elements in the form of
"location/board/version/image_type". Each of these elements can have different
values such as:

*   **location** (optional) - Where to get the artifact.
    *   `local` (default) - xBuddy will attempt to find the artifact
        locally.
    *   `remote` - xBuddy will search Google storage for artifacts. The exact
        bucket is automatically detected based on the other arguments below.
*   **board** (required) - Mostly self-explanatory e.g. `amd64-generic` or a
    specific builder alias like `amd64-generic-paladin` or
    `amd64-generic-paladin-tryjob`.
*   **version** (optional) - A version string or an alias that maps to a
    version.
    *   `latest` (default) - If location is `local`, the latest locally built
        image for that board. For `remote`, the newest image from stable channel
        on Google Storage.
    *   `latest-{canary|dev|beta|stable}` - Latest build produced for the given
        channel.
    *   `latest-official` - Latest available release image built for that board
        in Google Storage (e.g. `gs://chromeos-image-archive/eve-release`).
    *   `latest-official-{suffix}` - Latest available image with that board
        suffix (can be anything like `pre-cq`, `release`, etc.) in Google
        Storage (e.g. `gs://chromeos-image-archive/eve-pre-cq`).
    *   `latest-{branch-prefix}` - Latest build with given branch prefix
        i.e. RX-Y.Z or RX-Y or just RX -- seems broken, see line below.
    *   `{branch-prefix}` - Latest build with given branch prefix i.e. RX-Y.X or
        RX-Y or just RX.
*   **image_type** (optional) - Any of the artifacts that devserver recognizes.
    *   `test` (default) - A dev image (signed with dev keys) that has been
        modified to make testing even easier.
    *   `dev` - Developer image (signed with dev keys). Like base but with
        additional dev packages.
    *   `base` - A pristine ChromeOS image (signed with dev keys).
    *   `recovery` - A base image (signed with dev keys) that can be flashed to
        a USB stick and used to recover a ChromeOS install.
    *   `signed` - A recovery image that has been signed by Google and can be
        installed through autoupdate.

### Examples

Some valid examples of xBuddy path are:

*   `remote/amd64-generic/latest-canary`
*   `remote/amd64-generic/latest-official`
*   `remote/amd64-generic-paladin/R32-4830.0.0-rc1`
*   `remote/trybot-amd64-generic-paladin/R32-5189.0.0-b100`
*   `amd64-generic/latest`
*   `local/amd64-generic/latest/test`
*   `amd64-generic/latest/test`

### xBuddy Path Overrides

There are abbreviations for some common paths. For example, an empty path is
taken to refer to the latest local build for a board.

The defaults are found in `chromite/lib/xbuddy/xbuddy_config.ini`, and can be
overridden with `shadow_xbuddy_config.ini`.

## xBuddy Interface and Usage

### Devserver RPC: xbuddy

Please see [Using the Devserver] for more details about the devserver.

If there is a devserver running on host ${HOST} and port ${PORT}, stage
artifacts onto it using xBuddy paths by calling "xbuddy". The path following
`xbuddy/` in the call URL is interpreted as an xBuddy path.

The up to date documentation can be found under the help section of devserver's
index, `http://${HOST}:${PORT}/doc/xbuddy`
```bash
http://${HOST}:${PORT}/xbuddy/${XBUDDY_PATH}
```

Or download the artifact by calling:
```bash
wget http://${HOST}:${PORT}/xbuddy/${XBUDDY_PATH} -O chromiumos_test_image.bin
```

To get the directory name that the artifact will be found in:
```bash
http://${HOST}:${PORT}/xbuddy/${XBUDDY_PATH}?return_dir=true
```

### Devserver RPC: xbuddy_list

If there is a devserver running, check which images are currently cached on it
by calling `xbuddy_list`:
```bash
http://${HOST}:${PORT}/xbuddy_list
```

### Using cros flash

To use xBuddy path with cros flash:
```bash
cros flash ${DUT_IP} xbuddy://${XBUDDY_PATH}
```

`xbuddy://` is optional.

See [cros flash] page for more details.

### Updating from a DUT: update_engine_client

If there is a devserver running, and you have access to the test device, update
the device by calling `update_engine_client` with an `omaha_url` that contains
an xBuddy path.  The `omaha_url` is a call to devserver's update
RPC. Documentation found under the help section of devserver's
index. `http://${HOST}:${PORT}/doc/update`

To update the image to the latest stable:
```bash
update_engine_client --update --omaha_url=${HOST}:${PORT}/update/
```

To update to a specific xBuddy artifact:
```bash
update_engine_client --update --omaha_url=${HOST}:${PORT}/update/xbuddy/{XBUDDY_PATH}
```

See [update engine] page for more details.

[cros flash]: /chromium-os/developer-library/reference/tools/cros-flash/
[Using The Devserver]: https://chromium.googlesource.com/chromiumos/chromite/+/HEAD/docs/devserver.md
[update engine]: https://chromium.googlesource.com/aosp/platform/system/update_engine/
