---
breadcrumbs:
- - /blink
  - Blink (Rendering Engine)
- - /blink/launching-features
  - Launching Features
page_name: wide-review
title: Getting Wide Review
---

To help the [API owners](/blink/guidelines/api-owners) follow the
[guidelines for Web Platform changes](/blink/guidelines/web-platform-changes-guidelines),
every intent to ship has to show what external stakeholders (in particular the
[W3C TAG](#tag), the [non-Chromium browser engines](#standards-positions), and
[web developers](/blink/launching-features/wide-review/web-developers)) think
about the new feature. These signals are only a few inputs that the process
takes into consideration. None of them are either a veto nor an implicit
approval to ship.

This page describes how to collect that evidence. See the [launch
process](/blink/launching-features) when to request reviews.

## Exceptions {:#exceptions}

Some changes are too small and non-controversial to justify the effort of [TAG
review](#tag) or [requesting standards positions](#standards-positions). Whether
a feature is in this category is up to the discretion of the API owners during
their [approval of an intent to ship](/blink/guidelines/api-owners/procedures/).
You can ask ahead of time via
[blink-api-owners-discuss@chromium.org](https://groups.google.com/a/chromium.org/g/blink-api-owners-discuss/).
Mark these as "Not applicable" and "No signals".

Consensus features in certain working groups don't need either [TAG
reviews](#tag) or [standards positions from other browser
engines](#standards-positions):

* ECMAScript features at [stage 3](https://tc39.es/process-document/) or higher.
* WebAssembly features at [phase
  4](https://github.com/WebAssembly/proposals/blob/master/README.md#phase-4---standardize-the-feature-wg)
  or higher.
* Khronos features developed under its [Working Group
  Guidelines](https://www.khronos.org/files/working-group-guidelines.pdf), for
  which the specification has been committed to the relevant Khronos standard &
  reflected on the Khronos website.
  ([documentation](https://groups.google.com/a/chromium.org/g/blink-api-owners-discuss/c/CVkhZNVPlSg))

Other features don't need to request a [TAG review](#tag) but still need to
request [standards positions](#standards-positions):

* The intent ships a new API or augmentation of an API, or changing an API to
  match a spec, that is
  * already specified and accepted by the relevant standardization body, and
  * has already shipped in at least one other browser.
* The intent removes a feature or part of a feature.
* The intent is for WebDriver / WebDriver BiDi support for a feature already
  shipped in Chromium.

## TAG review {:#tag}

The [W3C TAG](https://www.w3.org/2001/tag/) is most effective when it can
suggest changes early in a feature's design. To get this input at the right
time, try to [request an "early design
review"](https://github.com/w3ctag/design-reviews/issues/new/choose) when you
have a solid description of the problem you're trying to solve, a collection of
alternative solutions, and one solution prototyped to demonstrate that it's
viable. This is likely around when you [start developer
trials](/blink/launching-features/#dev-trials).

When you have a complete specification for your feature, you should give the TAG
another opportunity to review it, unless they said not to in the early design
review. If they didn't close the early design review, or if your final
specification does not differ in any significant way from the the form reviewed
by the TAG, just comment on the early design review saying that a specification
is ready. No need to reopen the issue in that case. Otherwise (the early design
review is closed and the design has changed in any significant way), [open a new
"specification
review"](https://github.com/w3ctag/design-reviews/issues/new/choose).

<div id="slow-tag-review">

The TAG sometimes leaves a review open for several months, during which the
relevant stakeholders find consensus or Chromium ships the feature. If that
happens, it's best to notify the TAG so that they can close the issue and
prioritize other issues. If Chromium ships without wide consensus, it's still
polite to inform the TAG that Chromium considers the feature stable, and future
proposals for changes will be weighed against the compatibility risk of changing
a shipping feature.

</div>

## Browser engine standards positions {:#standards-positions}

Request formal standards positions from the Gecko and WebKit browser engines
around when a specification goes to [Origin
Trial](/blink/launching-features/#origin-trials) or [when you believe the
feature is in close to its final form](/blink/launching-features/#widen-review),
at least a month before you plan to send your intent to ship.

For cases where the signal is not Shipping/Shipped or In Development, the
[Official Standards Signal Process](#signal-process) (see next section) for that
implementation should be followed to obtain a signal. If the Official Standards
Signal Process is skipped or incomplete and N/A or No Signal is reported, it
should be reported why. Reasons why can include:

*   The feature has an [exception](#exceptions).
*   The Official Standards Signal Process was followed but no response came in a
    reasonable timeframe

For each of Gecko and WebKit, state one of the following signals:

<table>
  <thead>
    <tr>
      <th>Signal
      <th>Required evidence
  </thead>
  <tr>
    <td>No Signal
    <td>Link to Official Standards Signal Process issue or thread that is not
    yet complete
  <tr>
    <td>Shipped/Shipping
    <td>Link to public documentation or bug/issue that demonstrates the issue
    has shipped (i.e., an issue that links to patches that have been merged, or
    a comment that a previously disabled feature is now enabled by default).
  <tr>
    <td>In Development
    <td>Link to public documentation or bug/issue clearly indicating feature is
    in development (not just filed as a bug or under consideration). When in
    doubt, follow the Official Standards Signal Process
  <tr>
    <td>Positive
    <td>Statement made through an Official Standards Signal Process for that
    implementation
  <tr>
    <td>Neutral
    <td>Statement made through an Official Standards Signal Process for that
    implementation
  <tr>
    <td>Negative
    <td>Statement made through an Official Standards Signal Process for that
    implementation
  <tr>
    <td>Defer
    <td>Statement made through an Official Standards Signal Process for that
    implementation
  <tr>
    <td>N/A
    <td>Indication of why an Official Standards Signal is not needed in this
    case
</table>

### Official Standards Signal Process {:#signal-process}

#### Gecko

[File a Mozilla standards position issue.](https://github.com/mozilla/standards-positions)
The
[possible outcomes](https://github.com/mozilla/standards-positions/blob/main/README.md)
of this are: **positive**, **neutral**, **negative**, **defer**, and **under
consideration**. Link to the issue in the Implementation Signals List.

#### WebKit

[File a WebKit standards position issue](https://github.com/WebKit/standards-positions/),
and link to the issue in the Implementation Signals List. The
[possible outcomes](https://github.com/WebKit/standards-positions/) of this are:

*   support ⇒ Positive
*   neutral ⇒ Neutral
*   oppose ⇒ Negative
*   not considering ⇒ No Signals
*   blocked ⇒ No Signals
*   duplicate ⇒ usually use the URL and signal of whatever position this is a
    duplicate of
*   invalid ⇒ If you don't understand why the position is "invalid", ask your
    [spec-mentor](/blink/spec-mentors/) for help.
