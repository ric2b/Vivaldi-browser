---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: detect-online
title: Detecting online status in polymer
---

[TOC]

## How to check for online status in Polymer

IMPORTANT: Online status does not necessarily indicate that the user can access
the internet. See [Comments/Discussion](#commentdiscussion) for more
information.

### Problem

In a Polymer website or application, I want to detect whether the user's device
is online or offline. I want to use this value as a JavaScript variable.

### Solution

Use
[`navigator.onLine`](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/onLine)!

1.  Create a
    [declared property](https://polymer-library.polymer-project.org/3.0/docs/devguide/properties)
    in your custom element that will store the online status boolean value.

    ```javascript
    static get properties() {
      return {
        isOnline_: {type: Boolean},
      };
    }
    ```

2.  In your element's constructor, set the initial value of your declared
    property to `navigator.onLine`.

    ```javascript
    constructor() {
      super();

      /** @type {boolean} */
      this.isOnline_ = navigator.onLine;
    }
    ```

3.  In your element's
    [`ready` callback](https://polymer-library.polymer-project.org/3.0/docs/devguide/custom-elements#ready-callback),
    add event listeners for the
    [`online`](https://developer.mozilla.org/en-US/docs/Web/API/Window/online_event)
    and
    [`offline`](https://developer.mozilla.org/en-US/docs/Web/API/Window/offline_event)
    events. The callbacks on these event listeners should update the state of
    your declared property.

    ```javascript
    /** @override */
    ready() {
      super.ready();

      window.addEventListener('online', () => {
        this.isOnline_ = true;
      });

      window.addEventListener('offline', () => {
        this.isOnline_ = false;
      });
    }
    ```

4.  You can now use your bound property (`isOnline_` in this example) in your
    element's code!

    ```javascript
    ...
    if (this.isOnline_) {
      showOnlineMessage();
    } else {
      showOfflineMessage();
    }
    ...
    ```

### Example CL

*   [feedback: Help content offline message (https://crrev.com/c/3784329)](https://crrev.com/c/3784329)

### References

*   [MDN Navigator.onLine —
    https://developer.mozilla.org/en-US/docs/Web/API/Navigator/onLine](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/onLine)

### Comment/Discussion

`Navigator.onLine` is somewhat limited in functionality. This quote from MDN's
[Navigator.onLine article](https://developer.mozilla.org/en-US/docs/Web/API/Navigator/onLine)
has more details:

> In Chrome and Safari, if the browser is not able to connect to a local area
> network (LAN) or a router, it is offline; all other conditions return true. So
> while you can assume that the browser is offline when it returns a false
> value, **you cannot assume that a true value necessarily means that the
> browser can access the internet**. You could be getting false positives, such
> as in cases where the computer is running a virtualization software that has
> virtual ethernet adapters that are always "connected." Therefore, if you
> really want to determine the online status of the browser, you should develop
> additional means for checking.

While there doesn't seem to be a standardized way to detect internet connection
status from a WebUI, here are some leads to get started:

*   [ash/webui/shimless_rma/resources/onboarding_network_page.js](https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/shimless_rma/resources/onboarding_network_page.js;l=169-180;drc=09076fa5428a1a379ed4849b80b7463cf371716e)
*   [ui/webui/resources/cr_components/chromeos/cellular_setup/esim_flow_ui.js](https://source.chromium.org/chromium/chromium/src/+/main:ui/webui/resources/cr_components/chromeos/cellular_setup/esim_flow_ui.js;l=272;drc=357a9e5df7bd8979ecbb955ce6024f1853511618)
