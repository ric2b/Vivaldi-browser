---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: tast-tests-writing-tips
title: Tips on writing Tast tests
---

[TOC]

## How to view accessibility tree

**Problem**

I want to know what elements I can obtain for tast end to end test.

**Solution**

Solution 1:

Tast test's ui automation API is based on accessibility DOM tree. To view full
accessibility tree, follow this guide in the chrome devtool.
[Full accessibility tree in Chrome DevTools](https://developer.chrome.com/blog/full-accessibility-tree/).

Solution 2:

Print out the full a11y tree in a tast test.

```go
s.Log(uiauto.RootDebugInfo(ctx, tconn))
```

## How to obtain a button and verify if it exists

**Problem**

I want to verify if a button exists.

**Solution**

```go
button := nodewith.Name("Continue").Role(role.Button)
if err := ui.WithTimeout(20 * time.Second).WaitUntilExists(button)(ctx); err != nil {
    s.Fatal("Failed to click continue button: ", err)
}
```

Some buttons appear as a `button` in the a11y tree, but if you look
them up by `role.Button`, you won't be able to find them. [For example](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/chrome/uiauto/filesapp/filesapp.go),
the "More…" button in the files app should be looked up by `role.PopUpButton`.
If you are looking for a button and can't find it, try to remove the role and
look it up by the name only.

**Example CL**

*   Launch feedback app from launcher: https://crrev.com/c/3708153.

**References**

*   `nodewith` is a helpful API to access the node. Here is the
    [source code](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/chrome/uiauto/nodewith/nodewith.go).
*   `role` is a helpful API to identify the role of the node. Here is the
    [source code](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/chrome/uiauto/role/constants.go).

## How to mock a mouse click action

**Problem**

I want to mock a mouse click action.

**Solution**

 Use `DoDefault` whenever possible, e.g. if the node is a `Button`.

```go
button := nodewith.Name("Continue").Role(role.Button)
if err := ui.DoDefault(button); err != nil {
    s.Fatal("Failed to click continue button: ", err)
}
```
For some nodes, e.g. `GenericContainer`, the `DoDefault`
function does something else instead of clicking. In these cases, use
`LeftClick`. It is more flaky, so only use if `DoDefault` doesn't work.

```go
feedbackShortcut := nodewith.NameContaining("Send feedback").Role(role.GenericContainer)
if err := ui.LeftClick(feedbackShortcut)(ctx); err != nil {
    s.Fatal("Failed to click feedback shortcut: ", err)
}
```

In the above examples, we obtain the node first, then click on it.

**Example CLs**

*   `DoDefault`: https://crrev.com/c/3754610.

*   `LeftClick`: https://crrev.com/c/3935384.

**References**

*   `LeftClick` and `DoDefault` are two of many actions we can mock.To see all
    other actions, here is the
    [source code](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/chrome/uiauto/automation.go).

## How to mock keyboard typing

**Problem**

I want to mock a keyboard typing action.

**Solution**

Solution 1: here we mock the keyboard typing an issue description: "I am not
able to connect to bluetooth".

```go
// Set up keyboard.
kb, err := input.Keyboard(ctx)
if err != nil {
    s.Fatal("Failed to find keyboard: ", err)
}
defer kb.Close()

// Enter issue description.
if err := kb.Type(ctx, "I am not able to connect to bluetooth"); err != nil {
    s.Fatal("Failed to enter issue description: ", err)
}
```

Sometimes it is useful to make sure that the text field is loaded before the
typing starts. If you run the test and don't see any input, try this.

```go
searchBar := nodewith.NameContaining("Search settings").Role(role.SearchBox)
if err := ui.WaitUntilExists(searchBar)(ctx); err != nil {
    s.Fatal("Failed to find search bar: ", err)
}

// Search for "feedback" in the settings search bar.
if err := kb.Type(ctx, "feedback"); err != nil {
    s.Fatal("Failed to type search query: ", err)
}
```

NOTE: you don't need to `WaitUntilExists` when using `DoDefault` and `LeftClick`,
because it's baked into those functions.

Solution 2: here we mock the keyboard typing a shortcut Alt+Shift+I.

```go
if err := kb.Accel(ctx, "Alt+Shift+I"); err != nil {
    s.Fatal("Failed pressing alt+shift+i: ", err)
}
```

**Example CLs**

*   Launch feedback app from launcher: https://crrev.com/c/3708153.

*   Wait until the field exists: https://crrev.com/c/3935384.

**References**

*   To see other available actions of Keyboard, here is the
    [source code](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/input/keyboard.go).

## How to verify an attribute value

**Problem**

I want to verify if a checkbox is checked.

**Solution**

```go
// Verify share url checkbox is checked by default.
checkboxAncestor := nodewith.NameContaining("Share URL").Role(
  role.GenericContainer).Ancestor(feedbackRootNode)
checkbox := nodewith.Role(role.CheckBox).Ancestor(checkboxAncestor)
if err := ui.WaitUntilExists(checkbox.Attribute("checked", "true"))(ctx); err != nil {
  s.Fatal("Failed to find checked share url checkbox: ", err)
}
```

In the above example, we obtain the checkbox ancestor first, which is a
cr-checkbox, then select the checkbox. We can use `Attribute` function to verify
if the `checked` attribute is `true`.

**Example CL**

*   Verify share url checkbox is checked
    by default: https://crrev.com/c/3825006.

**References**

*   `Attribute` function is the one we use to verify the attribute value, here
    is the
    [source code](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/chrome/uiauto/nodewith/nodewith.go;l=316).

## How to obtain node name value

**Problem**

I want to obtain the link name value.

**Solution**

```go
currentNodeInfo, err := ui.Info(ctx, link)
if err != nil {
    return errors.Wrap(err, "failed to find the first link info during update")
}
currentName := currentNodeInfo.Name
```

In the above example, we call `ui.Info` function first to get the
`currentNodeInfo` object. In this object the `Name` attribute has the string
value for this `link`.

**Example CL**

*   Verify delete issue description will
    update search result: https://crrev.com/c/3832358.

**References**

*   `Info` function is the one we use to get the `NodeInfo` object, here is the
    [source code](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/chrome/uiauto/automation.go;l=179).

## How to wait for a process to complete

**Problem**

I need to wait for a running process to complete before continuing.

**Solution**

```
    proc, err := procutil.FindUnique(procutil.ByExe("/path/to/exe"))
	if err != nil {
		s.Fatal("Process did not start: ", err)
	}
    if err := procutil.WaitForTerminated(ctx, proc, 2*time.Minute); err != nil {
        s.Fatal("Process did not stop: ", err)
    }
```

In the above example a process is already running and `WaitForTerminated` polls
until the process is no longer running or timeout occurs.
until the process is no longer running or timeout occurs.

**Example CL**

*   Pause while `stressapptest` running before looking for Pass/Fail badge:
    http://go/cl-c/3833292

**References**

* procutil [source code](https://osscs.corp.google.com/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/procutil)

## How call mojo-remote from tast

**Problem**

I need to trigger JS mojo function from the test.

**Solution**

* Create a JS wrapper which implements the functionality you need for your test
including getting the mojo remote.
* Create a go-module/file and use `conn.Call` to load the JS into a JSObject

```
// SystemDataProviderMojoAPI returns a MojoAPI object that is connected to a SystemDataProvider
// mojo remote instance on success, or an error.
func SystemDataProviderMojoAPI(ctx context.Context, conn *chrome.Conn) (*MojoAPI, error) {
	var mojoRemote chrome.JSObject
	if err := conn.Call(ctx, &mojoRemote, systemDataProviderJs); err != nil {
		return nil, errors.Wrap(err, "failed to set up the SystemDataProvider mojo API")
	}

	return &MojoAPI{conn, &mojoRemote}, nil
}
```

* Use `Call` to trigger javascript from JS wrapper

```
// RunFetchSystemInfo calls into the injected SystemDataProvider mojo API.
func (m *MojoAPI) RunFetchSystemInfo(ctx context.Context) error {
	jsWrap := "function() { return this.fetchSystemInfo() }"
	if err := m.mojoRemote.Call(ctx, nil, jsWrap); err != nil {
		return errors.Wrap(err, "failed to run fetchSystemInfo")
	}

	return nil
}
```

* Always release when done to free resources.

Note: Works for mojo which is loaded into the frontend as "mojom-lite" or
"mojom-webui". If using "mojom-webui" you will need to programmatically load the
module you want to interact with. "mojom-lite" loads the mojo API onto the
window.

**Example CL**

* Using mojom-lite without embed https://crrev.com/c/3811849
* Using mojom-webui with embed https://crrev.com/c/3911047

**References**

*   Diagnostics api example
    [source code](https://osscs.corp.google.com/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/diagnostics/utils/api.go)
*   Network api go-module
    [source code](https://osscs.corp.google.com/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/network/diag/api.go)
    and javascript
    [source code](https://osscs.corp.google.com/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/common/network/diag/api.go)

## How to add name to generic element

**Problem**

I need to add name to generic element in a11y tree.

**Solution**

*   Add `aria-label` to the element in html to give it a name.
*   Add `aria-labelledby` to the element in html if you want to reuse a string
name in the current html file.

**Example CL**

*   aria-label: https://crrev.com/c/4408376
*   aria-labelledby: https://crrev.com/c/3867933
