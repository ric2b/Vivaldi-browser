---
breadcrumbs:
- - /Home
  - Chromium
- - /Home/chromium-security
  - Chromium Security
- - /Home/chromium-security/root-ca-policy
  - Root Program Policy
page_name: moving-forward-together
title: Moving Forward, Together
---

## Last updated: 2024-10-09

For more than the last decade, Web PKI community members have tirelessly worked together to make the Internet a safer place. However, there’s still more work to be done. While we don’t know exactly what the future looks like, we remain focused on promoting changes that increase speed, security, stability, and simplicity throughout the Web PKI ecosystem.

With those goals in mind, the Chrome Root Program continues to explore introducing future requirements related to the following themes:
*   Encouraging modern infrastructures and agility
*   Focusing on simplicity
*   Promoting automation
*   Reducing mis-issuance
*   Increasing accountability and ecosystem integrity
*   Streamlining and improving domain validation practices
*   Preparing for a "post-quantum" world

## Understanding "Moving Forward, Together" initiatives

The initiatives described on this page are distinct from the requirements detailed in the [Chrome Root Program Policy](https://g.co/chrome/root-policy). These initiatives are proposals for exploration. They are not requirements.

Some proposals may change during our review process, after considering community feedback, or studying the ecosystem impacts and tradeoffs of adoption. Others may not be adopted at all.

As an example, community feedback shaped the adoption of a previous "Moving Forward, Together" initiative promoting CA agility. This proposal  placed a limit on the duration of a root CA certificate’s inclusion in the Chrome Root Store (i.e., root "term-limit"). Based on feedback from the CA Owners included in the Chrome Root Store and as a result of studying data from publicly-available sources related to certificate ubiquity, we extended the duration of the initially proposed term-limit from 7 to 15 years and landed these changes in Version 1.5 of the Chrome Root Program Policy.

### Why communicate these initiatives?

Our program prioritizes transparency and recognizes that significant change takes time, careful planning, and collaboration. Many initiatives in "Moving Forward, Together" represent significant shifts within the ecosystem, and we want to minimize adverse ecosystem impacts when appropriate and possible. By sharing our initiatives early, we aim to understand the challenges stakeholders may face, collaboratively develop solutions to meet stated goals, and ultimately amplify the positive impact of these changes while reducing the negative.

### When might these initiatives take effect?

We understand the enthusiasm within the Web PKI community regarding some of the proposals described on this page. However, we want to emphasize that there are currently **no planned implementation timelines** for these initiatives, unless explicitly stated below, in the Chrome Root Program Policy, or in [CA/Browser Forum Server Certificate Working Group Ballots.](https://cabforum.org/working-groups/server/ballots/) Again, not all proposals and initiatives may be ultimately adopted by the Chrome Root Program Policy.

### How might these initiatives take effect?

Some of the proposals described on this page might be achieved through collaborations within the CA/Browser Forum. In other cases, it might be most appropriate for corresponding changes to land only in the Chrome Root Program Policy, as not all CA Owners who adhere to the CA/Browser Forum TLS Baseline Requirements intend to serve Chrome’s focused PKI use case of server (i.e., website) authentication, or wish to be trusted by default in Chrome. Regardless of how these proposals might eventually be implemented, we are committed to collaborating with community members and deeply value their feedback.

### What informs our approach?

To explore and understand the broader ecosystem impacts of these proposals, we:
*   study ecosystem data from publicly available tools like [crt.sh](http://crt.sh) and [Censys](https://censys.com/),
*   interpret data resulting from Chrome tools, experiments, and usage data,
*   evaluate peer-reviewed research,
*   collect direct feedback through surveys shared with the CA Owners included in the Chrome Root Store,
*   communicate with members of the Web PKI community using the Common CA Database [discussion forums](https://groups.google.com/a/ccadb.org/g/public), and
*   monitor indirect feedback from ecosystem stakeholders across a number of communications channels.

This helps ensure our decisions are informed by real-world data, observed Chrome user behavior, expert research, and the many voices of the community.

## Areas under exploration:

### Phase out "multi-purpose roots" from the Chrome Root Store

**Themes:** (1) "Encouraging modern infrastructures and agility" and (2) "Focusing on simplicity"

**Understanding dedicated hierarchies:**

Certificates issued by publicly-trusted CA Owners (i.e., those included in various product and operating system trust stores) serve a variety of use cases including TLS server authentication, TLS client authentication, secure email (e.g., signed and encrypted email), document signing, code signing, and others. Up until about five years ago, it was common to see some or all of these use cases served from a single PKI hierarchy. While this approach offered flexibility to some stakeholders, there is inherent complexity of balancing multiple, sometimes competing use cases and requirements, especially as the CA/Browser Forum created additional sets of standards focused on use cases beyond TLS.

Beginning in September 2022, the Chrome Root Program [codified](https://www.chromium.org/Home/chromium-security/root-ca-policy/policy-archive/version-1-1/#4-dedicated-tls-pki-hierarchies) its commitment to simplicity by requiring applicant PKI hierarchies to the Chrome Root Store focus only on serving TLS use cases. However, while this approach promotes future simplicity, not all CA certificates included in the Chrome Root Store are aligned on this principle. To do so, and to completely realize the benefits of the transition to TLS-dedicated hierarchies, we intend to remove "multi-purpose" root CA certificates, or those CA certificates not dedicated to TLS server authentication use cases, from the Chrome Root Store.

**Why it matters:**

*   **Improves security by reducing attack surface:** Today, Chrome transitively trusts over 2,300 CA certificates, however, only about half of these CAs issue TLS server authentication certificates, the only PKI use case applicable for Chrome while authenticating websites. Removing out-of-scope CAs from Chrome’s security boundary results in fewer points of potential vulnerability, reducing risk for Chrome users.
*   **Promote simplicity:** Dedicated-use hierarchies reduce complexity, clarify priorities, and improve maintainability for the use cases they serve. This simplicity can lead to more effective policies, practices, and operations compared to multi-use hierarchies that try to address multiple disparate requirements and standards at once.
*   **Focuses innovation:** By focusing on purpose-built hierarchies the Web PKI community can work together to more directly satisfy the specific needs of TLS certificate subscriber use cases (e.g., improve support for automation) and allow unencumbered innovation and iteration, rather than being obligated to satisfy the lowest common denominator of many different subscriber-type use cases or corresponding certificate issuance and management requirements.
*   **Decouple public and private PKI use cases:** Client authentication represents a private PKI use case that is not relied upon by web browsers for website authentication. Issuing certificates intended for client authentication by CAs that validate to certificates included in public root stores, like the Chrome Root Store, mean CAs and sometimes by extension, subscribers, are obligated to adhere to the CA/Browser Forum [TLS Baseline Requirements](https://cabforum.org/working-groups/server/baseline-requirements/documents/) and root program policies. In some cases, this approach is actively harmful to both subscribers and CA Owners. Serving client authentication use cases from private PKI hierarchies (i.e., those not trusted by public root stores) may promote enhanced security and control, offer improved scalability and flexibility, and advance the CA Owner’s ability to satisfy the unique use cases of their customers while promoting meaningful security improvements to TLS server authentication.

**What’s next?**

*   **Define and communicate expectations:** Using previously collected CA Owner feedback in response to a Chrome Root Program survey, and that collected through independent analysis, we will establish and communicate a phase-out plan that removes multi-purpose CA certificates from the Chrome Root Store.

## Areas of future exploration:

The following initiatives are planned for future exploration by the Chrome Root Program Team. As observed with several areas of past exploration, for example "[Multi-Perspective Issuance Corroboration](#require-multi-perspective-issuance-corroboration)," the Chrome Root Program Team looks forward to pursuing these security-enhancing initiatives within the CA/Browser Forum Server Certificate Working Group, when appropriate.

### Prune domain validation methods and domain validation reuse periods

**Themes:** (1) "Encouraging modern infrastructures and agility", (2) "Streamlining and improving domain validation practices", and (3) "Focusing on simplicity"

We intend to better understand the use of existing domain validation practices permitted by the CA/Browser Forum TLS Baseline Requirements to help focus future opportunities for improving agility and increasing security.

The Baseline Requirements describe domain validation and authorization data may be reused so long as it was verified within the last 398 days. In reality, this represents a 795-day reuse period (i.e., assume a 398-day certificate is issued on the 397th day after domain validation was last completed). More timely domain validation will better protect domain owners while also reducing the potential for a CA to mistakenly rely on stale, outdated, or otherwise invalid information resulting in certificate mis-issuance and potential abuse. Currently, we want to study the impact of reducing domain validation reuse periods to 90 days or less.

### Promote agility through reduced certificate lifetimes

**Themes:** (1) "Encouraging modern infrastructures and agility", (2) "Promoting automation", and (3) "Streamlining and improving domain validation practices"

**TLS Server Authentication Certificates**

In April 2014, a [security vulnerability](https://heartbleed.com/) ("Heartbleed") was discovered in a popular cryptographic software library used to secure the majority of servers on the Internet that broke the security properties provided by TLS. It was estimated that over [500,000](https://www.netcraft.com/blog/heartbleed-certificate-revocation-tsunami-yet-to-arrive/) active publicly accessible server authentication certificates needed to be revoked and replaced as a result of Heartbleed.

In response to instances where a certificate’s private key is either 1) compromised or 2) demonstrated weak, CA/Browser Forum TLS Baseline Requirements dictate revocation must take place [within 24 hours](https://cabforum.org/working-groups/server/baseline-requirements/requirements/#4911-reasons-for-revoking-a-subscriber-certificate). Despite a demonstrated vulnerability, remediation efforts from website operators in response to Heartbleed were slow. Only [14%](https://www.netcraft.com/blog/keys-left-unchanged-in-many-heartbleed-replacement-certificates/#:~:text=Only%2014%25%20of,bug%20was%20disclosed.) of affected websites completed the necessary remediation steps within a month of disclosure. About[ 33%](https://www.bankinfosecurity.com/blogs/nonstop-heartbleed-nearly-200k-servers-still-vulnerable-p-2381) of affected devices remained vulnerable nearly three years after disclosure.

Despite Heartbleed taking place over 10 years ago, the reality remains that at any point in time, a TLS certificate is no more than 24-hours away from required revocation, and based on recent publicly disclosed Web PKI incident reports, it appears that the ecosystem is not yet prepared to respond as necessary.

[Peer reviewed research](https://zanema.com/papers/imc23_stale_certs.pdf) also demonstrates that reduced TLS certificate lifetime improves security. For example, in scenarios where a third party maintains control over a TLS certificate and private key, a shorter certificate lifetime implies a shorter window in which the third party can still use the key when they might no longer be authorized to possess it.

Beyond these unambiguous security needs, reducing TLS certificate lifetime encourages automation and the adoption of practices that will drive the ecosystem away from baroque, time-consuming, and error-prone issuance processes. These changes will allow for faster adoption of emerging security capabilities and best practices. Decreasing certificate lifetime will also reduce ecosystem reliance on "[broken](https://scotthelme.co.uk/revocation-is-broken/)" revocation checking solutions that [cannot fail-closed](https://www.imperialviolet.org/2014/04/29/revocationagain.html) and, in turn, offer incomplete protection. Currently, our proposed maximum TLS server authentication subscriber certificate validity is ninety (90) days.

**Subordinate CA Certificates**

Much like how introducing a [term-limit for root CAs](#root-ca-term_limit) allows the ecosystem to take advantage of continuous improvement efforts made by the Web PKI community, the same is true for subordinate CAs. Promoting agility in the ecosystem with shorter subordinate CA lifetimes will encourage more robust operational practices, more closely align relied-upon certificate profiles with modern best practices, reduce ecosystem reliance on specific subordinate CA certificates that might represent single points of failure, and discourage potentially harmful practices like key-pinning. Currently, our proposed maximum subordinate CA certificate validity is three (3) years.

### Preparing for a "post-quantum" world

**Themes:** (1) "Encouraging modern infrastructures and agility", (2) "Focusing on simplicity", and (3) "Preparing for a "post-quantum" world"

Modern networking protocols like TLS use cryptography for a variety of purposes including protecting information (confidentiality) and validating the identity of websites (authentication). The strength of this cryptography is expressed in terms of how hard it would be for an attacker to violate one or more of these properties. There’s a common mantra in cryptography that attacks only get better, not worse, which highlights the importance of moving to stronger algorithms as attacks advance and improve over time.

One such advancement is the development of quantum computers, which will be capable of efficiently performing certain computations that are out of reach of existing computing methods. Many types of asymmetric cryptography used by the Web PKI today are considered strong against attacks using existing technology but do not protect against attackers with a sufficiently-capable quantum computer.

Learn more about how we’re preparing Chrome users for a "post-quantum" world in the following Chromium Blog posts:

*   [A new path for Kyber on the web](https://security.googleblog.com/2024/09/a-new-path-for-kyber-on-web.html)
*   [Advancing Our Amazing Bet on Asymmetric Cryptography](https://blog.chromium.org/2024/05/advancing-our-amazing-bet-on-asymmetric.html)
*   [Protecting Chrome Traffic with Hybrid Kyber KEM](https://blog.chromium.org/2023/08/protecting-chrome-traffic-with-hybrid.html)

## Past accomplishments:

### Require "Multi-Perspective Issuance Corroboration"

**Themes:** (1) "Increasing accountability and ecosystem integrity" and (2) "Streamlining and improving domain validation practices"

**Understanding Multi-Perspective Issuance Corroboration:**

Before issuing a certificate to a website, a CA must verify the requestor legitimately controls the domain whose name will be represented in the certificate. This process is referred to as "domain control validation" and there are several [well-defined](https://cabforum.org/working-groups/server/baseline-requirements/requirements/#3224-validation-of-domain-authorization-or-control) methods that can be used to accomplish this goal. For example, a CA can specify a random value to be placed on a website, and then perform a check to verify the value’s presence has been published by the certificate requestor.

Despite the existing domain control validation requirements defined by the CA/Browser Forum, peer-reviewed research authored by the Center for Information Technology Policy of Princeton University ([1](https://www.usenix.org/conference/usenixsecurity18/presentation/birge-lee) and [2](https://www.usenix.org/conference/usenixsecurity21/presentation/birge-lee)) and others (e.g., [3](https://dl.acm.org/doi/10.1145/3243734.3243790)) highlights the risk of equally-specific prefix Border Gateway Protocol (BGP) attacks or hijacks resulting in fraudulently issued certificates. This risk is not merely theoretical, as it has been demonstrated that attackers have successfully exploited this ongoing vulnerability on numerous occasions, with just [one](https://freedom-to-tinker.com/2022/03/09/attackers-exploit-fundamental-flaw-in-the-webs-security-to-steal-2-million-in-cryptocurrency/) of these attacks resulting in approximately $2 million dollars of direct losses.

Multi-Perspective Issuance Corroboration (referred to as "MPIC"), sometimes referred to as "Multi-Perspective Domain Validation" ("MPDV") or "Multi-VA", enhances existing domain control validation methods by reducing the likelihood that routing attacks can result in fraudulently issued certificates. Rather than performing domain control validation and authorization from a single geographic or routing vantage point, which an adversary could influence as demonstrated by security researchers, MPIC implementations perform the same validation from multiple geographic locations and/or Internet Service Providers and have been [observed](https://drive.google.com/file/d/15e4Z9InYbThwJsDuH0oS7vfXKvdSBzi9/view) as an effective countermeasure against ethically conducted, real-world BGP hijacks ([4](https://arxiv.org/abs/2302.08000)).

The Chrome Root Program [led](https://drive.google.com/file/d/1LTwtAwHXcSaPVSsqKQztNJrV2ozHJ7ZL/view?usp=sharing) a work team of ecosystem participants which culminated in a CA/Browser Forum Ballot to require adoption of MPIC via [Ballot SC-067](https://cabforum.org/2024/08/05/ballot-sc-67-v3-require-domain-validation-and-caa-checks-to-be-performed-from-multiple-network-perspectives-corroboration/). The ballot received unanimous support from organizations who participated in voting, and the adoption process is ongoing.

**Why it matters:**

*   **Enhanced security:** MPIC improves protection against equally-specific prefix BGP attacks or hijacks. Unless all publicly-trusted CA Owners adopt MPIC, it will be possible for attackers to target those who do not to obtain fraudulently issued certificates.

### Establishing minimum expectations for linting

**Themes:** (1) "Encouraging modern infrastructures and agility" and (2) "Reducing mis-issuance"

**Understanding linting:**

Linting refers to the automated process of analyzing X.509 certificates for errors, inconsistencies, and adherence to best practices and industry standards. Linting ensures certificates are well-formatted and include the necessary data for their intended use, such as website authentication. There are numerous open-source linting projects in existence (e.g., [certlint](https://github.com/certlint/certlint), [pkilint](https://github.com/digicert/pkilint), [pkimetal](https://github.com/pkimetal/pkimetal), [x509lint](https://github.com/kroeckx/x509lint), and [zlint](https://github.com/zmap/zlint)), in addition to numerous custom linting projects maintained by members of the Web PKI ecosystem.

The Chrome Root Program participated in drafting CA/Browser Forum [Ballot SC-075](https://cabforum.org/2024/08/05/ballot-sc-75-pre-sign-linting/) to require adoption of certificate linting. The ballot received unanimous support from organizations who participated in voting, and the adoption process is ongoing.

**Why it matters:**

*   **Enhanced security:** Linting can expose the use of weak or obsolete cryptographic algorithms and other known insecure practices, improving overall security.
*   **Reducing mis-issuance:** Linting helps CA Owners reduce the risk of non-compliance with industry standards (e.g., CA/Browser Forum TLS Baseline Requirements). Non-compliance can result in certificates being "mis-issued". By detecting these issues before certificate distribution to Subscribers, the likelihood and negative impact associated with having to correct the mis-issued certificate(s) can be reduced.

### Root CA term-limit

**Theme:** "Encouraging modern infrastructures and agility"

In Chrome Root Program Policy 1.5, we [landed](https://www.chromium.org/Home/chromium-security/root-ca-policy/#root-ca-term-limit) changes that set a maximum "term-limit" (i.e., period of inclusion) for root CA certificates included in the Chrome Root Store to 15 years.

While we still prefer a more agile approach, and may again explore this in the future, we encourage CA Owners to explore how they can adopt more frequent root rotation.

**Why it matters:**

1. **Term limits will help us realize the value of continuous improvement:** The Baseline Requirements, the audit schemes permitted therein, and the ecosystem’s processes and practices have been in a state of continuous improvement since their inception. Aligning ongoing practices with modern requirements, audit frameworks, and best practices is the best way of benefiting from that improvement — and the lessons we’ve learned along the way.
2. **Term limits promote agility:** Reducing over reliance on a specific root or set of roots eliminate single points of failure, while also helping to discourage potentially dangerous practices like key pinning.
3. **Term limits can help reduce risk:** Particularly, they help re-establish a known good baseline that otherwise may have been unknowingly lost over what is now sometimes a 35 year period of time. Reducing the functional lifetime that a Root CA is relied upon reduces the maximum window of potential abuse.

### Improve automation support

**Themes:** (1) "Encouraging modern infrastructures and agility", (2) "Focusing on simplicity" and (3) "Promoting automation"

In Chrome Root Program Policy 1.5, we landed changes that require Applicant hierarchies to support automated certificate issuance and management. For more information on automation, refer to our blog post: "[Unlocking the power of automation for a safer and more reliable Internet.](https://blog.chromium.org/2023/10/unlocking-power-of-tls-certificate.html)"

**Why it matters:**

1. **Automation promotes agility:** Automation increases the speed at which the benefits of new security capabilities are realized.
2. **Automation increases resilience and reliability:** Automation eliminates human error and can help scale the certificate management process across complex environments. Innovations like ACME Renewal Information (ARI) present opportunities to seamlessly protect site-owners and organizations from outages related to unforeseen events.
3. **It increases efficiency:** Automation reduces the time and resources required to manually manage certificates. Team members are instead free to focus on more strategic, value-adding activities.

### Make OCSP optional and incentivize automation

**Themes:** (1) "Encouraging modern infrastructures and agility", (2) "Focusing on simplicity", and (3) "Promoting automation"

We proposed and collaborated with members of the CA/Browser Forum Server Certificate Working Group to pass [Ballot SC-063](https://cabforum.org/2023/07/14/ballot-sc-063-v4-make-ocsp-optional-require-crls-and-incentivize-automation/) which transitioned support for the Online Certificate Status Protocol (OCSP) from mandatory to optional within the TLS Baseline Requirements. The ballot also incentivized adoption of automation solutions by standardizing the definition of a "short-lived certificate" which is exempt from certificate revocation requirements defined in [Section 4.9.1](https://cabforum.org/working-groups/server/baseline-requirements/requirements/#4911-reasons-for-revoking-a-subscriber-certificate) of the TLS Baseline Requirements. Ballot SC-063 became effective on March 15, 2024.

**Why it matters:**

1. **Improve privacy:** OCSP requests reveal details of individuals’ browsing history to the operator of the OCSP responder. These can be exposed accidentally (e.g., via data breach of logs) or intentionally (e.g., via subpoena). Due to privacy concerns, several certificate consumer products like Chrome **do not** perform online OCSP checks by default, while others have signaled interest in transitioning to privacy-preserving methods of communicating revocation status.
2. **Enhance security:** Subscriber certificate expiration is broadly and reliably enforced across major certificate consumers, while the same is not true for certificate revocation. From a security perspective, short-lived certificates may reduce the aperture of an attack where subscriber private keys are compromised - limiting the maximum attack window to just a few days. Short-lived certificates also present an opportunity to further reduce the size of Certificate Revocation Lists, which are relied upon by many certificate consumers.


### Clarify certificate profiles

**Themes:** (1) "Focusing on simplicity" and (2) "Reducing mis-issuance"

We proposed and collaborated with members of the CA/Browser Forum Server Certificate Working Group to pass [Ballot SC-062](https://cabforum.org/2023/03/17/ballot-sc62v2-certificate-profiles-update/) which clarified and improved upon the existing certificate profiles included within Section 7 of the TLS Baseline Requirements. The ballot received unanimous support from organizations who participated in voting and became effective on September 15, 2023.

**Why it matters:**

1. **Promote simplicity:** The ballot better aligned TLS Baseline Requirements certificate content expectations across certificate issuers and consumers, reduced the opportunity for confusion resulting from the absence of a more precise certificate profile specification, and promoted more consistent and reliable implementations across the ecosystem.