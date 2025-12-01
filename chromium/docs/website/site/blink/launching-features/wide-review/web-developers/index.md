---
breadcrumbs:
- - /blink
  - Blink (Rendering Engine)
- - /blink/launching-features
  - Launching Features
- - /blink/launching-features/wide-review
  - Getting Wide Review
page_name: web-developers
title: Documenting Review from Web and Framework Developers
---

TL;DR: Try searching for developers positively mentioning your feature on
Twitter (via Twitter's own search if you have an account
[[example]](https://x.com/search?q=%22img%22%20%22loading%3D%27lazy%27%22&src=typed_query),
or via Google Search if you don't
[[example]](https://www.google.com/search?q=%22img%22+%22loading%3D%27lazy%27%22+site%3Atwitter.com)).
Proceed similarly on StackOverflow, GitHub, or any other relevant platform
(hint: append the term "blog" to your keywords). If this doesn't reveal useful
signals, see [Obtaining developer feedback](#obtaining-developer-feedback)
below.

Every shipment *requires* Web / Framework developer views in the "iterate on
design" section of [Chrome Status](https://chromestatus.com/). Obtaining this
signal is *optional* for [dev trials](/blink/launching-features/#dev-trials).
For example, the "Web / Framework developer views" notes box for a hypothetical
feature might read:

> * &lt;link to Twitter poll>
> * &lt;link to library that implements the feature in question>
> * &lt;link to GitHub thread comment>

These signals are reported for the purpose of giving an as clear as possible
indication of the views of those developers as a proxy for "all" developers. The
[Other Implementers Signal](/blink/launching-features/wide-review/) is yet
another signal separate from the Web / Framework Developers Signal, and is not
less important. As with other signals, the Web / Framework Developers Signal is
only one input into the intent-to-ship process to take into consideration, and
is neither a veto nor an implicit approval to ship.

If obtaining a Web / Framework Developers Signal is skipped or incomplete and
N/A or No Signal is reported, it should be reported why. Reasons why can
include:

*   The change is too small to justify this effort or something that developers
    commonly would not ask for (for example, a deprecation), or the change is a
    Web-exposed modification to align with the spec and other browsers’
    behavior. Use common sense.
*   No response came in after a reasonable timeframe. Since the steps may vary
    intent-by-intent, It should be mentioned how long that "reasonable
    timeframe" was.

## Details

<table>
<thead>
<tr>
  <th>Signal
  <th>Required evidence
<tbody>
<tr><td>No Signal
<td>

Link to outreach attempts that remained unanswered. This is different from N/A.

<tr><td>Positive
<td>

There is evidence of publicly linkable **positive** statements of support from
developers or framework authors not associated with or not speaking on behalf of
a browser vendor:

*   Online social networking sites like [Twitter](https://x.com/home).
*   Version control sites like [GitHub](https://github.com/).
*   Discussion boards like [WICG Discourse](https://discourse.wicg.io/).
*   Personal sites like
    [blogs](https://lea.verou.me/blog/2020/04/lch-colors-in-css-what-why-and-how/).
*   Support sites like
    [StackOverflow](https://stackoverflow.com/questions/2568480/cross-browser-text-stroke-methods).
*   Bug trackers like
    [issues.chromium.org](https://issues.chromium.org/) (including the "star"
    signal[^1]).
*   Working Group call minutes that included industry participants.
*   Authorized quotes from an important partner that they will try it and why
    like [on blink-dev@](https://groups.google.com/a/chromium.org/g/blink-dev/c/lExIv0Fh8fs/m/CeCx1xunAgAJ).
*   Client-side implementations of libraries that (in part, if not fully doable
    on the client-side) do what the feature proposes, like, for example,
    [NoSleep.js](https://github.com/richtr/NoSleep.js/) for the
    [Screen Wake Lock API](https://developer.mozilla.org/en-US/docs/Web/API/Screen_Wake_Lock_API).
*   Feature reached [TC39 stage 3](https://tc39.es/process-document/).

<tr><td>Mixed
<td>

There are signals that point in both positive and negative directions. Link to
representative examples of each. This situation can happen if there is some
amount of developer disagreement or controversy regarding a feature.

In such cases, also add explanatory text about the reasoning for how the
negative feedback is outweighed by the positive, and how the feature design
mitigates the legitimate concerns represented by the negative feedback.

<tr><td>Neutral / Negative
<td>

There is evidence of publicly linkable **neutral / negative** statements of
support from developers or framework authors not associated with or not speaking
on behalf of a browser vendor:

*   Version control sites like GitHub.
*   Discussion boards like WICG Discourse.
*   Personal sites like blogs.
*   Support sites like StackOverflow.
*   Bug trackers like issues.chromium.org (including the "star" signal).
*   Working Group call minutes that included industry participants like the
    W3C’s archives.
*   Authorized quotes from an important partner that they will not try it and
    why like on blink-dev@.

In such cases, also add explanatory text about the reasoning for how the feature
design mitigates the legitimate concerns represented by the negative feedback.

<tr><td>N/A
<td>

Indication of why a Web / Framework Developers Signal is not needed in this case
and no outreach attempt was made. This is different from No Signal.

</table>

[^1]: Anecdotal evidence shows that 10+ stars can be counted as a strong signal
      and 5+ stars as a positive signal.

## Obtaining developer feedback

Chromium/Chrome Developer Relations (DevRel) can—no surprise—help with reaching
developers. The team can be contacted via <chromium-devrel@chromium.org>. For
external-facing developer outreach, DevRel has access to the official
[@ChromiumDev Twitter account](https://x.com/chromiumdev)—well noting that this
may introduce follower bias. DevRel can signal-boost or create polls, and ask
people to chime in on conversations happening on GitHub, Discourse, and
elsewhere. For more in-depth surveys, the Chrome team is working with
[C SPACE](https://cspace.com/) to build a dedicated community of about 600 Web
developers from a range of backgrounds, experiences, skill sets, and industries
who will provide direct feedback to questions that you come up with. The Chrome
team also works with a group of volunteer
[Google Developer Experts](https://developers.google.com/community/experts/directory)
(filter the map for “Web Technologies”) who may be able to provide expert
feedback. Contact DevRel via the alias above if any of this is of interest and
we will set you up. In all cases, a reasonable amount of time should be allowed
for responses to this process.
