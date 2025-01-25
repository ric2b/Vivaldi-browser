---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: cpp-invariant-checks
title: Invariants and CHECKKs in C++
---

[TOC]

## Defensive vs offensive coding

Defensive programming is the practice of writing software to enable continuous
operation after and while experiencing unplanned issues. However, while
defensive coding will result in code that is more resilient to crashes, it can
leave the system running in an undefined state. Devices that are in an undefined
state are broken and need to be reset. By using defensive programming, you are
continuing the device's state to handle an undefined situation, which leaves the
system vulnerable and masks the error.

Offensive programming, although seemingly opposite word choice, actually expands
upon defensive programming and takes it one step further: instead of gracefully
handling a failure point, offensive programming asserts that invariants always
hold, and reveals to developers any information that will be helpful to debug.

See
["Defensive Programming: Friend or Foe"](https://interrupt.memfault.com/blog/defensive-and-offensive-programming)
for more information.

## CHECK(), DCHECK(), NOTREACHED_NORETURN() and NOTIMPLEMENTED()

Assertions are an important tool of offensive programming to enforce assumptions
and code paths that we expect to be a certain way. See this
[README](https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/c++/checks.md)
for more information on this topic. Below are commonly used assertions:

-   CHECK(): Use CHECK() to document and verify invariants. CHECK() will
    immediately kill the process if its condition is `false` for *all* builds,
    release and otherwise.

    Another use case for CHECK() is to track down known crashes when debugging
    or developing a feature. If a crash is happening and you cannot figure out
    why, add CHECK()s in the code that run before the crash to check your
    assumptions. This may move the crash further up the call stack thus helping
    you find the erroneous condition leading to the crash.

-   DCHECK(): DCHECK() is like CHECK(), but is only included in builds where
    `DCHECK_IS_ON` is true (debug builds not end-user builds). Prefer CHECK() to
    DCHECK() for stronger assertive coding. Use DCHECK() only if the invariant
    is known to be too expensive to verify in production.

-   NOTREACHED_NORETURN(): Use NOTREACHED_NORETURN() to enforce a particular
    branch of code should never be run. If run, it will cause a crash if
    `DCHECK_IS_ON` is enabled. Prefer to unconditionally CHECK() instead of
    conditionally hitting a NOTREACHED_NORETURN(). Prefer NOTREACHED_NORETURN()
    instead of NOTREACHED(). For example, if you are writing a switch statement
    that requires all enums, but only a subset of the enums are ever expected to
    be reached, use a NOTREACHED_NORETURN() on the others to declare they should
    never be run. For example,

    ```cpp
    void FastPairPresenterImpl::OnPairingFailedDismissed(
       PairingFailedCallback callback,
       FastPairNotificationDismissReason dismiss_reason) {
     switch (dismiss_reason) {
       case FastPairNotificationDismissReason::kDismissedByUser:
         callback.Run(PairingFailedAction::kDismissedByUser);
         break;
       case FastPairNotificationDismissReason::kDismissedByOs:
         callback.Run(PairingFailedAction::kDismissed);
         break;
       case FastPairNotificationDismissReason::kDismissedByTimeout:
         // Fast Pair Error Notifications do not have a timeout, so this is never
         // expected to be hit.
         NOTREACHED();
         break;
     }
    }
    ```

-   NOTIMPLEMENTED(): Use NOTIMPLEMENTED() to note that a path of code that is
    reached is not implemented via logs. For example, NOTIMPLEMENTED() can be
    used to ensure that a method in a base class is either overridden or never
    called.

See
[here](https://chromium.googlesource.com/chromium/src/+/HEAD/styleguide/c++/checks.md)
for a shorthand reference.
