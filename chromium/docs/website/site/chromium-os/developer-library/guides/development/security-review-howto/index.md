---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - Chromium OS > Developer Library > Guides
page_name: security-review-howto
title: ChromeOS security review HOWTO
---

This document describes the ChromeOS security review process. It's aimed at
feature owners, tech leads, and product managers.

## Goals

The ChromeOS security review process' main goal is to ensure new features stay
faithful to ChromeOS' security architecture. More details on what the review
covers are described below in the [review framework] section, but at a high
level the goal is to protect our users' data and devices. The security review
process is designed to make sure that new features don't make it
disproportionately easier for attackers to compromise user data or devices.
In general, every ChromeOS feature requires a security review.

## The security review process

ChromeOS development is structured around four-week cycles called *milestones*.
Every four weeks a new release branch is created, based off our main development
branch (also known as *trunk* or *tip-of-tree*). Accordingly, a new milestone is
pushed to ChromeOS devices every four weeks. It takes seven to eight weeks from
the time a branch is cut to the time a new software image built from that
branch is pushed to devices on the stable channel.

A feature targeting a given milestone will be reviewed during that milestone's
development cycle. The ChromeOS security team tracks features by looking at
*Launch bugs* filed in [crbug.com]. **As long as the feature has an associated
launch bug, the security team will track it**. The new launch bug template
allows feature owners to initiate a security review by flipping the
*Launch-Security* flag to *ReviewRequested*. The security team tracks this flag
as well.

In order to streamline the process as much as possible, make sure that the
launch bug links to a design doc that includes a section covering the security
implications of the feature. **The rest of this document describes what
questions such a "Security considerations" section should answer and what
concerns it should address**.

Launch bugs include a set of cross-functional review flags, which includes the
*Launch-Security* flag mentioned above. The security team will flip this flag
to *Approved* after the feature owner has successfully engaged the security team
to understand (and address or mitigate) the security implications of the
feature. **Don't think of the security review process as an arbitrary bar set by
the security team that you have to pass no matter what**. Instead, think of it
as the process by which you take ownership of the security implications of the
feature, so that you are shipping something that doesn't detract from the
overall security posture of the product.

Even if you consider that the feature is trivial, or has no security
implications, **please refrain from flipping the security flag in the launch bug
 to *NA* or *Approved* yourself**. In most cases, features are not as trivial as
they initially appear. More importantly, the security team uses these flags to
track features and work on our side. We will flip the security flag to
*Approved* when the feature is ready.

Fast-track features will still require a full security review if the feature is
modifying more than one component, where *component* can be understood as a
single process or service. For example, if the feature is modifying two things
that communicate over IPC, it would require a full security review. In other
words, modifying a single component is a necessary but *not sufficient*
condition for a feature to be fast-tracked from a security perspective. Many
features that modify a single component are complex enough to require a full
security review.

If the feature is big or complex, or if you find yourself implementing something
that needs to go against the recommendations in this document, please reach out
to the security team as soon as possible. Send email to
[chromeos-security@google.com], and try to include a design doc, even if it's
just an early draft. **When in doubt, just reach out**. We are always happy to
discuss feature design. You can also book [office hours] for any questions or
discussions.

The ChromeOS security team will normally not look at the implementation details
of a feature -- there is just not enough time to read through thousands of lines
of code each milestone. Instead, we prefer to focus on ensuring that the design
of the feature is contributing to, rather than detracting from, the overall
security posture of the system. The reason for this is two-fold: first, the time
constraints mentioned before. Second is the fact that even with careful review
bugs will likely slip through, and a sound, defensive design will ensure that
these bugs don't end up being catastrophic. For particularly risky code we can
always contract out a security audit.

**The expectation is that the feature will be security-complete (e.g. the new
system service will be fully sandboxed) by the time the branch is cut**. Merging
CLs that implement security features to release branches is risky, so we avoid
it.

If the feature is enabled by default, **the security flag in the launch bug
tracking the feature must be flipped before branch point**. This means that all
the relevant information (e.g. a design doc with a "Security considerations"
section) must be available well before the branch is cut, to give time for the
security team to review the feature and flip the flag before branch point.

If the feature is kept behind a flag, the security bit in the launch bug must
be flipped before the flag is enabled by default. This means that the feature
must be security-complete by the time the flag is enabled by default. Even in
this case we strongly recommend tackling security work earlier rather than
later. It's normally not possible to address security concerns in a feature
that's complete without requiring costly refactoring or rearchitecting.

In general, a feature that properly addresses the questions and recommendations
in this document can expect to have its security flag flipped by branch point.

### Life of a security review

This section describes how we use launch bug labels to determine the status of a
given launch bug and its progression through various states. It's useful context
for feature owners to be able to request reviews and supply information properly
and to clarify expectations on what should happen next at any point in time.
Similarly, ChromeOS security team members find a detailed description of their
role and responsibility in the review process:

1.  The process only kicks off once the *Launch-Security* flag set to
    *ReviewRequested*. If the feature isn't marked as such, it might not be
    ready for review. Feature owners are responsible for setting
    *ReviewRequested* and should make sure to supply a design doc with a
    "Security considerations" section before doing so.
2.  If the *Launch-Security* flag is set to *ReviewRequested*, the first thing
    the security reviewer will do is check for a design doc. If there isn't one,
    they will generally ask for one and flip the *Launch-Security* flag to
    *NeedInfo*. The feature owner should flip the flag back to *ReviewRequested*
    when the design doc is ready for review.
3.  If the design doc doesn't have a *Security Considerations* section, the
    security reviewer may ask for one. Offer a link to the [review framework] as
    a guide for how to write that section. Flip the *Launch-Security* flag to
    *NeedInfo*. The feature owner should flip the flag back to *ReviewRequested*
    when the design doc is ready for re-review.
4.  The security reviewer should use the [review framework] to evaluate if the
    feature is respecting security boundaries, handles sensitive data
    appropriately, etc. It's often also a good idea to consider existing
    features, in particular their security design and trade-off decisions made
    in previous reviews.
5.  If any security-relevant aspects are unclear or if there are concerns,
    the security reviewer should communicate this back to the feature owner via
    design doc comments and comment on the launch bug to clarify the security
    review status. Flip the *Launch-Security* flag back to *NeedInfo*.
6.  Security reviewers are encouraged to surface controversial
    design/implementation choices or aspects you're unsure about in the weekly
    ChromeOS security team meeting. This is useful so the rest of the team can
    suggest useful alternative angles and to provide historical context and
    high-level guidance on ChromeOS security philosophy.
7.  If necessary, the security reviewer and feature owner will iterate to
    resolve any questions or concerns. Use whatever means of communication seems
    most appropriate to make progress. For simple questions, document comments
    or email threads will work. For in-depth discussion of product requirements,
    design choices, and implications on security assessment, it's usually better
    to ask for a meeting. Note that it is generally the responsibility of the
    feature owner to drive the review process to a conclusion. However, the
    security reviewer should strive for clear communication on what remains to
    be addressed at any point in the process. As you have probably realized by
    now, the *Launch-Security* flag gets flipped between *NeedInfo* and
    *ReviewRequested* as the review progresses, always keeping track of who's
    responsible for the next action.
8.  We're aiming to acknowledge a *ReviewRequested* flag for the upcoming
    milestone within seven days. This doesn't mean the review needs to be
    completed in seven days. *ReviewRequested* means "the feature team has done
    everything they thought they had to, and now they need input from the
    security team". It doesn't mean "we solemnly swear the feature is 100%
    complete". Therefore, what we're aiming to do within seven days is to
    unblock the feature team by letting them know what the next step is. As
    explained in this list, the next step could be more documentation, or an
    updated or more robust implementation. In any of those cases, or if the
    feature is not really ready for review, flip the review flag to *NeedInfo*
    and explain what's missing.
9.  Once everything looks good, the security reviewer should flip the review
    flag to *Approved* and document conclusions and aspects that were
    specifically evaluated in the security review in a bug comment. The [review
    framework] section is useful to structure this. The information in the
    comment is intended for future reference when consulting previous security
    review decisions for guidance. Also, in case aspects of a feature are later
    found to cause security issues, it's useful to understand whether these
    aspects surfaced in the security review and the reasoning behind review
    conclusions. Note that the purpose is *not* to blame security reviewers in
    case they have missed problems, but to help our future selves understand how
    we can improve the process as needed (for example by adding specific items
    to watch out for to the [review framework]).
10. In case the review reaches an impasse, the security reviewer shouldn't just
    flip the review flag to *NotApproved* as we're committed to engaging
    productively as much as we can. Instead, the current state of things should
    be surfaced and relevant leads be consulted to figure out a way forward.

## Review framework - things to look at

### Security boundaries

Does the feature poke holes in existing security boundaries? This is not a good
idea. Existing boundaries include:

*   Chrome renderer process to Chrome browser process: this boundary is
    traversed only with Chrome IPC or Mojo. It should not be trivial for a
    Chrome renderer process to directly manipulate or control the Chrome
    browser process. For browser-specific guidelines, check out
    [How To Do Chrome Security Reviews].
*   Chrome browser process (running as user `chronos`) to system services: this
    boundary is traversed with D-Bus. It should not be possible for the Chrome
    browser process to directly manipulate a system service or directly access
    system resources (e.g. firewall rules or USB devices).
*   ARC++ container to Chrome browser or ChromeOS system: this boundary is
    traversed with Mojo IPC. It should not be possible for the container to
    directly manipulate resources outside of the container. **Trust decisions
    should always be made outside of the container.**
*   Userspace processes to kernel: this boundary is traversed by system calls.
    It should not be possible for userspace to gain untrusted kernel code
    execution (this includes loading untrusted kernel modules). Seccomp (see
    the [sandboxing guide] for details) should be used to secure this boundary.
*   Kernel to firmware: it should not be possible for a kernel compromise to
    result in persistent, undetectable firmware manipulation. This is enforced
    by Verified boot.

Are security boundaries still robust after the feature has been implemented?
For example, a feature might be in theory respecting a boundary by implementing
IPC, but if the IPC interface is designed so that the higher-privilege side
implicitly trusts and blindly carries out what the lower-privileged side
requests, then the security boundary has been weakened.

Does the feature require adding new boundaries? For example, ARC++ was a
feature that required adding a new security boundary: the ARC++ container. A
new security boundary, while sometimes necessary to restrict what untrusted
code can do, also adds a layer that will need to be enforced and maintained
going forward.

### Privileges

*   Implement the principle of least privilege: give code only the permissions
    it needs to do its job, and nothing else. E.g. if code doesn't need to be
    root, it shouldn't run as root. If it doesn't need to access the network, it
    shouldn't have access to the network.
*   See the [sandboxing guide] for more details.

### Untrusted input

*   Prefer robust parsers, e.g. protobuf. Avoid implementing your own
    serialization and deserialization routines: buggy (de)serialization code is
    routinely the source of security bugs. These bugs are dangerous because they
    can occur on the trusted side of an IPC boundary, allowing a less privileged
    process to possibly get code execution in a more privileged process. If you
    absolutely must write custom code, the requirement for new serialization
    code is for the code to be thoroughly fuzzed (see the
    [fuzzing documentation]) or written in a memory-safe language like Rust.
*   Don't reimplement IPC using pipes or sockets, use Mojo or D-Bus. The concern
    is the same as in the previous case: hand-written IPC is brittle, and bugs
    in IPC mechanisms could allow a less privileged process to subvert the
    execution of a more privileged process, which is the definition of a
    privilege escalation bug. Refer to the [Mojo IPC security guidelines] for
    more details.
*   Any code handling non-trivial untrusted data must be fuzzed to ensure its
    robustness. See our [fuzzing documentation] for more information.

### Sensitive data

*   Does the feature handle sensitive user data? Maintain ChromeOS's
    guarantee: user data is never exposed at rest (i.e. when the user is not
    logged in or when the device is off), and different users on the same device
    cannot access each other's data.
*   User data stored by the Chrome browser in the user's home directory is
    protected by ChromeOS's user data encryption. If the feature accesses this
    data from outside a Chrome session (e.g. not running as the `chronos`) user,
    how is it making sure that the data will not be saved to disk unencrypted?
*   Does the feature expose existing sensitive data to less privileged
    contexts? This is dangerous, and should be avoided. If necessary, consider
    filtering the data that is made available, allowing only reading the
    minimum amount of data required, disallowing modifications, and take care
    to not allow cross-user modification of user data.

### Attack surface

*   What can an attacker gain by triggering the code added for the feature? For
    example, new code might be calling a previously unused system call with
    parameters received directly from another process over IPC. If that other
    process is compromised, and the new code doesn't validate these parameters,
    the feature just added that system call as attack surface exposed to all
    processes that can access the IPC endpoint.
*   Are new libraries being used and what new packages are pulled into the
    image? If so, how confident are we about the quality of the code in the
    library? Do the upstream maintainers patch security bugs? Will the team
    adding the feature be responsible for updating the library when security
    bugs get discovered? Library code is subject to the same (or arguably more)
    scrutiny than code written by ChromeOS engineers: is it robust against
    malformed or malicious input? Can it be fuzzed?
*   Is the code accessible to remote attackers? Is it exposed directly over the
    network? If so, consider whether this is really necessary, and whether it
    can be mitigated with firewall rules or other restrictions.

### Implementation robustness

*   Does the code use threads? Are you confident there are no race conditions?
    Consider using the most straightforward threading or multi-process
    constructs you can (like `libbrillo`'s [BaseMessageLoop]).
*   Does the code implement a state machine or a protocol? Will an attacker
    sending packets in the wrong order break things? Write the code in a
    defensive way, to be robust in the face of malicious input and to *fail
    closed*, this is, to fail by denying access or by refusing to carry out a
    request, instead of the opposite.
*   Are there other assumptions prone to accidentally breaking in the future?
    This can include assuming an IPC is only ever called with certain
    parameters, or assuming a certain process is always present on the system,
    or that the state on disk for a file or a directory is always the same.
    If so, add a test to enforce the assumption, and revisit your code to be
    robust in the event of a broken expectation.

### Cryptography

*   If the feature is using cryptography for integrity, confidentiality, or
    authentication, reach out to the security team. You can always find us at
    [chromeos-security@google.com]. We can put you in contact with crypto
    experts to make sure that your use of cryptography is correct.
*   There should be no need for features to roll their own crypto. Use
    well-established cryptographic libraries like BoringSSL or OpenSSL.
*   Please check any dependencies you add for implementations of cryptographic
    algorithms or protocols and flag these to the security team before bringing
    in the dependency. This includes e.g. TLS client implementations, which are
    highly non-trivial to get right, and incur significant maintenance cost.
*   Prefer to use high-level primitives. E.g. don't take an AES-CBC
    implementation and add your own padding, use a high-level primitive that
    simply encrypts and decrypts data provided a key.
*   If you're using keys, what is the key management story? Where are keys
    kept? Do they need to be hardware-protected?
*   Should key management tie in with our signing infrastructure? This is
    important if the keys are being used to sign code or other artifacts.
*   Do we know how to rotate keys in case there's a compromise? If the feature
    is using cryptographic keys extensively, you must write a key rotation
    doc as part of the feature.
*   For a quick primer on cryptography best practices, check out this
    [newer list of cryptographic right answers], as well as this
    [older reference of cryptographic right answers]; but remember to reach out
    to the security team to validate your design.

### Metrics

For features that are security-sensitive, strive to add metrics that allow
tracking whether the feature is functioning as expected. You should be able to
answer whether the expectations you had regarding the state or behavior of
devices in the field were correct or not. For example:

*   If you're adding a feature that verifies signatures or hashes, either from
    the network or on disk, report the result of the verification.
*   More generally, if you're adding a feature that needs to check the validity
    of data, consider reporting when you encounter malformed data.
*   If you're adding a feature that depends on device state, like converting
    a file on disk to an updated format, consider reporting both the state
    before the feature, as well as the result of the operation.

The objective of this reporting is to identify blind spots in our security
posture in the field. If a security-sensitive feature is failing, we should know
about it. It's possible that we could learn about individual instances of the
failure, maybe via bug reports, but without metrics we cannot find out about
the *extent* of the problem.

[UMA] is the metrics infrastructure used in Chrome and ChromeOS. You can report
metrics both from the Chrome browser and from system services.

### Philosophy

Overall ChromeOS security architecture is inspired by a set of principles that
help us make consistent decisions. They guide us towards solutions that achieve
security trade-offs which make sense in the world our users live in. As part of
the security review process, we evaluate whether new features strike the right
balance with respect to our principles. This is not exact science - we often
need to balance legitimate interest to add a new feature with slight
deterioration of ChromeOS's overall adherence to the principles. In case of
obvious conflicts, it is important though to explore whether the feature in
question can be implemented in an alternative way that is more in line with the
principles. If a better design is identified, please do make the effort to
seriously consider it. Design improvements are a triple win: The feature team
produces a shinier feature, users get something that works better, and the
security team has fewer things to worry about.

These are our security principles:

*   *Secure by default*: All ChromeOS features should offer adequate security
    without requiring the user to take further action. We don't ever want to end
    up in a situation where people seriously follow "10 absolutely essential
    ChromeOS security tweaks" guide documents on the internet.
*   *Don't scapegoat the user*: Pushing security decisions on the user is
    sometimes unavoidable, in particular where there is no viable secure default
    that works for everyone. We want to avoid asking the user security-relevant
    questions as much as possible though and just figure out based on the
    situation what is the correct security choice for them. Are there other
    settings or decisions that have already been taken that can guide the
    decision? Can we identify relevant user segments automatically?
*   *Defense in depth*: Layered security defenses are industry standard now. All
    features must be evaluated under the assumption that one or more security
    boundaries are broken by attackers. What would be the correct system
    behavior under that scenario?
*   *The perfect is the enemy of the good*: There's always room for improvement
    when it comes to security features. Striving for ideal security is not only
    impossible in general, but often prevents or delays security improvements
    that are meaningful but do leave gaps in practice, so we need to strike a
    balance. When in doubt, evaluate choices against attack scenarios to
    determine how worthwhile a given security defense is.
*   *Protect user data, be transparent*: Our ultimate goal is to protect our
    users' data. Security features help with that, but we must acknowledge that
    user behavior will always be part of the equation. In the light of that, we
    aim to be transparent so users are aware when they share their data or
    otherwise open it up to additional exposure. There is tension here to
    "secure by default" and *don't scapegoat the user* - this is intentional :-D

[sandboxing guide]: sandboxing.md
[crbug.com]: https://crbug.com
[chromeos-security@google.com]: mailto:chromeos-security@google.com
[office hours]: https://goto.google.com/cros-security-office-hours
[chromiumdash]: https://chromiumdash.appspot.com/schedule
[fuzzing documentation]: testing/fuzzing.md
[BaseMessageLoop]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/libbrillo/brillo/message_loops/base_message_loop.h
[newer list of cryptographic right answers]: http://latacora.singles/2018/04/03/cryptographic-right-answers.html
[older reference of cryptographic right answers]: https://www.daemonology.net/blog/2009-06-11-cryptographic-right-answers.html
[review framework]: #review-framework-things-to-look-at
[How To Do Chrome Security Reviews]: https://docs.google.com/document/d/1JDC411NquvDGTQjQbtQSzHtBaQyCFMDTAAQZGifdmuE/edit
[Mojo IPC security guidelines]: https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/mojo.md
[UMA]: https://g3doc.corp.google.com/analysis/uma/g3doc/home/index.md
