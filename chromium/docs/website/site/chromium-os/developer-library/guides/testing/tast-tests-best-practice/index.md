---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: tast-tests-best-practice
title: Best practices for writing Tast tests
---

[TOC]

## How to dump the UI Tree when the test fails?

**Problem**

In order to interact with the UI during a Tast test, the first thing you’ll need
to know is how
[UI Auto](https://pkg.go.dev/chromium.googlesource.com/chromiumos/platform/tast-tests.git/src/chromiumos/tast/local/chrome/uiauto){.external} gets the internal representation of the UI. UI Auto proxies data
through a Chrome extension that then uses `chrome.automation`. This is an API
created for accessibility services, and therefore should be able to view and
interact with the UI in the same way an end user would.

Logging the UI tree with `s.Log(uiauto.RootDebugInfo(ctx, tconn))` is useful
during development, but it has the standard problem of printf debugging where
the content isn’t available when you aren’t actively working on it.

**Solution**

Adding the following line will store the UI tree when your test fails and is
useful for almost any UI test. It will cause your test to create a file named
“ui_tree.txt” any time your test fails, and can be used to debug the cause of
the failure. The combination of this and the screenshot are the first place that
you will look when debugging a failure through Testhaus.

```go
defer faillog.DumpUITreeOnError(ctx, s.OutDir(), s.HasError, tconn)
```

Note: The ui_tree.txt file will not be created if your test test times out, but
this can be managed with `ctx.Shorten()`.

**Example**

[enabled_policy.go - ChromiumOS Code Search](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/bundles/cros/projector/enabled_policy.go;l=103;drc=68c9da70ef981f68627c487df4ed4bcc7897fb33){.external}

```go
    tconn, err := cr.TestAPIConn(ctx)
    defer faillog.DumpUITreeOnError(cleanupCtx, s.OutDir(), s.HasError, tconn)

    if err := policyutil.Refresh(ctx, tconn); err != nil {
        s.Fatal("Failed to update Chrome policies: ", err)
    }
    if err := subTest.testFunc(ctx, cr, fdms, tconn); err != nil {
        s.Fatalf("Failed to run subtest %v: %v", subTest.name, err)
    }
```

## How to reserve time for cleanup tasks?

**Problem**

It is important to ensure that your test cleans up properly and leaves the
machine state as unchanged as possible. However, Tast does not automatically
provide any buffer time after a test finishes to perform cleanup. Therefore, you
need to reserve time to run those clean up tasks. Another use case is to reserve
time to dump the UI tree on error as mentioned above.

**Solution**

Use `ctxutil.Shorten()` whenever you have a defer statement in your test.

The following is an example of how this API is used in practice. The original
context is stored as `cleanupCtx`; this should be used in any defer statements.
`ctxutil.Shorten()` is then given the original context and a time delta of 10
seconds, and returns a new context with a 10 second shorter timeout, as well as
a function to cancel this timeout. Ctx should then be used as normal throughout
the function, and`cleanupCtx` should be used in any defer statements for cleanup
tasks.

```go
    button := nodewith.Name("Continue").Role(role.Button)
    // Reserve time for cleanup.
    cleanupCtx := ctx
    // Creates a new context with a shorter timeout.
    ctx, cancel := ctxutil.Shorten(ctx, 10*time.Second)
    // ctx should then be used as normal throughout the function.
    defer cancel()
```

The name of this function can be a bit confusing since you usually are using it
because you want more time to clean up. The testing context comes with a timeout
value, and when this time has elapsed the test will fail and exit. Shorten will
create a new context that has the timeout shortened by the time delta specified.
When you use this new content during a test, when the timeout is reached you
will then have the specified amount of time to perform cleanup tasks before the
original context exits the test. This is useful for ensuring there is time to
dump the UI tree, as well as perform any other cleanup tasks. Most state changes
during a Tast test should come with a defer statement to clean them up. Any time
you have a defer statement in your test, there is a good chance you should use
Shorten. If you are not sure how long your cleanup functions will take, 10
seconds is a good default.

**Example**

*   [enabled_policy.go - ChromiumOS Code Search](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/bundles/cros/projector/enabled_policy.go;l=83;drc=68c9da70ef981f68627c487df4ed4bcc7897fb33)
    {.external}

## How to focus an element?

**Problem**

Focusing an element is one of the most common interaction you’ll need to do,
although you don’t need to worry about setting focus when clicking a button.
Some examples of when focusing a UI element is needed are:

-   To test behavior targeting keyboard users.

-   To test any special functionalilty provided by custom controls such as media
    widgets can provide special functionality that needs to be tested.

-   To test UI that requires text entry, especially on pages that have multiple
    text fields. You could manage this by navigating with the tab key, but that
    adds a dependency on tab order which can be fragile. Relying on sequences of
    hotkeys can have similar problems. Additionally, sequences of hot keys can
    require careful timing on a system under load, which can add flakiness to a
    test.

**Solution**

There are two main functions you should use to focus elements:

-   `FocusAndWait()`: this API will perform all the operations required to bring
    an item into focus, including scrolling it onto the page. It will then wait
    until the item has been successfully focused until returning.

-   `EnsureFocused()`: this API will first check if the item is focused, and
    only call FocusAndWait() if it is not. This is generally the function you
    should use, unless you want to ensure a focus event is sent to the control
    before you interact with it.

Here is an example of how to focus an element. Similar to
[`DoDefault()`](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/chrome/uiauto/automation.go;l=1443;drc=b40f17f466920754edbf239e3a5783e49c73e74a){.external},
in most cases you’ll just need to use it directly with
[`nodewith`](https://crsrc.org/o/src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/chrome/uiauto/nodewith/nodewith.go){.external}.

```go
    issueDescriptionInput := nodewith.Role(role.TextField).Ancestor(feedbackRootNode)
    if err := ui.EnsureFocused(issueDescriptionInput)(ctx); err != nil {
        s.Fatal("Failed to find the issue description text input: ", err)
    }
```

In the above examples, we obtain the text field first, then focus on it.

**Example**

*   [user_feedback_service.go - ChromiumOS Code Search](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/bundles/cros/feedback/user_feedback_service.go;l=110;drc=6ffdd544590d41d2e2dfc0b5b6aa201c11cf61fa)
    {.external}

## How to wait for a transition?

**Problem**

A lot of UI tests will have multiple screens to pass through, and ensuring that
the transitions occur correctly will help prevent your test from becoming flaky.
Often there are intermediate animations or loading screens that need to complete
before you can interact with the UI again. It can be tempting to just a
`testing.Sleep()`, but sleeping in tests is both discouraged and fragile.
Instead, we should use one of the APIs listed below that allow us to wait until
a provided condition is satisfied.

**Solution**

There are three functions that can be used to handle most transitions.

-   `WaitUntilExists()` is a good function when you are waiting for another
    screen to be displayed so you can interact with it. Chose a UI element on
    screen you will be interacting with, and pass the nodewith() for it into
    this function. This will then wait to return until that element is
    available.

-   `WaitUntilGone()` is useful for cases when you are closing a dialog or other
    UI. You specify an element on the dialog, and this function will block until
    that element is removed from the UI tree.

-   `EnsureGoneFor()` is useful for retry dialogs, or anything that might reshow
    itself on error.

In order to keep your test as stable as possible, there are a few best practices
to keep in mind.

-   Try not to rely on intermediate states. These are generally informative, or
    intended to let the user know that something is in progress. Relying on such
    UI remaining the same requires you to update the test any time that UI
    changes, and usually doesn’t improve the robustness of your test.

-   If you have trouble finding a UI state to wait for, it’s likely that a user
    will have the same problem. In this case you should consider working with UX
    to make the target UI more testable.

-   Try to chose a UI element on your target screen that you will be interacting
    with. This helps minimize your test’s dependency on the implementation, and
    prevents cosmetic changes from breaking your test.

Here is an example.

```go
    // Verify essential elements exist in the share data page.
    title := nodewith.Name("Thanks for your feedback").Role(role.StaticText).Ancestor(
        feedbackRootNode)
    newReportButton := nodewith.Name("Send new report").Role(role.Button).Ancestor(
        feedbackRootNode)
    exploreAppLink := nodewith.NameContaining("Explore app").Role(role.Link).Ancestor(
        feedbackRootNode)
    diagnosticsAppLink := nodewith.NameContaining("Diagnostics app").Role(role.Link).Ancestor(
        feedbackRootNode)

    if err := uiauto.Combine("Verify essential elements exist",
        ui.WaitUntilExists(title),
        ui.WaitUntilExists(newReportButton),
        ui.WaitUntilExists(exploreAppLink),
        ui.WaitUntilExists(diagnosticsAppLink),
    )(ctx); err != nil {
        s.Fatal("Failed to find element: ", err)
    }
```

**Examples**

*   WaitUntilExists:
    [submit_feedback_and_close_app.go - Chromium Code Search](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/bundles/cros/feedback/submit_feedback_and_close_app.go;l=86;drc=9628ca4c91018858fecf191eb06ca8fbfe328897){.external}

*   WaitUntilGone:
    [submit_feedback_and_close_app.go - Chromium Code Search](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/bundles/cros/feedback/submit_feedback_and_close_app.go;l=99;drc=9628ca4c91018858fecf191eb06ca8fbfe328897){.external}

*   EnsureGoneFor:
    [tab_capture_allowed_by_origins.go - Chromium Code Search](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/go.chromium.org/tast-tests/cros/local/bundles/cros/policy/tab_capture_allowed_by_origins.go;l=140;drc=6ffdd544590d41d2e2dfc0b5b6aa201c11cf61fa){.external}

## How to handle a dialog box stealing focus?

**Problem**

A common problem we’ve seen is tests failing due to unexpected dialog boxes
appearing.

**Solution**

In most cases, a dialog box stealing focus from your test is a legitimate bug.
Moving focus without an explicit user action is often a GAR issue, and will need
to be resolved to maintain our GAR4 rating. As such, when this happens
unexpectedly you should probably file a bug with the owner of the dialog. If
you’re not sure who that is or aren’t certain it’s appropriate, please reach out
to [TORA council](http://go/tora){.external} for advice!

There are many informative popups that you’ll encounter that should not
interfere with your test, such as toasts and notifications. Using `DoDefault()`
and the other functions discussed earlier should prevent these from interfering
with your test. If they still cause test flakiness, this is a good indication
that the end user will have a similar problem, so again it’s appropriate to file
a bug.

## Reducing external dependencies

*   Avoiding relying on the network where possible.

*   Relying on external websites or resources can increase flakiness.

*   When your feature relies on another component to test, ensure that
    component’s owner knows about the dependency.

*   Consider how you will test your features during the design phase. If it is
    difficult to determine how it can be tested in isolation that may be a sign
    that the design should be iterated on to be more testable.

## References

*   [Tast writing best practices - Google Slides](http://slides/1HVxo3NeL-PPJyQD1CXPXME0bNb48okY70A8NF2Ou28U#slide=id.p) {.external}

*   [UIAuto documentation](https://pkg.go.dev/chromium.googlesource.com/chromiumos/platform/tast-tests.git/src/chromiumos/tast/local/chrome/uiauto) {.external}
