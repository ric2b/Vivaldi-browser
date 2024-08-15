---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: on-scroll-event
title: Adding handling for scroll events
---

[TOC]

## How to add onScroll event

**Prerequisites**

Polymer, CSS class.

**Problem**

We want to add some event like adding a shadow shield and some other visual
effects when scrolling a container.

**Solution**

One way is to create a class for all the css features you want to display when
scrolling, and add the class into the
[classList](https://developer.mozilla.org/en-US/docs/Web/API/Element/classList)
with on-scroll event.

Key Steps:

1.  Create your CSS classes:

```
.scrolling-shield {
    background: linear-gradient(to bottom,
        rgba(var(--cros-bg-color-rgb),0), rgba(var(--cros-bg-color-rgb),1))
  }

.scrolling-elevation {
  background-color: var(--cros-bg-color-elevation-1);
  box-shadow: var(--cros-elevation-2-shadow);
}
```

2.  Customize scrolling event

In html file:

```
<div id="scrollContainer" on-scroll="onContainerScroll_">
  ...
</div>
```

In TS file, for the classes and on-scroll function you just created: use or to
enable the effects.

If we want to keep the effect after scrolling finished, use `classList.add()` to
add it to the element. otherwise, use `classList.toggle()`.

We can also remove a class with `classList.remove()`.

```
/** @protected */
onContainerScroll_() {
  // Scrolling effect depends on the scrolling of '#scrollingContainer'.
  const container = this.getElement_('#scrollContainer');

  const shadowShield = this.getElement_('#shadowShield');
  const shadowElevation = this.getElement_('#shadowElevation');

  shadowElevation.classList.toggle(
      'scrolling-elevation', container.scrollTop > 0);
  shadowShield.classList.toggle(
      'scrolling-shield',
      container.scrollTop + container.clientHeight < container.scrollHeight);
}
```

In the code above, the scrolling position is represented with
[`scrollHeight`](https://developer.mozilla.org/en-US/docs/Web/API/Element/scrollHeight),
[`clientHeight`](https://developer.mozilla.org/en-US/docs/Web/API/Element/clientHeight)
and
[`offsetHeight`](https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement/offsetHeight).
For the relationship between these variables we can have an insight
[here](https://stackoverflow.com/a/22675563).

**Example CL**

*   https://crrev.com/c/4020861

**References**

*   https://developer.mozilla.org/en-US/docs/Web/API/HTMLElement
