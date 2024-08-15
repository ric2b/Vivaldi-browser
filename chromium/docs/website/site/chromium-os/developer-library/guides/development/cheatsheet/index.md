---
breadcrumbs:
- - chromium-os/developer-library/guides/development
  - ChromiumOS > Guides > Development
page_name: cheatsheet
title: CrOS work command cheat sheet
---

## Chromium development

[TOC]

### Sync

```shell
$ git pull origin main && gclient sync -D
```

### Build & run chrome locally (not on real hardware)

```shell
$ autoninja -C out/Default chrome sandbox && ./out/Default/chrome \
--user-data-dir=~/.config/cros --login-manager
```

You can choose any name beside `Default` if you desire, but most engineers use
`Default` for any non board-specific work like this.

### Build chrome for a particular board, and deploy to it

```shell
$ autoninja -C out_${BOARD}/Release chrome chrome_sandbox nacl_helper && \
./third_party/chromite/bin/deploy_chrome --build-dir=out_${BOARD}/Release \
--device=${DUT} --nostrip --compress=always
```

### Test

```shell
$ autoninja -C out/Default ${TEST_NAME} && ./out/Default/${TEST_NAME} \
--gtest_filter=${FILTER} --log-level=0
```

## ChromiumOS development

### Enter shell

```shell
$ cd ~/chromiumos/src && cros_sdk
```

### Flash pre-built chromiumos images from goldeneye to your DUT

```shell
(cr)$ cros flash ssh://127.0.0.1:7777 xbuddy://remote/eve/latest-dev \
      --no-ping \
      --send-payload-in-parallel \
      --log-level=info
```

Be sure to use the correct port for your device. Replace `eve` with your board
name and replace `latest-dev` with another channel or a version number such as
`R84-13099.42.0`.

### Package workspace

#### Get a list of packages by running

```shell
(cr)$ cros_workon --board=${BOARD} --all list
```

#### Mark package as active to work on

```shell
(cr)$ cros_workon --board=${BOARD} start ${PACKAGE}
```

Then sync the repo with steps below.

### Sync

```shell
$ repo sync -j8
$ cros build-packages --board=${BOARD}
```

### Build and run

#### Using emerge

```shell
(cr)$ emerge-${BOARD} ${PACKAGE} && cros deploy ${DUT} ${PACKAGE}
```

Replace ${PACKAGE} with the one you are working on e.g. chromeos-base/feedback.

#### Using `cros_workon_make`

```shell
(cr)$ cros_workon_make --board=${BOARD} ${PACKAGE} && \
cros_workon_make --board=${BOARD} ${PACKAGE} --install && \
cros build-image --board=${BOARD} --noenable_rootfs_verification dev
```

Then reimage your test device with instructions in
[flash chromiumOS](/chromium-os/developer-library/guides/device/flashing-chromiumos/).

### Test

```shell
(cr)$ cros_workon_make --board=${BOARD} ${PACKAGE} --test
```

OR

```shell
(cr)$ FEATURES=test emerge-${BOARD} ${PACKAGE}
```

### Tast integration test

```shell
(cr)$ tast run ${DUT} ${TEST_NAME}
```

Replace ${TEST_NAME} with test package and function name e.g.)
crash.UncleanShutdownCollector.

Sometimes you might need to include `--extrauseflags chrome_internal`, such as
when working with metrics_consent.
