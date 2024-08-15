---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: javascript-mock
title: How to mock JavaScript functions
---

[TOC]

## How to test whether a built-in JavaScript function is being called

**Prerequisites**

Event, Promise

**Problem**

I want to verify that a JS function of an object is invoked after a certain
action is taken. The function does not raise any event that can be listened to.

For example, I wanted to test that when the done button is clicked, the
window.close() is called to close the app.

```
   * Close the app when the user clicks the done button.
   * @protected
   */
  handleDoneButtonClicked_() {
    window.close();
  }
```

**Solution**

One simple way is to replace the function definition with a mock function. The
mock function contains code that:

*   will resolve a promise so that we can wait on it and check expectations
    after it is resolved.
*   will update a counter so that we can verify the expectation.

Key Steps:

1.  Import
    [PromiseResolver](https://crsrc.org/c/ui/webui/resources/js/promise_resolver.js?q=lang:js%20-filepath:third_party%2F%20-filepath:out%2F%20-filepath:v8%2F%20promise_resolver&ss=chromium%2Fchromium%2Fsrc)
    and
    [flushTasks](https://crsrc.org/c/chrome/test/data/webui/test_util.js?q=lang:js%20-filepath:third_party%2F%20-filepath:out%2F%20-filepath:v8%2F%20flushtask&ss=chromium%2Fchromium%2Fsrc).

```
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {flushTasks} from '../../test_util.js';
```

1.  Add a new test

```
// Test clicking the done button should close the window.
test('ClickDoneButtonShouldCloseWindow', async () => {
  // Initialize the page by adding the polymer element to the page.
  await initializePage();

  // Create a  promise resolver.
  const resolver = new PromiseResolver();

  // Initialize a counter.
  let windowCloseCalled = 0;

  // Define a mock close function to replace the window.close().
  const closeMock = () => {
    windowCloseCalled++;
    return resolver.promise;
  };

  // Replace the close function with the mock one.
  window.close = closeMock;

  // Click the done button.
  const doneButton = getElement(page, '#buttonDone');
  doneButton.click();

  // Wait for the event to be processed.
  await flushTasks();
  // Verify the expectation. Counter value of 1 indicates the window.close()
  // has  been called once.
  assertEquals(1, windowCloseCalled);
});
```

If the function being tested is a private function, simply use the `set`
method in the element's constructor:

```
const mockFunction = () => {
  windowCloseCalled++;
  return resolver.promise;
}

element.set('privateMethodToCall_', mockFunction);
```

###Testing methods to fire a custom event###
One way to verify that a method successfully fire an event is adding
temporary event listeners to the element in the browser test.

In the following example, I want to test that the `select` element will observe
changing of `selectedValue` and fire a custom event `button-remapping-changed`:

In the initialize state, add a event listener to the
`select` element being tested:

```
select = document.createElement (CustomizeButtonSelectElement.is);

const mockFunction = () => {
  buttonRemappingChangedEventCount++;
  return resolver.promise;
}

select.addEventListener('button-remapping-changed', mockFunction);
```

Then in the testing stage, simply update the observed `selectedValue` and
assert `buttonRemappingChangedEventCount`:

```
// Update select to another remapping action.
select.selectedValue = 'acceleratorAction1';
await flushTasks();

// Verify that event is fired and button remapping is updated.
assertEquals(buttonRemappingChangedEventCount, 1);
```

**Example CL**

*   https://crrev.com/c/3680386

**Comment/Discussion**

Tip: This technique can also be used to return fake data in the function.
