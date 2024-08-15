---
breadcrumbs:
- - /blink
  - Blink (Rendering Engine)
- - /blink/launching-features
  - Launching Features
page_name: isolated-web-apps
title: Launching Isolated Web Apps-specific APIs
---

## Introduction

[Isolated Web Apps](https://github.com/WICG/isolated-web-apps/blob/main/README.md) (IWAs) provide an environment with stronger trust and integrity than a normal web site. While useful on its own, this also means that IWAs can be given access to APIs that are too risky to enable for regular pages.

The [Blink Launch Process](/blink/launching-features/) concerns itself with “web-exposed features”. IWAs use their own scheme (isolated-app://), must be installed, and are packaged and updated through Signed Web Bundles rather than loading on the fly over HTTP. These properties make them not entirely “part of the web” even though they are built using almost entirely web standard features.

Resolving this philosophical debate is outside the scope of this document but, web or not, it is important that changes to APIs exposed to IWAs go through a similarly rigorous launch process to ensure that changes affecting compatibility with existing applications and other implementations of these APIs are appropriately considered and adhere to the principles below.

## Principles

### Open Standards

We believe that platforms are better for developers and more successful when they are based on open standards. It should be possible for multiple vendors to build compatible engines which can run IWAs. Therefore, as with any Web Platform feature, IWA-specific APIs must have well-written specifications and a complete test suite.

When developing new APIs we should solicit community feedback to ensure that the proposed API meets the broadest potential use cases and integrates well with the rest of the platform. Note, it is reasonable to limit the scope of a proposal. Seeing broad feedback does not mean you need to accept feature creep but designs should be extensible.

### Web platform by default

Choosing to enable an API only in Isolated Web Apps should be the last option considered.

#### The goal of the Chromium project is to move the web forward

While IWAs are built on top of web technologies and retain some of the web’s benefits, they lose others such as ephemerality and seamless updates. It will be tempting to respond to concerns from Security and Privacy reviewers by choosing to make an API only be available to IWAs. Engineers should resist that temptation and keep working to make the API safe enough to let any site use it. APIs which require an IWA should be ones which break fundamental concepts in the web’s security model, such as the [same-origin policy](https://developer.mozilla.org/en-US/docs/Web/Security/Same-origin_policy). If restricting your API to IWAs is unavoidable, try to design a subset or alternative which could be available to regular sites.

For example, the web platform is strongly opinionated about the kinds of network connections a site can make. The [Direct Sockets API](https://wicg.github.io/direct-sockets/) allows a site to make unrestricted connections and doesn’t require hosts to opt in to connections from the web. This is necessary for some applications, but not every application. The complement to the Direct Sockets API is the [WebTransport API](https://w3c.github.io/webtransport/), which provides equivalent performance when endpoints can be updated to support it.

#### Plan for the possibility of APIs to be adopted by the web

History has shown that the web can find solutions to challenging technical problems, expand its capability or adapt its security model to changing user needs. For example, [getDisplayMedia()](https://developer.mozilla.org/en-US/docs/Web/API/MediaDevices/getDisplayMedia) was originally only available to Extensions, but a model was eventually developed to bring it to the web platform. If changes in the web security model or new innovations allow IWA-specific APIs to move to the web, they should. Thus, API design, implementation, and testing, should all follow web API best practices.

#### Minimize the size of the IWA platform

We seek to reduce long term maintenance cost and simplify platform comprehension by keeping the IWA platform as small as possible. If there is an equivalent solution available on the web there shouldn’t be a different solution for IWAs.

### Stable Platform

Isolated Web Applications should continue to function even as the IWA platform is updated and changes. API contributions will need to have teams that own bug fixing and other maintenance tasks indefinitely.

## When should an API be IWA-specific?

### Definitely not IWA-specific if

| Policy | Explanation |
| ------ | ----------- |
| Can be safely landed on the Web Platform | Development of Web Platform APIs should prefer an outcome in which the API is available to any site.<br><br>Building an IWA-specific API is a last resort. |
| Unsafe at any speed | Footguns. Capabilities which simply cannot be made safe for users or developers even in the enhanced trust environment provided by IWAs.<br><br>For example, the ability to directly execute arbitrary unsandboxed native code. |

### Might be IWA-specific if

| Policy | Explanation |
| ------ | ----------- |
| Violates same-origin security model but can be made less unsafe with the trust, provenance and revocation offered by the IWA packaging and distribution model | The same-origin policy is a foundational part of web security that helps isolate malicious documents and reduce attack vectors.<br><br>However, this policy can also block web apps from delivering powerful capabilities that are not restricted for other types of apps.<br><br>IWAs are packaged, auditable, and require additional steps and disclosures for a user to install them. This helps an administrator check whether the app uses the capability in an acceptable way and protects users against an app silently changing the behavior of a powerful feature which breaks the same-origin model after it has gained permission to use that feature. |
| Impossible for user to reason about their safety and we can’t easily explain risks in UX, but is also considered a default enabled capability in native apps | The threat model of some types of access are simply too difficult for a user to reason about.<br><br>For example, the risks involved in allowing access to sockets would not be obvious at all to a user. For example, a user might not consider that they still have been using a default username and password on their network router, making it easy for an app to connect and compromise it. |
| APIs that can be made safe but which need streamlined permissions for specific use cases where we can assert enhanced trust | If there is no currently safe way to ship this API, and there is a pressing developer requirement it may qualify as an IWA-specific API.<br><br>For example, APIs under this policy may use different restrictions when they become available. We will review and requalify IWA-specific APIs regularly, and if an API can be made safe it may become a Web Platform API. |

This guidance will evolve as we consider more API proposals. If your review discovers an interesting new consideration please add it to this document.

## Who do I request feedback from for IWA-specific APIs?

* Application Developers
* Other browser vendors*
* W3C TAG*
* Other JavaScript-based development platforms: Electron, Node.js, Deno, Bun, etc.

\* Standards positions have been requested from [Apple](https://github.com/WebKit/standards-positions/issues/184) and [Mozilla](https://github.com/mozilla/standards-positions/issues/799), and a review request has been made to the [TAG](https://github.com/w3ctag/design-reviews/issues/842) to gauge interest in the concept of IWAs overall. Depending on the outcome of those requests requesting feedback on particular IWA-specific APIs from other browser vendors and/or the TAG may or may not be productive. When requesting feedback be sure to reference these existing requests so that reviewers are aware of the dependency between your feature and the general IWA infrastructure.

## Who reviews IWA-specific API launches?

Notifying developers and the broader community that the Chromium project intends to prototype and ship a new IWA-specific API is just as important as for any web-exposed API. Having transparency in our review process builds trust with the community.

IWA intents should go to blink-dev@ because these APIs are implemented in Blink and many APIs we hope will have long term web-adoption. blink-dev@ is monitored by all parties interested and creating yet another venue would reduce awareness.

Send the same Intent to Prototype, Ready for Trial, Intent to Experiment, Intent to Ship and Intent to Deprecate messages you would when following the Blink Launch process. Marking a ChromeStatus feature as IWA-specific adds an additional approval gate requiring review by the IWA project owners, in addition to the normal approvals. Most importantly, 3 LGTMs from the [Blink API owners](/blink/guidelines/api-owners/).

## IWA-specific API Launch Process

### Step 0: ChromeStatus entry

Start with step 0 of the [normal launch process](/blink/launching-features/#create-chrome-status).

When creating the new feature entry, select "Isolated Web Apps-specific API" from the "Category" drop-down so that reviewers know that your feature is following the process described here.

### Step 1: Incubating

Start with step 1 of the [normal launch process](/blink/launching-features/#start-incubating) for new feature incubations.

Remember that an explainer shouldn’t contain any Chromium-specific implementation details except in rare circumstances where they are necessary as context to explain a design decision. These documents will be read by developers and non-Chromium engineers for whom such details are meaningless and distract from the discussion of the problem being tackled and the proposed solution itself.

Your explainer should be able to answer the question of why this has to be an IWA-specific API. Ask yourself the following questions in your explainer:

* Why can’t the proposed API fit into the standard web trust model?
* Why is this problem significant enough to be worth landing in this configuration?
* Is there a different API or a limited subset of the proposed API that exists already or might be made part of the standard trust model in the future?

### Step 2: Prototyping

Start with step 2 of the [normal launch process](/blink/launching-features/#prototyping) for new feature incubations.

When pushing for public engagement, look outside the traditional browser and web developer circles. IWA-specific APIs often enable capabilities found in native platforms, so reach out to other communities of JavaScript developers such as Node.js, Deno and Electron. Consistent APIs allow developers to move code between ecosystems easily.

### Step 3: Feature complete behind a feature flag

Same as step 3 of the [normal launch process](/blink/launching-features/#dev-trials) for new feature incubations.

### Step 4: Widen review

Same as step 4 of the [normal launch process](/blink/launching-features/#widen-review) for new feature incubations.

### Step 5 (optional): Origin Trial

Same as step 5 of the [normal launch process](/blink/launching-features/#origin-trials) for new feature incubations.

### Step 6: Prepare to Ship

Start with step 6 of the [normal launch process](/blink/launching-features/#new-feature-prepare-to-ship) for new feature incubations.

Include a summary of the reason why this launch is IWA-specific from the explainer section written for step 1 in the "Intent to Ship" email.

IWA-specific API launches will require an additional approval from the IWA project owners. This approval process is started by sending your "Intent to Ship" email to <a href="mailto:iwa-dev@chromium.org">iwa-dev@chromium.org</a> in addition to <a href="mailto:blink-dev@chromium.org">blink-dev@chromium.org</a>.

Documentation for the feature should be finalized by this point. Work with Developer Relations to get an article posted to [chromeos.dev](https://chromeos.dev/). This content will move to [developer.chrome.com](https://developer.chrome.com/) when IWAs are supported on more platforms and [web.dev](https://web.dev/) when IWAs are supported by multiple browsers.
