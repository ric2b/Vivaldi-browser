---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cpp-intro
title: C++ intro
---

[TOC]

As with any large project, developing for Chromium will be much easier with a
better understanding of the technology, languages, and most frequently used
concepts and abstractions. This page is intended to provide links to resources,
guides, and codelabs that will help kickstart your Chromium development career.

## Style guides

Similar to internal Google development, Chromium development follows style
guides whose goal is largely to document and manage the "dos and don'ts" of
authoring code within Chromium. These are the
[Chromium C++ style guide](https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/styleguide.md)
and [Google C++ style guide](https://google.github.io/styleguide/cppguide.html).

## Codelabs

Chromium has a C++ 101 codelab that will help introduce developers to C++
development with Chromium, and includes coverage of threads, task runners,
callbacks, Mojo, and more! This codelab can be found at
[codelabs/](https://source.chromium.org/chromium/chromium/src/+/main:codelabs/).

## Tips of the week

While the C++ "Tips of the Week" occasionally reference concepts specific to
internal Google C++ development, the vast majority of these articles are
excellent resources on general C++ development best practices. Many of these
are [available publicly](https://abseil.io/tips/).

## Chromium code directories

-   [//ash](https://source.chromium.org/chromium/chromium/src/+/main:ash/)
    contains code pertaining to the “Aura Shell”, the window manager and system
    UI for ChromeOS. Read
    [here](https://chromium.googlesource.com/chromium/src/+/HEAD/ash/README.md)
    for more details. Notably, //ash sits below //chrome in the dependency
    graph, and cannot depend on code in //chrome.
-   [//chrome](https://source.chromium.org/chromium/chromium/src/+/main:chrome/)
    contains the open source, application layer of Chrome, and includes ChromeOS
    system UI code.
-   [//chromeos](https://source.chromium.org/chromium/chromium/src/+/main:chromeos/)
    contains low-level support for Chrome running on ChromeOS. It does not
    contain any user-facing UI code. The contents of //chromeos cannot depend on
    //chrome.

Note: If you run into a situation where the code you're writing in, say, //ash
needs to depend on something in //chrome, delegates can be used to invert the
dependency.

## Chromium containers

Chromium provides a small library of containers (e.g. maps, queues, stacks) in
`//base/containers` -- see the readme
[here](https://source.chromium.org/chromium/chromium/src/+/main:base/containers/README.md).

Don't always assume that the `std` library containers are the best option; base
containers can provide better memory or runtime performance than std library
containers in many common use cases. For example, `base::flat_map` is a great
memory-efficient alternative to `std::map` or `std::unordered_map` if the map
only has a few elements or is rarely accessed.

## Threading in Chromium

Resources:

-   [Threading and Tasks in Chrome](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/threading_and_tasks.md)

## Callbacks in Chromium

Learn more about callbacks and how they are used within Chromium
[here](https://source.chromium.org/chromium/chromium/src/+/main:docs/callback.md).
See also talk
["Callbacks in Chromium" in CrOS 101](/chromium-os/developer-library/training).

## Switch statements

### Default cases

[Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html#Switch_Statements) says if not
conditional on an `enum class`, switch statements should always have a default
case. If the default case should never execute, treat this as an error (see
[Defensive and Offensive Coding](#defensive-and-offensive-coding) below).

Generally, you should always avoid using `default` on an `enum class` to ensure
you're hitting every case and to ensure that any new case added to the enum is
the future is handled. The compiler will emit a warning if you'are not covering
every case.

### [[fallthrough]]

The [[fallthrough]] attribute should be used in switch statements to indicate
intentionally falling to the next case.

```cpp
case 1:
  DoSomething();
  [[fallthrough]]
case 2:
  DoSomethingElse();
```
