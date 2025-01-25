---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: subtools-builder
title: SDK Subtools Builder
---

The SDK Subtools Builder creates [CIPD] packages from Portage packages which can
be used outside of the SDK.  This is useful for a variety of applications, such
as pre-upload hooks, distributing developer tools, and specialized workflows
like signing.

Currently available subtools packages are visible in the [CIPD Web UI].

[CIPD]: https://chromium.googlesource.com/infra/luci/luci-go/+/HEAD/cipd/
[CIPD Web UI]: https://chrome-infra-packages.appspot.com/p/chromiumos/infra/tools

## Adding a package to the subtools builder

You'll need to write a config for the package.  The config is in
[textproto format] and [`subtools.proto`] is the schema for it.  Here is a
simple example config:

```protocol-buffer-text-format
name: "futility"
type: EXPORT_CIPD
max_files: 200
paths: [
  {
    input: "/usr/bin/futility"
  }
]
```

The config can be placed anywhere that you like, as long as it's accessible
during the ebuild `src_install` phase (which means, you can even generate the
config with a script, should you need).  Common locations are in `${FILESDIR}`
of the ebuild, or in your project's source tree.

To install the config, make sure your ebuild inherits `cros-subtool`, and add to
your `src_install`:

```gentoo-ebuild
src_install() {
	...
	cros-subtool_src_install path/to/subtool.textproto
}
```

Finally, you need to tell the subtools builder to build your package.  List it
in [`virtual/target-sdk-subtools`].

After landing your changes, your package will be available in CIPD after the
next [`build-chromeos-sdk-subtools`] run (kicked off every 24 hours).

[textproto format]: https://protobuf.dev/reference/protobuf/textformat-spec/
[`subtools.proto`]: https://chromium.googlesource.com/chromiumos/config/+/HEAD/proto/chromiumos/build/api/subtools.proto
[`virtual/target-sdk-subtools`]: https://chromium.googlesource.com/chromiumos/overlays/chromiumos-overlay/+/HEAD/virtual/target-sdk-subtools/target-sdk-subtools-9999.ebuild
[`build-chromeos-sdk-subtools`]: https://luci-scheduler.appspot.com/jobs/chromeos/build-chromeos-sdk-subtools

## Running the subtools builder locally

To test your subtools changes, you can run the subtools builder locally.  To do
so, execute:

```shellsession
(outside) ~/chromiumos $ chromite/bin/build_sdk_subtools
```

This tool will create its own specialized chroot for building in, so it should
be run *outside* the SDK.

## How it works

Internally, the subtools builder uses [`lddtree`] to bundle both your package
and its shared library dependencies (e.g., `libc`), replacing binaries with
wrapper scripts using the correct `ld-linux.so` to load your programs.  This
creates a portable archive that should work on most any `x86_64` linux
distribution without depending on system libraries.

[`lddtree`]: https://github.com/gentoo/pax-utils/blob/HEAD/lddtree.py
