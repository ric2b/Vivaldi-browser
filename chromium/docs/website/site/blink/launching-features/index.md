---
breadcrumbs:
- - /blink
  - Blink (Rendering Engine)
page_name: launching-features
title: Launching Features
---

If you have concerns about your feature not fitting into this process, or any
other questions, general concerns or discussion on this page or the process it
describes, please e-mail
[blink-api-owners-discuss@chromium.org](mailto:blink-api-owners-discuss@chromium.org).

## Exempt features

You do not need to use this process if your change is not exposed to web developers.
This means:

- Your change does not [affect web API
  behavior to the point that developers need to be aware of
  it](/blink/guidelines/web-exposed), e.g. no significant behavioral changes, and
  no web-facing API changes.
- Your change does not affect WebDriver API behavior. Although WebDriver changes
  are not observable by web pages, they are exposed to web developers running
  browser automation scripts. Thus, similar compatibility and interoperability
  concerns apply as for web-exposed features.

The rest of this document doesn’t apply to these types of changes, although such
features might still have to go through a different launch process. Large projects
should have public design docs that are also shared on blink-dev@chromium.org (or
chromium-dev@, for projects that have significant parts outside of Blink) for feedback
(this is also a good way to get the attention of other relevant leads).

## Frequently asked questions

**Q**: *Do I need any of this if my project is just refactoring or
re-architecting the code? Do the [API owners](/blink/guidelines/api-owners) need
to be involved?*

**A**: No. The API owners oversee the **process** of shipping [web-exposed](/blink/guidelines/web-exposed)
API changes. They are not necessarily leads or overseers of any of the code.
Instead, you should get the buy-in of the leads of the code areas touched by
your project. If there may be side effects of your change, you should follow the
"Architectural change" process below. In addition, such large projects should
have public design docs that are also shared on
[blink-dev@chromium.org](mailto:blink-dev@chromium.org) (or
[chromium-dev@chromium.org](mailto:chromium-dev@chromium.org), for projects that
have significant parts outside of third_party/blink) for feedback (this is also
a good way to get the attention of relevant leads you might not have thought
of).

For code-related questions, you can email
[platform-architecture-dev@chromium.org](mailto:platform-architecture-dev@chromium.org)
in addition to blink-dev@ as a catch-all option when the code ownership is not
clear or the feature needs large-scale refactoring changes.

**Q**: *What if I want to add some code to third_party/blink that is for a
non-web-exposed feature? Is that allowed?*

**A**: In general, no. On a case-by-case basis an exception could be given if
there is no other reasonable way to implement the feature. Ask for permission
from leads of the code area as well as the API owners. (The API owners need to
be involved only to help understand if the feature really is not web-exposed;
this can be a very subtle question.)

**Q**: *I am not sure of the right approach for my feature. What should I do?*

**A**: Please reach out to the API owners for help! While they are not
gatekeepers for everything, they are very happy to give advice and unblock your
feature. An email to
[blink-api-owners-discuss@chromium.org](mailto:blink-api-owners-discuss@chromium.org)
is the best way; if a public-facing email is not possible, please email the [API
owners](/blink/guidelines/api-owners) directly.

**Q**: *Do I need tooling support for my new feature?**

Shipping new Web Platform features in Chromium requires tooling support. Follow [the
DevTools support checklist](https://goo.gle/devtools-checklist) to figure out the
appropriate steps to take for your feature.

**Q**: *What if another browser or the [W3C TAG](https://tag.w3.org/) objects to
my new feature? Can I ship it anyway?*

**A**: Blink and Chromium do sometimes ship features over the objections of
other browsers or the TAG, though we prefer to build consensus around features
if possible. See the [Blink Values in
Practice](/blink/guidelines/web-platform-changes-guidelines/) for how we make
this tradeoff.

## The Feature Types {:#feature-types}

The first thing you will need to do is identify what type of feature you are
building:

### New feature incubation

This is the normal path when we are incubating and
defining new features from scratch - e.g., most
[Fugu](/teams/web-capabilities-fugu) features follow this pattern, and any
feature where we start without an already-defined standard for the feature.
This also covers changes that are extending existing features (e.g.,
defining a new value and behavior for an existing standard feature). This
type of feature has the most associated process steps, as it is charting new
territory.

### Implementation of existing standard {:#implementation-of-standard}

This type of feature has a
lighter-weight “fast track” process, but this process can only be used when
the feature has already been defined in a consensus standards document -
e.g. a W3C Working Group has already agreed on the design, it has already
been merged into a WHATWG standard, or the feature has already been
implemented in another engine.

### Deprecation

Removal of already-shipped features.

### Web developer facing change to existing code {:#change-to-existing-code}

This type is not commonly used.  It is basically a public
service announcement- “This is a web-developer-facing change to existing
code without API changes, but you may see side effects.” This may be due to
large-scale code refactoring or rewriting, where the goal is to cause no
behavioral changes (but due to scope of change, side effects are likely), or
this may encompass changes to current code that fix a bug or implement new
changes to the spec without changes to the API shape itself.

### Changing feature type

It is possible to change types later in the process - for example, if you start
out implementing an already existing standard, but discover you need to incubate
a new API during the process, you can start over with a new ChromeStatus entry.
Note that there are few [strictly required gates to the Chromium
process](/blink/guidelines/api-owners/procedures) (e.g., 3 LGTMs from API owners
on an intent-to-ship) - particularly in the earlier stages we want to encourage
experimentation. However, there are required fields for most stages in the
[ChromeStatus tool](https://www.chromestatus.com/), and it’s important to
consider all the steps listed below; this will maximize your chance of success
on an intent-to-ship and reduce the risk of having to redo parts of your design
or implementation.

## The Chromium process to launch a new feature {:#launch-process}

### Step 0: Create a ChromeStatus entry, and choose your feature type. {:#create-chrome-status}

For all types of features, the first step is to create a ChromeStatus entry. Go
to <https://www.chromestatus.com/features>, ensure you are logged in (see the
upper right corner), and click “Add new feature”. If you do no have access to
create a new feature, please send an email to
[webstatus-request@google.com](mailto:webstatus-request@google.com) to request
access. Follow the directions to name your feature and give a short summary, and
select the appropriate feature type.

For Chrome, some launches will require a formal Chrome launch
review (especially if your feature has security, privacy,
legal, or UI implications). You can work with a Google counterpart to get all
launch approvals. We are working on making this process more open and transparent
outside Google (new tooling is in progress).

From this point on, the process changes a little depending on the type of
feature you’re adding.

### For New Feature Incubations {:#new-feature-process}

#### Step 1: Incubating: Write up use cases and scenarios in an explainer {:#start-incubating}

Proceed to the "Start Incubating" stage in ChromeStatus.

Write up the use cases and scenarios for the feature in an
[explainer](https://tag.w3.org/explainers/), which is typically written in
[Markdown](https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax).
Then host your explainer at a public URL. Some organizations recommend specific
places:
* Google: <https://github.com/explainers-by-googlers>
* Microsoft: <https://github.com/MicrosoftEdge/MSEdgeExplainers>
* If your organization has an area for explainers, [send us a
  patch](https://source.chromium.org/chromium/chromium/src/+/main:docs/website/docs/CONTRIBUTING.md)
  to describe it here.

Otherwise, a public personal Github repo typically works well. Then put a link
to your public explainer in the feature entry.

We have a [program to provide mentorship for specification
writing](/blink/spec-mentors); If you are a Googler you must file a
[request](https://docs.google.com/forms/d/e/1FAIpQLSfCYE4U13GkZ11rAWBUjOb3Lza-u4m2k3TklQHXe7Zfn3Qo1g/viewform)
for a spec mentor, and ask them to review this early explainer before
proceeding. If you are not a Googler, you are welcome to make use of this but
not required to.

Then kick off "standards incubation" by proposing your explainer to a relevant
<a href="#incubation-venue">incubation venue</a> or working group and
socializing the problem with other vendors and developers. For web features, the
[WICG](https://github.com/WICG/admin#contributing-new-proposals) is a good
default, but ask your [spec mentor](/blink/spec-mentors) if you need help
picking a venue. Enter a reference to the public proposal in “Initial Public
Proposal” field. It’s also a good idea to discuss your idea with the team/team
lead (TL) or area expert in the feature area, prior to checking in code in the
area.

After some discussion with the community, start sketching out a proposed
solution in your (public) explainer, including sample code for using your API
design. You may wish to review this with your specification mentor before
proceeding to prototyping.

#### Step 2: Prototyping {:#prototyping}

Proceed to the “Start Prototyping” stage in ChromeStatus. Click the “API Owners”
button in that section and then click “Draft Intent to Prototype email” - this
will generate an email for you. Send that email to
[blink-dev](mailto:blink-dev@chromium.org) and start checking in prototype code
to Chromium under a runtime flag. You should do your detailed API design in the
open, in your public repository, and response to feedback filed there. You
should continue pushing for public engagement (from other vendors and web
developers), and to move into an <a href="#incubation-venue">incubation
venue</a> or working group if you haven’t already. During this stage, you should
expand on the explainer with a full design doc (this may also have
implementation-specific details), and consider creating a specification (or
writing up a pull request with changes to an existing spec).

Note that any CLs landing at this stage should be [behind a feature
flag](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/RuntimeEnabledFeatures.md).
Consider adding a [UseCounter](/blink/platform-predictability/compat-tools) to
track usage of your new feature in the wild, and be sure to write integration
tests for your feature as[
web-platform-tests](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_platform_tests.md)
as you go. Continue to work with your API mentor if there are any design
changes.

Ensure you have an API overview and descriptions for all IDL methods and
properties (these are probably in your specification or explainer, but
developers will need them to try your feature out), and at least a basic sample.

As soon as you have a functional and reasonably complete implementation of your
initial design ready for developers to try out behind a flag, proceed to the
next step.

#### Step 3: Feature Complete behind a feature flag: iteration on design {:#dev-trials}

Once you have a functional and reasonably complete feature implementation
available as a runtime enabled feature, we recommend (but don't require) that
you request an [Early Design Review](https://github.com/w3ctag/design-reviews/issues/new?assignees=&labels=Progress%3A+untriaged%2C+Review+type%3A+early+review&template=005-early-design-review.md&title=)
from the [TAG](https://www.w3.org/2001/tag/) and proceed to
the “Dev Trials” stage in ChromeStatus (note also exceptions enumerated
[here](/blink/guidelines/api-owners/process-exceptions/)). This will generate
a “Ready for Developer Testing” email that you should send to
[blink-dev](mailto:blink-dev@chromium.org) to notify the community they can
try out the feature. At this point, you should consider asking other browser
vendors and the web developer community for
[signals on their opinion of the API](https://docs.google.com/document/d/1xkHRXnFS8GDqZi7E0SSbR3a7CZsGScdxPUWBsNgo-oo/edit#heading=h.tgzhprxcmw4u).

This is the main iterating stage of feature development and helps you assess
product-market-fit early on before you corner yourself (does your API address a
problem with meaningful demand? did we get the ergonomics right?). You may wish
to send more than one Ready for Developer Testing emails, if you make
substantial changes to
the API shape while iterating. You should work with the TAG to complete their
review and address any issues raised during this stage, and should address any
issues raised by other horizontal reviews (accessibility, privacy,
internationalization, etc.).

[Flag guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/flag_guarding_guidelines.md) require you to place the code change behind a
[flag](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/RuntimeEnabledFeatures.md),
which enables a quick roll-back of that change in situations where we find our estimate
regarding its breaking potential was overly optimistic.

#### Step 4: Widen review {:#widen-review}

Once you believe you have addressed all major open issues, you should proceed to
the “Evaluating readiness to ship” stage in ChromeStatus.

<a id="incubation-venue"></a>By this point, your explainer and other public
specification work should have migrated to an incubation venue (or a full
working group), in order to assure contributors that IP concerns are being
handled appropriately, and so that all browser vendors are able to provide
structured feedback in a known place.
Any [W3C community group](https://www.w3.org/community/groups/) is acceptable,
as is publication as an [IETF internet draft](https://www.ietf.org/how/ids/).
For web features, the
[WICG](https://github.com/WICG/admin#contributing-new-proposals) is a good
default, but ask your [spec mentor](/blink/spec-mentors) if you need help
picking a venue.
If migrating the work to a CG is not appropriate for your case (e.g. you're
running an experiment you're not intending to ship as is), you can ask for an
exception from the API owners.

If you haven't already received [signals on their opinion of the
API](https://docs.google.com/document/d/1xkHRXnFS8GDqZi7E0SSbR3a7CZsGScdxPUWBsNgo-oo/edit#heading=h.tgzhprxcmw4u)
from other browser vendors and the web developer community, now is the time to
pursue getting those signals. You should ask at least a month ahead of sending
an Intent to Ship, to give reviewers sufficient time for meaningful feedback.

You should also work with the documentation team to ensure your features will be
documented when shipped, and estimate when (in what milestone) you would like to
target shipping.

You should also decide if an Origin Trial would help gather significant data for
your feature.

Now that you think your overall design is relatively settled (or just after
starting your Origin Trial), if you haven't already, start writing your
specification. Talk to your [spec mentor](/blink/spec-mentors) about how long
you should expect this to take. As a very rough guess, estimate at least a month
of full-time work, but that can take more calendar time than the 12-16 weeks of
an Origin Trial. Starting the spec-writing process too late might delay your
feature launch.

Once you have a complete specification:

1. If you have one, ask your spec mentor to [review the
   specification](/blink/spec-mentors/#reviewing-the-specification).
2. Request a [Specification
   Review](https://github.com/w3ctag/design-reviews/issues/new?assignees=&labels=Progress%3A+untriaged&template=010-specification-review.md)
   from the TAG (except in the cases noted
   [here](/blink/guidelines/api-owners/process-exceptions/)). You should submit
   this at least a month ahead of sending an Intent to Ship, to give the TAG
   sufficient time for meaningful feedback.

   Note that if the TAG takes several months to review the feature,
   and during that time the implementations and relevant standards
   bodies find consensus on the feature, the TAG's review might not
   be able to influence the design anymore. If that happens, it's best to notify the TAG so
   that they can close the issue and prioritize other issues.

#### Step 5 (Optional): Origin Trial or other Experiments {:#origin-trials}

If you want to gather data on the usability of your feature that an [Origin
Trial](/blink/origin-trials/running-an-origin-trial) can help collect, proceed
to the “Origin Trial” stage in ChromeStatus and fill out the required fields
detailing what you hope to learn from the origin trial. This will generate an
[Intent to
Experiment](https://docs.google.com/document/d/1vlTlsQKThwaX0-lj_iZbVTzyqY7LioqERU8DK3u3XjI/edit)
mail that you should send to [blink-dev](mailto:blink-dev@chromium.org). After
receiving at least [one LGTM](/blink/guidelines/api-owners/procedures) from the
API owners, you can proceed with your origin trial release. Collect data and
respond to any issues. From here, you may wish to return to Dev Trials, proceed
to Prepare to Ship, or park the feature.

As noted above, a specification is recommended but not required for an Origin
Trial.

Please note that neither Origin Trials nor other experiments are exempt from
requiring cross-functional approvals from the Chrome launch review process.

Depending on your feature and your experimentation goals, running a non-Origin
Trial experiment via Finch on a percentage of the user population may be
useful. Features that are going through the intent process should adhere to the
following guidelines:
* Canary, Dev, and Beta channel experimentation does not require API owners'
  approval.
* Experimentation on 1% of Stable channel population requires an Intent to
  Experiment approval. Note that an additional explanation for why this
  experimentation is needed is also required.
* Experimentation on higher percentages of Stable channel population (> 1%)
  should be considered exceptional, and requires 3 LGTMs from API owners.

Note that if your plan for shipping involves a gradual ramp-up on Stable channel,
this is not considered experimentation, and instead follows the usual Intent to Ship process.

An initial origin trial or experiment for a feature may only run for
*6 milestones of Chromium*. Each request to extend beyond that limit may only be
for *3 milestones* at a time, and will not be approved unless substantial
progress is demonstrated in all of these areas:
* Draft spec (early draft is ok, but must be spec-like and associated with the
  appropriate standardization venue, or WICG)
* TAG review (see [exceptions](/blink/guidelines/api-owners/process-exceptions/))
* bit.ly/blink-signals requests
* Outreach for feedback from the spec community
* WPT tests

Each subsequent request to extend an origin trial must provide substantial
*additional* progress on top of the previous extension request.

*Technical note:* Origin Trials are nowadays "gapless", which means they are
configured to end after the version of Chrome with the feature enabled by default
is available to most users. The current convention for that is that the trial ends
when the Chrome milestone after the one where the feature is enabled by default
ships. This technicality will only be relevant to you if you intend to pause the
experiment after the trial is over and restart it later, e.g. for very early stage
experiments where there is certainty that they will be revised before shipping. In
such cases you can enforce a break in the trial to demonstrate lack of usage before
restarting the experiment. Email
[origin-trials-support@google.com](mailto:origin-trials-support@google.com) to
discuss this option.

#### Step 6: Prepare to Ship {:#new-feature-prepare-to-ship}

By this stage, you need to have a complete specification available that matches
what you have implemented, and you should have given the TAG and [other
browser vendors](https://bit.ly/blink-signals) at least a month to
comment on that specification. If you are a Googler you should get a final spec
review from your [spec mentor](/blink/spec-mentors), and discuss options for
moving your spec to a final standardization venue. You should get final signoff
from Documentation.

If the TAG has commented on your review, and the state of the review is anything
other than [`Resolution:
satisfied`](https://github.com/w3ctag/design-reviews/issues?q=is%3Aissue+label%3A%22Resolution%3A+satisfied%22),
the API Owners should be able to see evidence that you've seriously and
comprehensively engaged with their comments, and tried to resolve any concerns.
If the TAG has _not_ commented, then after your I2S is approved, it's courteous
to post to the review saying that Chromium considers the feature stable, and
future proposals for changes will be weighed against the compatibility risk of changing a
shipping feature.

If your specification is still in an [incubation venue](#incubation-venue) and
not a working group, propose that the feature migrate to a working group.
There's no requirement that a working group adopt the feature before Chromium
ships it, but if we're ready to ship it, we need to declare publicly that we
think incubation is finished. Ask your [spec mentor](/blink/spec-mentors) for
help with this.

If there is a relevant community for your specification, you should have
evidence that you asked that community to discuss the latest specification for
the feature and that either it has consensus or you tried to achieve consensus
and took action on all actionable feedback. For example, if you're developing an
extension to [CSS](https://drafts.csswg.org/), you should ask the [CSS
WG](https://www.w3.org/Style/CSS/) to consider it and give feedback. If the
relevant community decides not to discuss the feature, that doesn't necessarily
block shipping.

If your specification has more than a handful of open issues, consider labelling
those issues such that it is clear which of them are editorial, which are
asking for future enhancements, and which would require breaking changes that
may pose a compatibility risk. If you don't already have appropriate labels,
consider using `editorial`, `enhancement`, and `compatibility-risk`

Update ChromeStatus with:
* a target milestone for shipping (and remember to keep this updated, if things
  change) and
* any changes in vendor signals.

Proceed to the “Prepare to Ship” stage in ChromeStatus. Use the buttons at the
top of this section to request reviews for gates such as privacy and security.
The “API Owners” button will give you the option to generate an “Intent to Ship”
mail that you should send to [blink-dev](mailto:blink-dev@chromium.org). If your
specification isn't a modification of an existing specification, include a
one-line [spec maturity
summary](/blink/spec-mentors/#reviewing-the-specification) from someone outside
your team (like your [spec mentor](/blink/spec-mentors)) who has done a review.
This will spark a conversation with the API owners; address any feedback from
them, and once you get [3 LGTMs from the API
owners](/blink/guidelines/api-owners/procedures), you may enable the feature by
default. You can learn more about the policies and guidelines the API owners
evaluate [here](/blink/guidelines). Requirements for new API owners are
[here](/blink/blink-api-owners-requirements). Email
blink-api-owners-discuss@chromium.org or reach out to one of the[ API
owners](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/API_OWNERS)
if there is no open/unaddressed feedback and you are still blocked on LGTMs
after 5 days.

Once you have the needed approvals, set the implementation status to "Enabled by default" in
ChromeStatus.
You will likely also need to land a Chromium CL to turn on the feature.
Link to the blink-dev thread via the [Google Groups web interface](https://groups.google.com/a/chromium.org/g/blink-dev)
in your CL's commit message to make it easy and clear to review.

### Implementations of already-defined consensus-based standards {:#process-existing-standard}

#### Step 1: Write up use cases and scenarios, start coding {:#existing-standard-use-cases}

Fill out the “Motivation” section with a brief summary, and then write up the
use cases and scenarios. If this is a large feature, or a combination of
multiple attributes/properties/methods/events, you may wish to do this in a
separate explainer file.

Proceed to the “Start Prototyping” stage in ChromeStatus - this will generate an
“Intent to Prototype” mail for you. Send that email to
[blink-dev](mailto:blink-dev@chromium.org) and start checking in prototype code
to Chromium as [runtime enabled features](/blink/runtime-enabled-features).
Ensure there are adequate
[web-platform-tests](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/web_platform_tests.md)
for this feature. Also ensure you have an API overview and descriptions for all
IDL methods and properties (this is probably already in the consensus standard
specification, or even on MDN), and at least a basic sample.

As soon as you have a functional and reasonably complete implementation of the
feature ready for developers to try out under a flag, proceed to the next step.

#### Step 2: Feature Complete behind a flag and implementation refinement {:#existing-standard-dev-trials}

If the TAG has not already reviewed the consensus specification, request a
[Specification Review](https://github.com/w3ctag/design-reviews/issues/new?assignees=&labels=Progress%3A+untriaged&template=010-specification-review.md)
(except in the cases noted
[here](/blink/guidelines/api-owners/process-exceptions/))
and proceed to the “Dev Trials” stage in ChromeStatus. This will generate a
“Ready for Developer Testing” email that you should send to
[blink-dev](mailto:blink-dev@chromium.org) to notify the community they can
try out the feature.

After you have addressed any issues that the community finds, you should proceed
to the “Evaluating readiness to ship” stage in ChromeStatus. You should also
work with the documentation team to ensure your feature will be documented when
shipped, and estimate when (in what milestone) you would like to target
shipping. You should also decide if an Origin Trial would help gather
significant data for your feature.

#### Step 3 (Optional): Origin Trial {:#existing-standard-origin-trials}

The [Origin Trial guidelines](#origin-trials) for new feature
incubations applies here as well.

#### Step 4: Prepare to Ship {:#existing-standard-prepare-to-ship}

You should update ChromeStatus with a target milestone for shipping (and
remember to keep this updated, if things change). If you are working with DevRel
on documentation, this is the time to finish that work and update for any
changes in vendor signals.

Proceed to the “Prepare to Ship” stage in ChromeStatus; this will generate an
[Intent to
Ship](https://docs.google.com/document/d/1vlTlsQKThwaX0-lj_iZbVTzyqY7LioqERU8DK3u3XjI/edit#bookmark=id.w8j30a6lypz0)
mail that you should send to [blink-dev](mailto:blink-dev@chromium.org). This
will spark a conversation with the API owners; address any feedback from them,
and once you [get 3 LGTMs](/blink/guidelines/api-owners/procedures) from the API
owners, you may enable the feature by default. See[
here](/blink/guidelines/web-platform-changes-guidelines) for the principles the
API owners will apply in evaluating whether the feature is mature enough to
ship. (API owners requirements are listed[
here](/blink/blink-api-owners-requirements) - email
blink-api-owners-discuss@chromium.org or reach out to one of the[ API
owners](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/API_OWNERS)
if no open/unaddressed feedback and you are still blocked on LGTMs after 5
days.)

Once you have the needed approvals, set the implementation status to "Enabled by default" in
ChromeStatus.
You will likely also need to land a Chromium CL to turn on the feature.
Link to the blink-dev thread via the [Google Groups web interface](https://groups.google.com/a/chromium.org/g/blink-dev)
in your CL's commit message to make it easy and clear to review.

### Feature deprecations

Deprecating/removing features creates a heavy burden for developers and users.
Broken sites might hurt developers’ livelihood or prevent users from completing
vital tasks. Ambiguous removal timelines confuse developers and make it hard for
them to respond appropriately.

Deprecations and removals must have strong reasons, explicitly balanced against
the cost of removal. Deprecations should be clear and actionable for developers.
When suggesting a deprecation/removal, be ready to answer these questions:

* Why are we removing this feature?
  * Security
  * Code health
  * Interoperability
  * Known bad practice that leads to unintuitive performance problems, bad user
    experience, etc.
  * We shouldn't remove APIs that have small value on the path towards a removal
    that has significant value. For example, getting rid of attribute nodes
    *is* valuable and would benefit the platform. Getting rid of half the
    attribute node methods is not. So we should evaluate the usage of all the
    APIs we need to remove together in order to get there.
* What is the cost of removing this feature?
  * Use counter data: Try to estimate how many users would be inconvenienced.
    There is no threshold for which removal is necessarily safe.
    * Compare with similar removals in the past.
    * Will removal lead to a broken user experience?
    * You can still remove a feature with higher usage if the need is great enough.
    * For older, widely implemented APIs, the cost of removing an API is not
      accurately reflected by the UseCounter. It's more likely that there's a
      longer-tail of legacy content that we're breaking. We should be more
      hesitant to remove older, widely implemented APIs.
  * HTTP Archive: Find and analyze affected sites. What will the typical effect of removal be?
    * If feature detection is almost always used, that makes removal less risky.
* When will the feature be removed?
  * Unless there is a specific, compelling reason for why a feature should be
    removed immediately, it should be deprecated for at least 1 milestone before
    removal.
  * Depending on the degree of usage/breakage, more milestones may be warranted.
  * If we remove a related collection of things, we should remove them all in
    the same release. Breaking people once is better than breaking them
    repeatedly in small ways.
* What is the suggested alternative?
  * There should be another way for developers to achieve the same functionality

High-usage APIs may require much more work to land successfully. See
[here](https://docs.google.com/document/d/1-_5MagztiYclsMccY4Z66XI465WaasT5I5y2dnhRvoE/edit#heading=h.n49iymp16hl6)
for a good example of how this worked in practice with the deprecation and
removal of the Web Components v0 APIs.

#### Step 1: Measure usage {:#deprecation-measure}

Measure feature usage in the wild. [Various
tools](/blink/platform-predictability/compat-tools) are available, but typically
this is accomplished by adding your feature to the [`UseCounter::Feature`
enum](https://code.google.com/p/chromium/codesearch#chromium/src/third_party/WebKit/Source/core/frame/UseCounter.h&sq=package:chromium&type=cs&q=file:UseCounter.h%20Feature)
and adding `MeasureAs=<your enum value>` to the feature's IDL definition.

It takes 5-9 weeks (based on 2023's typical 4-week release cycle) to hit Stable
once you enable instrumentation. If there is time pressure you may be able to
get permission to merge UseCounter changes into an existing dev/beta branch, and
make provisional decisions based on data from beta channel.

#### Step 2: Coordinate with other engines {:#deprecation-coordinate}

If multiple browser engines support the feature, talk to decision-makers at the
other engines and to spec editors in order to ensure that the feature is removed
in a coordinated way. We usually don't want to try to remove a feature from
Chromium if Gecko and WebKit are going to keep supporting it. Once you have a
coordinated removal plan, [file standards position
issues](https://bit.ly/blink-signals) to document the agreement.

<a id="deprecation-motivation"></a>
#### Step 3: Deprecate the feature {:#deprecate}

Proceed to the “Write Up Motivation” stage in ChromeStatus.

Fill out the “Motivation” section with an explanation of why you want to remove
this feature and why you think you'll be able to remove it. Make sure to provide
suggested alternatives to the feature being deprecated.

Consider whether your feature is likely to need a [deprecation
trial](#deprecation-trial), and if so, describe what you know about the
deprecation trial. You'll be able to correct this later if you learn something
new.

Also describe the conditions under which you plan to proceed to removing the
feature. For example, you might expect that just running [dev
trials](#deprecation-dev-trials) for several milestones will be enough, or you
might only want to remove if measured usage drops to a certain level. Again,
you'll be able to change your mind later by sending another email and waiting
for the API owners to agree to it.

Have ChromeStatus generate an “Intent to Deprecate and Remove” mail for you,
send it to [blink-dev](mailto:blink-dev@chromium.org), and wait for 3 LGTMs.

Once [the API owners have given 3
LGTMs](/blink/guidelines/api-owners/procedures/) for your removal plan, start
checking in your code to add a [runtime enabled
feature](https://chromium.googlesource.com/chromium/src/+/HEAD/third_party/blink/renderer/platform/RuntimeEnabledFeatures.md)
that turns off the deprecated feature. As soon as you have a functional removal
of the feature ready for developers to try out under a flag, proceed to the next
step.

##### Deprecating without removal

If you are unsure of when a feature could be removed, or would like to
discourage usage, you may deprecate a feature without a removal deadline. This
is strongly discouraged and will require significant justification:

* Email blink-dev using the ["Intent to Deprecate" template](https://docs.google.com/a/chromium.org/document/d/1Z7bbuD5ZMzvvLcUs9kAgYzd65ld80d5-p4Cl2a04bV0/edit).

* [1 LGTM](/blink/guidelines/api-owners/procedures) necessary from the API owners

* Must justify why there is no removal date

#### Step 4: Dev trial of deprecation {:#deprecation-dev-trials}

Proceed to the “Dev Trial of deprecation” stage in ChromeStatus. This will
generate a “Ready for Developer Testing” email that you should send to
[blink-dev](mailto:blink-dev@chromium.org) to notify the community they can try
out the feature deprecation.

At this point, you should also notify developers by adding a deprecation issue
as outlined [here](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/frame/deprecation/README.md).
Give developers as many milestones as possible to respond to the deprecation.

You should work with the documentation team to ensure the community is prepared
for this feature deprecation, and estimate when (in what milestone) you would
like to target shipping. You should also decide (possibly based on data from the
dev trial) if a deprecation trial is going to be necessary to help smooth the
removal of this feature from the web platform.

#### Step 5 (Optional): Deprecation Trial {:#deprecation-trial}

If you are concerned that there are going to be web developers who need
additional time to fix up their implementations, and will want to delay your
feature deprecation, you can file for a [Deprecation
Trial](https://developer.chrome.com/docs/web-platform/origin-trials/#deprecation-trials).
This will let you disable the feature by default, but let developers request an
origin trial token to re-enable the feature, for a limited period of time after
the feature deprecation. You will need to decide how long to keep the
deprecation trial open and enter that milestone in the tool for planning
purposes.

Deprecation trials are especially useful to support enterprise users, who have
additional difficulties reacting to breaking changes. See [shipping changes that
are enterprise-friendly](/developers/enterprise-changes) for best practices.

If the details are different from what you wrote in your “Intent to Deprecate
and Remove” thread, select “Draft Request for Deprecation Trial email” in
ChromeStatus, and send the resulting “Request for Deprecation Trial” email to
[blink-dev](mailto:blink-dev@chromium.org). If the original details were
correct, the “Intent to Deprecate and Remove” thread is sufficient.

After your deprecation trial plan has received at least [one
LGTM](/blink/guidelines/api-owners/procedures) from the API owners, request a
new trial at ChromeStatus by clicking the "Request Trial Creation" button in
the Origin Trial section. Ensure your removal is integrated with the origin
trials framework ([see
details](/blink/origin-trials/running-an-origin-trial#integrate-feature)).
Once your Deprecation Trial is in place, proceed to the next step.

Deprecation trials are run for up to 6 chromium milestones, and can be
repeatedly extended for 6 milestones as long as significant progress is
demonstrated in at least one of the following areas:
 * Designing and shipping alternative APIs for the use case, directly addressing
   feedback from developers currently relying on the deprecation trial.
 * Driving the number of origins signed up for the deprecation trial down to
   zero.

Requests for a deprecation trial or deprecation trial renewal that extend beyond
these limits will require sufficient justification and 3 LGTMs from API owners.

If significant progress cannot be demonstrated towards removing the need for the
deprecation trial, it will be necessary to un-deprecate the API to acknowledge
the reality that ongoing support for the API is required for web compatibility.

#### Step 6: Prepare to Ship {:#deprecation-prepare-to-ship}

You should update ChromeStatus with a target milestone for disabling the
feature (and remember to keep this updated, if things change). You should get
final signoff from the Documentation team.

Proceed to the “Prepare to Ship” stage in ChromeStatus. If any details have
changed from what you described in your “Intent to Deprecate and Remove” thread,
update that thread with the new details, and wait for 3 API owners to confirm
that their LGTMs still apply. Otherwise, you can keep following your approved
plan without new LGTMs.

You will likely also need to land a Chromium CL to turn on the feature.
Link to the blink-dev thread via the [Google Groups web interface](https://groups.google.com/a/chromium.org/g/blink-dev)
in your CL's commit message to make it easy and clear to review.


#### Step 7: Disable the feature {:#disable-feature}

Disable the feature by default. Update ChromeStatus to either “Disabled” or
“Disabled with Deprecation Trial”.

#### Step 8: Wait for the removal to land {:#wait-for-landing}

If you are running a Deprecation Trial, wait until the Deprecation Trial period
has ended.

Monitor developer chatter and bug reports for at least a month or two after your
removal has gotten to Stable and any Deprecation Trial has ended to make sure
it's landing well. If the removal goes particularly badly, you may need to turn
the feature back on using the runtime enabled feature you added
[above](#deprecate).

If you need to extend the Deprecation Trial, notify
[origin-trials-support@google.com](mailto:origin-trials-support@google.com),
click “Generate an Intent to Extend Deprecation Trial” in ChromeStatus, and send
the resulting notification to blink-dev. Then repeat this step.

#### Step 9: Remove Code {:#remove-code}

Once it's clear that developers are no longer relying on the disabled feature,
remove the code, and set the ChromeStatus to “Removed.”

### Web-developer-facing change to existing behavior {:#behavior-changes}

#### Before you begin: Assess backwards compatibility {:#behavior-change-compat}

Before choosing this feature type, you should determine if the behavior change you're trying to
make can be considered a low-risk bug fix, or it has non-negligible
likelihood to result in site breakage.

The following criteria are worthwhile to consider:

* Can the change cause user-visible or functional breakage?
  - Is it possible that existing code relies on the current behavior?
  - What would that coding pattern look like? How likely it is that this
  coding pattern is used in the wild? Our collection of
  [compat tools](https://www.chromium.org/blink/platform-predictability/compat-tools/)
  can help with such an assessment.
  - Non-user-visible breakage (e.g. breakage in reporting or monetization) is
 still considered functional breakage.
* What are other browser engines doing?
  - Would this change align Chromium's behaviour with other vendors? Or
  would Chromium be the first to roll-out this behavior change?
* What is the usage of the feature being changed (typically measured with
  a [UseCounter](https://chromium.googlesource.com/chromium/src.git/+/HEAD/docs/use_counter_wiki.md))?
* How likely is breakage in sites that only support Chromium browsers?
(enterprise environments, kiosks, etc)

After considering these factors, choose the "New feature incubation"
or "Existing feature implementation" feature types
for any change with some potential to break sites. Use "Web developer facing code change"
only for changes deemed very unlikely to break sites, and that change APIs in at most a trivial way.

Another reason for a PSA could be a large-scale refactoring that
doesn't *intend* to result in behavior changes, but may do so in practice.

The purpose of a PSA is to notify the broader Chromium community about
the change, and enables folks to test against it and potentially
re-examine the risk assessment regarding potential breakage.

#### Step 1: Write up motivation {:#behavior-change-motivation}

Fill out the “Motivation” section in ChromeStatus with a brief summary of the
reasons we want to modify the current behavior.

#### Step 2: (Optional) Dev trial {:#psa-dev-trial}

If you want developers to try out this change before shipping it (e.g.,
to assess potential breakage), put the relevant code behind a [runtime
enabled feature](/blink/runtime-enabled-features), and set the
status to “Dev Trial” in ChromeStatus. This will generate a “Ready for
Developer Testing” email that you should send to
[blink-dev](mailto:blink-dev@chromium.org) to notify the community they
can try out this change.

#### Step 3: Prepare to Ship {:#psa-prepare-to-ship}

You should update ChromeStatus with a target milestone for shipping (and
remember to keep this updated, if things change). Proceed to the “Prepare to
Ship” stage in ChromeStatus; this will generate a “Web-Facing Change PSA” mail
for you. Send that email to [blink-dev](mailto:blink-dev@chromium.org) with the
summary of the code change and the expected milestone.

For a PSA you're expected to follow the [flag guarding
guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/flag_guarding_guidelines.md),
which generally allow for some discretion for trivial changes.

You will likely also need to land a Chromium CL to turn on the feature.
Link to the blink-dev thread via the [Google Groups web interface](https://groups.google.com/a/chromium.org/g/blink-dev)
in your CL's commit message to make it easy and clear to review.


You may wish to use [Finch](http://go/finch) to increase confidence in the new
code as you deploy it.

## Post Launch

After launching a new feature, watch for crashes, regressions caused by your
feature and any substantive spec feedback. Review [UseCounter](/blink/platform-predictability/compat-tools) and other metrics.
Update the intent-to-ship thread and ChromeStatus if non-trivial issues
(like web compatibility or serious design questions) arise. When in doubt,
email blink-dev@ (or blink-api-owners-discuss@ if you prefer a smaller
audience) for advice.

Once a new API is on by default, continue to support other implementations
(for instance, clarifying the spec, improving the tests, fixing bugs, and
updating support status in ChromeStatus) until the feature is broadly
supported and works the same across engines. Remember, your job is to move
the web forward, not simply add features to Chrome.

Review MDN docs for technical accuracy and clarity. Feel free to make edits
directly or reach out to jmedley@.

When you are convinced enabling the feature by default has “stuck”
(typically 2 milestones), remove any unused code including
RuntimeEnabledFeatures entries.

## Related

For an overview, and explanation of motivations, please see:

*   Video presentation: [Intent to explain: Demystifying the Blink
            shipping process (Chrome Dev Summit
            2019)](https://www.youtube.com/watch?v=y3EZx_b-7tk&list=PLNYkxOF6rcIDA1uGhqy45bqlul0VcvKMr&index=17)
*   Blog post: [Intent to Explain: Demystifying the Blink Shipping
            Process](https://blog.chromium.org/2019/11/intent-to-explain-demystifying-blink.html)
*   BlinkOn Talk, 2021: [Risky Business: Launching Faster In Blink](https://www.youtube.com/watch?v=1Z83L6xa1tw)
