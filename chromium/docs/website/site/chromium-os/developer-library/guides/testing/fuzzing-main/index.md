---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: fuzzing-main
title: Fuzz testing in ChromeOS (dupe)
---

[Fuzzing] is a testing technique that feeds auto-generated inputs to a piece
of target code in an attempt to crash the code. It's one of the most effective
methods we have for finding [security] and [non-security bugs]
(also see [go/fuzzing-success]). This guide introduces ChromeOS developers to fuzz
testing.

You can learn more about the benefits of fuzzing at [go/why-fuzz].

[TOC]

## Getting Started

For more information about ChromeOS kernel fuzzing, see
[here](kernel_fuzzing.md).
Userspace fuzzing on ChromeOS will be discussed below.

In ChromeOS, you can easily create and submit fuzz targets. The targets are
automatically built by buildbots, and then uploaded to the distributed
[ClusterFuzz] fuzzing system to run at scale.

Create your first fuzz target and submit it by stepping through our [Quickstart
Guide].

## Getting help

You can [chat](http://go/fuzzing-chat) or send an email to [chromeos-fuzzing@google.com] if you get
stuck, or to ask questions.

## Further Reading

*   [Detailed guide] to fuzzing on ChromeOS.
*   Improving [fuzz target effectiveness].
*   [Creating a fuzz target that expects a protobuf] (instead of a byte stream)
    as input.
*   [Debugging a fuzzer] using gdb.
*   [Reproducing bugs] found by libFuzzer and reported by ClusterFuzz.

## Trophies

*   [Issues found] with in-process fuzzing and automatically filed by
    ClusterFuzz.

## Other Links

*   [Fuzzing for Chrome Browser].
*   [Guided in-process fuzzing of Chrome components] blog post.

[ClusterFuzz]: https://clusterfuzz.com/
[Creating a fuzz target that expects a protobuf]: fuzzing.md#lib
[Debugging a fuzzer]: fuzzing.md#gdb
[Detailed guide]: fuzzing.md#Detailed-instructions
[fuzz target effectiveness]: fuzzing.md#Improving-fuzzer-effectiveness
[Fuzzing]: https://en.wikipedia.org/wiki/Fuzzing
[Fuzzing for Chrome Browser]: https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/README.md
[go/fuzzing-success]: https://goto.google.com/fuzzing-success
[go/why-fuzz]:https://goto.google.com/why-fuzz
[Guided in-process fuzzing of Chrome components]: https://security.googleblog.com/2016/08/guided-in-process-fuzzing-of-chrome.html
[Issues found]: https://bugs.chromium.org/p/chromium/issues/list?sort=-modified&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified&q=label%3AStability-LibFuzzer%2CStability-AFL%20label%3AClusterFuzz%20-status%3AWontFix%2CDuplicate&can=1
[libFuzzer]: https://llvm.org/docs/LibFuzzer.html
[libFuzzer and ClusterFuzz]: https://chromium.googlesource.com/chromium/src/+/HEAD/testing/libfuzzer/README.md
[security]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=reporter:clusterfuzz@chromium.org%20-status:duplicate%20-status:wontfix%20type=bug-security
[non-security bugs]: https://bugs.chromium.org/p/chromium/issues/list?can=1&q=reporter%3Aclusterfuzz%40chromium.org+-status%3Aduplicate+-status%3Awontfix+-type%3Dbug-security&sort=modified
[Quickstart Guide]: fuzzing.md#Quickstart
[Reproducing bugs]: fuzzing.md#Reproducing-crashes-from-ClusterFuzz
