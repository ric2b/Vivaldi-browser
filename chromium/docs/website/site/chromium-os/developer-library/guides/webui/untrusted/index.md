---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides > WebUI
page_name: webui-untrusted
title: Communication between chrome:// and chrome-untrusted://
---

## Why?

For security consideration, some contents/resources may be served under
chrome-untrusted://, we need to know how to communicate with resources under
chrome-untrusted:// server.

*   chrome:// web pages are granted special capabilities not granted to ordinary
    web pages. It is too powerful to process untrustworthy content.
*   By using chrome-untrusted:// we put the untrustworthy content into a
    sandboxed and non-privileged environment (an ordinary renderer, with no
    dangerous capabilities).

Good example will be feedback app(chrome://os-feedback), open it using
shift+alt+i, the Suggested help content section is served under
chrome-untrusted://.

screenshot: http://screen/8uwoWtpJYzLx7jp

## Solution

-   Use postMessage() and addEventListener('message').

    -   The postMessage() method safely enables cross-origin communication
        between Window objects;

### Examples

#### From TRUSTED_ORIGIN To UNTRUSTED_ORIGIN (Feedback app as example)

***Structure of searchPage os feedback app.***

```
<feedback-flow>
  <search-page>
    <!-- Resources/Contents in <iframe> is served under chrome-untrusted:// -->
    <iframe on-load="resolveIframeLoaded_"
        src="chrome-untrusted://os-feedback/untrusted_index.html">
        <untrusted-index>
          <help-content>
          </help-content>
        <untrusted-index>
    </iframe>
  </search-page>
</feedback-flow>
```

**Step 1: Post data from TRUSTED_ORIGIN to UNTRUSTED_ORIGIN**

-   In this example, post search result data from `<search-page>` to
    `<untrusted-index>`.

```
  /** @type {!SearchResult} */
    const data = {
      contentList: /** @type {!HelpContentList} */ (
          isPopularContent ? this.popularHelpContentList_ :
                             response.response.results),
      isQueryEmpty: isQueryEmpty,
      isPopularContent: isPopularContent,
    };

    this.iframe_.contentWindow.postMessage(data, OS_FEEDBACK_UNTRUSTED_ORIGIN);
```

https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/os_feedback_ui/resources/search_page.ts;l=200

**Step 2, addEventListener('message') to UNTRUSTED_ORIGIN**

-   In this example, addEventListener to `<untrusted-index>` and display the
    search result in `<help-content>`*

```
  window.addEventListener('message', event => {
    if (event.origin !== OS_FEEDBACK_TRUSTED_ORIGIN) {
      console.error('Unknown origin: ' + event.origin);
      return;
    }
    // After receiving search result sent from parent page, display them.
    helpContent.searchResult = event.data;
  });
```

https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/os_feedback_ui/untrusted_resources/untrusted_index.ts;l=20

#### From UNTRUSTED_ORIGIN To TRUSTED_ORIGIN

**Step 1: Post data from UNTRUSTED_ORIGIN to TRUSTED_ORIGIN**

-   In this example, post data(whether user clicked the helpContent) from
    `<help-content>` to `<feedback-flow>`

```
  /**
   * @param {!Event} e
   * @protected
   */
  handleHelpContentClicked_(e) {
    e.stopPropagation();
    window.parent.postMessage(
        {
          id: 'help-content-clicked',
        },
        OS_FEEDBACK_TRUSTED_ORIGIN);
  }
```

https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/os_feedback_ui/resources/help_content.ts;l=181-193

**Step 2, addEventListener('message') to TRUSTED_ORIGIN**

-   In this example, addEventListener to `<feedback-flow>`.

```
  window.addEventListener('message', event => {
      if (event.data.id !== HELP_CONTENT_CLICKED) {
        return;
      }
      if (event.origin !== OS_FEEDBACK_UNTRUSTED_ORIGIN) {
        console.error('Unknown origin: ' + event.origin);
        return;
      }
      this.helpContentClicked_ = true;
    });
```

### Example CLs:

3811341: Feedback: Metrics to record user's exit path |
https://chromium-review.googlesource.com/c/chromium/src/+/3811341

3445408: feedback: Send help content from parent page to embedded child page |
https://chromium-review.googlesource.com/c/chromium/src/+/3445408

3544317: feedback: Show popular help content when no match or as cold start |
https://chromium-review.googlesource.com/c/chromium/src/+/3544317
