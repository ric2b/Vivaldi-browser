---
breadcrumbs:
- - /developers
  - For Developers
page_name: webui
title: Creating Chrome WebUI Interfaces
---

### General guidelines
When creating or modifying WebUI resources, follow the [Web
Development Style Guide](/developers/web-development-style-guide). Note that most
WebUI code is using TypeScript, and any new additions must use TypeScript.

A general explanation of how WebUI works, including the interaction between
C++ and TypeScript code, can be found in the [WebUI Explainer](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webui/webui_explainer.md).

Shared, cross-platform resources can be found in [ui/webui/resources](https://source.chromium.org/chromium/chromium/src/+/main:ui/webui/resources/).

A detailed example of how to create a WebUI in can be found at
[Creating WebUI interfaces in chrome](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webui/webui_in_chrome.md).

If you need additional information on how to set up the BUILD.gn file to build
your WebUI, there is detailed information and additional examples for BUILD
files specifically at [WebUI Build Configurations](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webui/webui_build_configuration.md).

If you need additional information on how to share TS/HTML/CSS code between
multiple (2+) WebUI surfaces, see [Sharing Code in WebUI](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/webui/webui_code_sharing.md).

### Types of WebUI pages
Before adding a new WebUI, consider the UI's audience and purpose. If it's
primarily intended for use by anyone other than Chromium developers, it is
a user facing UI. If it's primarily intended for a Chromium developer team
to debug code they maintain or new features the team is developing, it is a
debug UI.

#### User facing UIs
There are a few subcategories of user facing pages:
- Most user facing UIs are meant for all Chromium end users. They must be
  internationalized and fully accessible by screen readers, as well as
  fully tested. They are held to high standards for UX design and
  testing.
- Developer facing UIs are meant for all developers (either Chromium or
  external developers). Like user facing UIs, these UIs should have automated
  tests to ensure they are not accidentally broken, and in most cases they
  should also be internationalized and fully accessible by screen readers.
- Bug reporting facilitators are primarily designed for Chromium end users
  who need to report bugs. Since they're intended for end users to access,
  they should be internationalized and accessible by screen readers, but like
  developer facing UIs, their design/appearance may not always be as polished
  or up to date as more prominent user facing UIs. Bug reporting facilitators
  should have automated tests to ensure these flows are not broken for users.

#### Debug UIs
Debug UIs are primarily intended for use by Chromium developers. If a UI is
expected to be frequently used by third party developers or some subset of
users (e.g., enterprise admins), it's not a debug UI; see section above.
- Debug UIs must be placed behind the `kInternalOnlyUisEnabled` pref. This
  is most easily accomplished by extending the [DefaultInternalWebUIConfig]
  (https://source.chromium.org/chromium/chromium/src/+/main:content/public/browser/internal_webui_config.h;l=43)
  class. Placing the UIs behind this pref means that developers need to
  enable such UIs from chrome://chrome-urls before accessing one for the first
  time.
- Debug UIs do not require internationalization or full a11y features.
- Debug UIs should be able to be removed from official builds without major
  disruption.
- Debug UIs may have occasional breakages, particularly if the team that owns
  them doesn't test them regularly or add any automated tests. If this happens,
  the bug fix likely won't be approved for merge to a release that is already well
  into Beta or Stable, because debug UIs are considered non-critical.
- Debug UIs will be allowed to break by the WebUI platform team if their owners
  cannot be reached and they are blocking horizontal infrastructure updates.

### Creating a new WebUI
Apart from audience and general purpose, there are a few other questions to
think about before creating any new WebUI:
- How long is the UI needed? While most user facing WebUIs are kept indefinitely,
  it's possible for debug UIs to only be a temporary need.
- Is the WebUI needed on all platforms? Android, iOS, and ChromeOS differ from
  Windows/Mac/Linux in terms of what shared resources are available. iOS also
  requires a separate backend implementation.
- Does the UI need a Mojo interface between the page and its backend controller?
- Will the UI use Lit, native web components, or neither? If it's using Lit,
  will it use shared elements from the cr_elements library?
- Who will maintain the WebUI going forward? Each new WebUI should have an
  OWNERS file listing individuals or teams that the WebUI team can contact
  regarding any deprecations, updates to best practices, etc. Make sure your
  OWNERS file contains multiple people or a full team; it's easy for single-
  owner UIs to end up unowned.
- How can the UI be verified by Chromium developers performing horizontal
  updates? Does testing it require just navigating to the page, or are additional
  feature flags/user actions/external hardware required to trigger important
  page features? This information should be added in a README alongside your
  code when you add your UI.
- If you are adding a Debug UI, consider: does this debug information warrant
  its own UI surface to display, or could it instead be incorporated into an
  existing UI like chrome://system? Chromium already has several dozen debug
  UIs. Each one adds to binary size and maintenance burden. Strongly consider
  whether you can reuse an existing page before adding a new one.

You should be prepared to address these questions in code review when adding
your WebUI.