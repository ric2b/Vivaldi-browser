---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: ui-tast-tests
title: UI selection in Tast tests
---

[TOC]

Many of our end-to-end (E2E) tests rely on finding and selecting UI to navigate
through the system. UIs are prone to change, and those changes can result in
test failures unrelated to the functionality being tested. For this reason, when
designing tests, there are general practices you should follow to reduce
fragility and reliance on UI structure and labels. And, where you must rely on
UI, there are methods to make tests easier to debug.

This guide provides a high-level overview of the basic structure of UIAuto
Finders as well as the UITree, and how to minimize brittle design patterns when
using them.

## UITree & Finders

Chrome uses the UIAuto framework to achieve UI automation. UIAuto relies on the
accessibility tree -- a representation of the UI that displays on screen -- to
navigate through the UI. The accessibility tree includes:

-   The Chrome Browser
-   The ChromeOS Desktop UI
-   ChromeOS Packaged Apps
-   Web Apps/PWAs

The accessibility tree is made up of 'nodes'. UIAuto refers to these as
`AutomationNode`. They each have a number of attributes. If you've worked with
HTML DOMs, this should look somewhat familiar. An example `AutomationNode` may
look like:

```
node id=917 role=switch state={"focusable":true} parentID=922 childIds=[921] name=Mobile data className=TrayToggleButton
```

Every node has a parent -- excluding the root node - and can possess child
nodes.

The UIAuto library uses "Finders" to find these nodes based on their attributes.
For finding nodes, the most relevant attributes are:

-   className: The implementation of the UI component.
-   name: Often the only unique identifier for a UI element. This is language
    dependant.

Other attributes include:

-   `id`: This cannot be used in tests, as it changes between test runs.
-   `role`:
    [Describes the purpose of an AutomationNode.](https://pkg.go.dev/chromium.googlesource.com/chromiumos/platform/tast-tests.git/src/chromiumos/tast/local/chrome/uiauto/role#Role)

Tip: Read more about using Finders and how to write Tast tests
[here](http://go/tast-test-intro).

## Decreasing Fragility in Tast Tests

There is no way to add additional metadata to nodes -- like an assignable ID --
to assist in finding them. Since the UITree is an accessibility tool, any
additional metadata would
[cause bloat when it is used for its primary purpose](http://go/tast-restriction),
like in screen readers.

Thus, the only way to find uniquely find a UI element is by its `name` and
`className`. Unfortunately, these attributes are the most prone to change, and
will cause unexpected failures unrelated to the functionality being tested if
they do change. There are general tips to help reduce fragility of your tests
that rely on UI selection:

### Regex Matching

When Regex matching a desired string against the `name` of a node, use **case
insensitivity** where possible. Often, a name is still uniquely identifiable
without capitalization (in regex, `(?i)`), and this greatly reduces the chance
of minor formatting changes breaking your test.

When matching against a label that reads "Connect to [Device Name]", a regex
string may look like:

```
detailsBtn := nodewith.Role("button").NameRegex(regexp.MustCompile("(?i)open settings for .*" + deviceName))
```

Rather than the case sensitive

```
... regexp.MustCompile("Connect to" + deviceName) ...
```

### Reference Existing Tast Packages

Many components have their own UIAuto library. It's safe to assume that if the
UI itself changes, that these libraries will also be updated. Packages like
[uiauto/quicksettings](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/chrome/uiauto/quicksettings/?q=chromiumos%2Ftast%2Flocal%2Fchrome%2Fuiauto%2Fquicksettings)
provide Finders as well as methods for using the QuickSettings UI.

Opening the QuickSettings UI can be achieved in one method that is guaranteed to
continue working, even if the UI changes:

```go
quicksettings.NavigateToNetworkDetailedView(...)
```

Specifying an Ancestor node in a Finder can help improve test stability by
adding another assurance that your Finder is finding the correct node.
Typically, this can add additional brittleness to your test, as it's another
node that is subject to change. However, because UIAuto packages are guaranteed
to be updated with the UI, referencing ancestor nodes from these packages is
safe, and can lead to more resistant tests.

For example, specifying a node must be a child of the QuickSettings Network UI
can be achieved by:

```go
nodewith.Role("button").NameRegex("My Button").Ancestor(quicksettings.NetworkDetailedViewRevamp)
```

### Print Errors When They Occur

To help EngProd diagnose test failures, always make sure you're checking for
errors when using Finders. When a node cannot be found, make sure to print the
error. Golang provides easy syntax to conditionally wrap errors.

```go
if err := ui.LeftClick(mobileNetworkView)(ctx); err != nil {
    s.Fatal("Failed to click on the instant tether device in Quick Settings menu: ", err)
}
```

## Ongoing Work to Decrease Fragility

There are proposals and work to decrease fragility of UI selection in automated
tests. In line with go/tast-restriction, mainline tests are currently restricted
from using UIAuto without a special exemption, since it is a major source of
flakiness and failures unrelated to functionality.

There are [ongoing proposals](http://go/tast-map-ui-pattern-proposal) for a UI
Map specifically for testing purposes, since additional data in the existing
UITree could lead to bloat when using accessibility tools. It is yet to be
approved or implemented, however. In future, this could introduce a more robust
way of selecting UI without relying on attributes that are prone to change.

## Further Reading

Highly recommended reading to get started with writing UIAuto tests include:

*   [UIAuto Codelab](http://go/tast-codelab-3)
*   [Intro to Tast Tests](http://go/tast-test-intro)
*   [Chrome Automation API Reference](https://developer.chrome.com/docs/extensions/reference/automation/)