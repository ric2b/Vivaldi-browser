---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: a11y-debugging
title: Debugging accessibility (a11y) Issues
---

[TOC]

For many people, tools like [screen readers](https://en.wikipedia.org/wiki/Screen_reader)
are their primary method of interaction with our products. Therefore, the
accessibility of our interfaces should be prioritized and treated with the same
level of attention and care as the visual aspects of our UI. Ensuring correct
screen-reader behavior can make or break the usability of our features for
someone who is differently abled.

Accessibility issues can be tricky to debug. Common issues include
non-descriptive accessible names, double announcement of the content of one
element, and incorrect focus behavior, among others. What follows are a few
helpful tips to get started into debugging why these issues may occur.

## ChromeVox

ChromeOS ships with **ChromeVox**, a screen reader that helps users who with
different sight abilities navigate otherwise visual interfaces.

ChromeVox works by changing the way the user interacts with a UI. When an item
is selected, either by clicking or using tab to navigate through each element
sequentially, it reads the item aloud. By default, it reads the element's
content (typically the text content of the button, label, etc), but this
behavior can be changed in both Web UI and Ash views to help users better
understand what is being displayed.

ChromeVox helpfully provides tools, like log capture, that can help debug
issues.

### Enabling ChromeVox Logs

1.  Enable ChromeVox by pressing Ctrl + Alt + Z.
2.  Navigate to ChromeVox settings by clicking on the settings icon in the top
    right of the display, inside the ChromeVox announcement bar.
3.  Scroll to the bottom of ChromeVox Settings panel, and expand the "Developer
    Options" section.
4.  Enable event logging, and choose relevant events (typically, earcon, alerts,
    and more).
5.  Reproduce the accessibility bug using ChromeVox.
6.  View ChromeVox logs in the "Developer Options" section (fig. 1).

![ChromeVox View Logs](/chromium-os/developer-library/guides/debugging/a11y-debugging/chromevox_view_logs.png)

Figure 1: View ChromeVox logs in ChromeVox settings.

### Reading ChromeVox Logs

ChromeVox outputs a series of events in the following format:

```txt
speech 17:08:48.198 Speak (C) category=nav "Device visibility Select who can share with you"
speech 17:08:48.198 Speak (Q) category=nav "Dialog"
speechRule 17:08:48.198 RULE: navigate default speak
$name: Device visibility Select who can share with you
$value: EMPTY
$role: Dialog
$description: EMPTY

speech 17:08:48.207 Speak (Q) category=nav "Device visibility Select who can share with you"
speech 17:08:48.207 Speak (Q) category=nav "Dialog"
speechRule 17:08:48.207 RULE: navigate default speak
$name: Device visibility Select who can share with you
$value: EMPTY
$role: Dialog
$description: EMPTY
```

This log shows the two events -- each of which contain multiple speech events,
the rule for why they occurred, and additional information, like value, role,
and description. Next to each `speech` event is the "queue mode" of the text.
This 'queue mode' is unique to ChromeVox. While not officially documented, the
queue mode is formatted according to the
[output_rules.js](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/chromeos/accessibility/chromevox/background/output/output_rules.js;drc=8ffd9addc532ac9a44ac1bb0e4140aab4d28e1d2)
file in ChromeVox's source.

The example logs are taken from a real double-announcement bug (b/243231488),
where the same content was being announced twice by ChromeVox. We can deduce the
reason for the double announcement is the two differing queue modes, between
each logged event, listed next to the `speech` value. This provides a lead for
further investigation into why this issue may be occurring.

### ChromeVox Top Bar

The ChromeVox top bar (fig. 2) also provides useful information to give insight
into speech events. Besides showing a stream of speech events, it also provides
a glimpse at the reason behind specific announcements. The formatting of each
line denotes different reasons for an announcement, and can provide a hint
towards why announcements occur. For example, a double announcement where the
first and second announcement differ in text formatting hints that there are two
different causes of the item being announced twice. (ChromeVox logs provides
more precise reasoning for why this happens!)

![](/chromium-os/developer-library/guides/debugging/a11y-debugging/chromevox_top_bar.png)

Figure 2: ChromeVox top bar with announcement.

## Web UI

HTML has built-in tools for making webpages and apps accessible. ARIA
(accessible rich internet applications) are a set of roles and attributes given
to HTML elements that make web content more accessible to people with different
abilities. Since ARIA attributes often describe what the screen reader
announces, it can be the source of many accessibility bugs.

### Accessibility Tree in DevTools

The accessibility tree provides additional insight into how the screen reader
"views" the webpage. You can view this by inspecting the view, and opening the
[Accessibility Pane](https://developer.chrome.com/docs/devtools/accessibility/reference/#pane).
This can help by providing insight into any anomalies with the structure that
may cause unexpected behaviour, such as extraneous announcements. It also
provides an overview of an elements aria attributes at a glance.

TIP: For system UI, like OOBE, an inspect view may not be available without
additional steps. A workflow like go/cros-device-oobe-debug allows remote
inspection of web views that cannot be inspected via right click.