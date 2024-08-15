---
breadcrumbs:
- - /developers
  - For Developers
- - /developers/testing
  - Testing and infrastructure
page_name: branded-build-tests
title: Fixing Chrome-branded Tests
---

This page describes how to fix test failures that only occur in Chrome-branded
builds. Note that only Google developers can build Chrome-branded builds.

Chrome-branded builds are built with the GN arg `is_chrome_branded` set to true.
There are some build bots that run the Chrome test suites with Chrome-branded
builds, and sometimes tests that pass on the "normal" bots fail on the
Chrome-branded bots. Here is an example
[bot](https://luci-milo.appspot.com/ui/p/chrome/builders/ci/win64).

Test failures in Chrome-branded builds that don’t occur in Chromium builds are
generally caused by one of several things:

* Experiments and features enabled for Canary, but not Stable. This happens
because generally experiments and features are enabled for Canary before Stable.
If the experiment/feature is known, enable it in the test fixture, e.g
[5011326](https://crrev.com/c/5011326), [5018992](https://crrev.com/c/5018992).
* `GetChannel()` on Chrome-branded build is “Stable”, because no channel is set.
On Chromium builds, it’s usually “Canary”. `ScopedCurrentChannel` can be used by
the test to force the channel to Canary, even for branded builds, e.g., 
[5003035](https://crrev.com/c/5003035). If it’s an experiment or feature that
needs to be enabled, it’s preferable to force the experiment or feature on, but
sometimes it can be hard to figure out the experiment to enable if it's an
unfamiliar area of the code.
* Calls to `EXPECT_DEATH` with DCHECKs in official, non-debug builds don't work,
so they should be guarded by `(!defined(OFFICIAL_BUILD) || !defined(NDEBUG))`,
e.g., [5014301](https://crrev.com/c/5014301). This is needed because the
Chrome-branded bots also set the GN arg `is_official_build`.
* CHECK failure in `ui::ResourceBundle::GetLocalizedStringImpl` on Windows
builds. This happens when a test uses a string that is not used in Chrome. On
Windows Chrome-branded builds, we strip out string resources that aren’t used in
Chrome. See [crbug.com/1181150](https://crbug.com/1181150). The fix is to
disable or not build the test for branded builds.
