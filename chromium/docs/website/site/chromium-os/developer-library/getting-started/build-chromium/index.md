---
breadcrumbs:
- - /chromium-os/developer-library/getting-started
  - ChromiumOS > Getting Started
page_name: build-chromium
title: Build Chromium
---

Once you have a
[checkout](/chromium-os/developer-library/getting-started/checkout-chromium) of
the `chromium` repository, you're now ready to build the Chromium binary.

## Install the build dependencies

Building Chromium requires many dependencies to be installed on the workstation,
including development libraries and tools. Thankfully, getting these
dependencies set up and configured is simplified with a script in the Chromium
source:

```
chromium/src$ build/install-build-deps.sh
```

## Set the `SDK_BOARD` variable

You'll need to find the board name for your test device on <a
href="https://cros-goldeneye.corp.google.com/chromeos/console/listDevice"
target="_blank">Goldeneye</a> (Googlers only). A number of commands use this
board name, so you may wish to store it in an environment variable. For example,
if your device is an eve (a Pixelbook):

```
export SDK_BOARD=eve
```

Make sure that the corresponding board is listed in the `cros_boards` section of
your `chromium/.gclient` file and subsequently `gclient sync` has been run;
otherwise, the import statement below will fail with a file not found exception.
See the ChromiumOS-specific GN args mapping <a
href="https://chromium.googlesource.com/chromium/src/+/main/build/args/chromeos/README.md"
target="_blank">README.md</a> for more information and examples.

## Create a build configuration

Create a build configuration for your board. The following example specifies a
few important build arguments to set:

```
chromium/src$ gn gen out_${SDK_BOARD}/Release --args="
    import(\"//build/args/chromeos/${SDK_BOARD}.gni\")
    is_chrome_branded = true
    is_official_build = false
    optimize_webui = false
    target_os = \"chromeos\"
    use_remoteexec = true"
```

This command creates the file `out_${SDK_BOARD}/Release/args.gn` containing the
contents passed in the `--args` command above. Future invocations of `gn gen`
will use the cached values in `args.gn` so passing the `--args` parameter is no
longer necessary. You can edit the cached arguments by editing
`out_${SDK_BOARD}/Release/args.gn` or running the following command:

```
chromium/src$ gn args out_${SDK_BOARD}/Release
```

More information regarding build configurations can be found <a
href="https://chromium.googlesource.com/website/+/HEAD/site/developers/gn-build-configuration/index.md"
target="_blank">here</a>.

## Building Chrome

You are now ready to build the chrome target:

```
autoninja -C out_${SDK_BOARD}/Release chrome
```

`autoninja` is a tool which passes the best parameters to the build command,
`ninja`, to handle build parallelization based on whether remote execution
(e.g., Reclient) is enabled and available. <a
href="https://chromium.googlesource.com/chromium/src/+/0e94f26e8/docs/ninja_build.md"
target="_blank">ninja</a> is a build system specializing in a fast edit-build
cycle and is the tool used to build Chromium.

### Common build errors

```
ninja: error: loading 'build.ninja': No such file or directory
```

This error cocurs if the `build.ninja` cache file is not present and updated in
the `out_${SDK_BOARD}/Release` directory. This may happen if `gn gen` has not
been run yet or if that command was interrupted before completing. To fix this
error, run `gn gen` again:

```
chromium/src$ gn gen out_${SDK_BOARD}/Release
```

## Up next

Once the `chrome` target successfully builds, you are ready to
[deploy](/chromium-os/developer-library/getting-started/deploy-chromium) the
Chromium binary to the test device.

<div style="text-align: center; margin: 3rem 0 1rem 0;">
  <div style="margin: 0 1rem; display: inline-block;">
    <span style="margin-right: 0.5rem;"><</span>
    <a href="/chromium-os/developer-library/getting-started/checkout-chromium">Checkout Chromium</a>
  </div>
  <div style="margin: 0 1rem; display: inline-block;">
    <a href="/chromium-os/developer-library/getting-started/deploy-chromium">Deploy Chromium</a>
    <span style="margin-left: 0.5rem;">></span>
  </div>
</div>
