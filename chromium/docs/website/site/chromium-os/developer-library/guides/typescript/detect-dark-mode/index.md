---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: detect-dark-mode
title: Detecting dark mode in polymer
---

[TOC]

## How to check for dark mode in Polymer

### Prerequisites

Polymer, CSS @media at-rule

### Problem

In a Polymer website or application, I want to detect the color scheme of the
browser (light mode or dark mode). I want to use this value as a JavaScript
variable.

### Solution

Use **iron-media-query**!

1.  Import the library into your custom element's JS file using:

    ```javascript
    import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
    ```

2.  Create a
    [declared property](https://polymer-library.polymer-project.org/3.0/docs/devguide/properties)
    in your custom element that will store the dark mode boolean value.

    ```javascript
    static get properties() {
      return {
        isDarkModeEnabled_: {type: Boolean},
      };
    }
    ```

3.  Add the `iron-media-query` element into your custom element's HTML file.

    The `query` attribute can contain any valid CSS @media at-rule
    ([see "@media" from MDN](https://developer.mozilla.org/en-US/docs/Web/CSS/@media)),
    but in this case we'll check for dark mode.

    The `query-matches` attribute should contain the Polymer property that you
    want the boolean query result to bind to. Remember to use curly brackets to
    allow
    [two-way binding](https://polymer-library.polymer-project.org/2.0/docs/devguide/data-binding#two-way-bindings).

    ```javascript
    <style>
    </style>
    <iron-media-query
      query="(prefers-color-scheme: dark)"
      query-matches="{{isDarkModeEnabled_}}">
    </iron-media-query>
    <div>
      // The body of your custom element
    </div>
    ```

4.  You can now use your bound property (`isDarkModeEnabled_` in this example)
    in your element's code!

    ```javascript
    ...
    if (this.isDarkModeEnabled_) {
      showDarkModeIllustration();
    } else {
      showLightModeIllustration();
    }
    ...
    ```

### Example CL

*   [feedback: Help content offline message (https://crrev.com/c/3784329)](https://crrev.com/c/3784329)

### References

*   [MDN @media — https://developer.mozilla.org/en-US/docs/Web/CSS/@media](https://developer.mozilla.org/en-US/docs/Web/CSS/@media)

### Comment/Discussion

**iron-media-query** can be used for other applications, including checking for
screen width, screen height, device orientation, and whether the user prefers
reduced motion.

## How to check for dark mode in CSS

You can use the @media at-rule to check for dark mode in CSS:

```css
/* Apply light mode styles */
body {
  background-color: white;
}

@media (prefers-color-scheme: dark) {
  /* Apply dark mode styles */
  body {
    background-color: black;
  }
}
```
