---
breadcrumbs:
- - /chromium-os/developer-library/getting-started
  - ChromiumOS > Getting Started
page_name: test-chromium
title: Test Chromium
---

[TOC]

## Building and running tests

Create a build configuration for testing (you can use Default for both virtual
ChromeOS on the desktop and for testing):

```shell
~/chromium/src $ gn gen out/Default --args="
    is_chrome_branded = true
    is_official_build = false
    optimize_webui = false
    target_os = \"chromeos\"
    use_remoteexec = true"
```

Some tests may be disabled when running locally in debug mode due to dbg
flakiness or if they're testing functionality that only applies to official
builds like webui optimization. Add `is_debug=false` to gn args to run these
tests.

Use `autotest.py` to run all tests in a file or directory. For example:

```shell
# Run all tests in the bluetooth_socket_apitest.cc file
$ tools/autotest.py -C out/Default --run_all ./extensions/browser/api/bluetooth_socket/bluetooth_socket_apitest.cc

# Run all tests in the bluetooth_socket directory
$ tools/autotest.py -C out/Default --run_all ./extensions/browser/api/bluetooth_socket
```

Alternatively, find the target your test resides in and invoke that directly.
You can most easily do this by plugging your test file path into `gn refs`. For
example, plugging in the test file
`//extensions/browser/api/bluetooth_socket/bluetooth_socket_apitest.cc` like so:

```shell
$ gn refs out/Default //extensions/browser/api/bluetooth_socket/bluetooth_socket_apitest.cc --all --testonly=true --type=executable
```

provides `//extensions:extensions_browsertests`. You can then build this test
target like so:

```shell
$ autoninja -C out/Default/ extensions_browsertests
```

Then, to run all the tests under `extensions_browsertests`, you can run

```shell
$ ./out/Default/extensions_browsertests
```

To specifically run a given test or set of tests, use the --gtest_filter
argument (allows wildcard matching to capture multiple tests):

```shell
$ ./out/Default/extensions_browsertests --gtest_filter=*BluetoothSocket*
```

Or even specify a specific test method:

```shell
$ ./out/Default/extensions_browsertests --gtest_filter=BluetoothSocketApiTest.Listen
```

To enable verbose logging during a test, add the
[appropriate args](/chromium-os/developer-library/guides/logging/logging#enable-verbose-logging)
for the desired module and logging level like so:

```shell
$ ./out/Default/extensions_browsertests --gtest_filter=*BluetoothSocket* --vmodule=*bluetooth*=1,*dbus*=1,*bluez*=2
```

To run browser tests over an SSH connection, an additional script is required to
simulate an attached display for the browser window to draw on.

```shell
$ testing/xvfb.py ./out/Default/extensions_browsertests --gtest_filter=*BluetoothSocket*
```

For help with writing unit tests see:

-   [Googletest Primer](https://github.com/google/googletest/blob/main/docs/primer.md)
-   go/gmockfordummies

For more see:

-   go/chrome-linux-build#running-test-targets
-   https://www.chromium.org/developers/testing/running-tests