---
breadcrumbs:
- - /Home
  - Chromium
- - /Home/chromium-security
  - Chromium Security
- - /Home/chromium-security/post-quantum-pki-design
  - Building a Deployable Post-quantum Web PKI
page_name: post-quantum-pki-design
title: Building a Deployable Post-quantum Web PKI
---

_Last Updated 2024-08-27_

While no one knows exactly when cryptographically-relevant quantum computers (CRQCs) will arrive, uses of classical cryptography are slowly being migrated to post-quantum cryptography (PQC) to prepare for their arrival. In TLS, this preparation consists of three major efforts:



1. Establishing TLS session keys in a quantum-resistant manner
2. Migrating the Web PKI, as the primary TLS authentication mechanism, to rely on PQC
3. Determining how server key material is used (e.g. signing, vs. [KEMs](https://blog.cloudflare.com/post-quantum-key-encapsulation))

In line with our [strategy for implementing post-quantum cryptography](https://blog.chromium.org/2024/05/advancing-our-amazing-bet-on-asymmetric.html), Chrome has already rolled out [hybrid PQC key agreement](https://blog.chromium.org/2023/08/protecting-chrome-traffic-with-hybrid.html) in TLS to protect network traffic against harvest now, decrypt later ([HNDL](https://en.wikipedia.org/wiki/Harvest_now,_decrypt_later)) attacks. When connecting to servers that support this mechanism, TLS connections are protected by session keys that are believed to be resistant to future quantum cryptanalysis.

This protects current traffic from a future CRQC, but post-quantum authentication will be needed when a CRQC arrives. Here, the core challenge is that a simple algorithm swap would produce X.509 certificate chains that are simply [too](https://dadrian.io/blog/posts/pqc-signatures-2024/) [big](https://blog.cloudflare.com/pq-2024#do-we-really-care-about-the-extra-bytes) to avoid significant performance and bandwidth regressions. The following table demonstrates the significant differences between several classical and post-quantum signature sizes:

| Signature Scheme     | Public Key Bytes | Signature Bytes | Post-Quantum? |
| ---------- | ---------------- | --------------- | ------------- |
| P-256      | 64               | 64              | N             |
| RSA-2048   | 272              | 258             | N             |
| ML-DSA-44  | 1,312            | 2,420           | Y             |
| ML-DSA-65  | 1,952            | 3,309           | Y             |
| ML-DSA-87  | 2,592            | 4,627           | Y             |
| UOV-ls-pkc | 66,576           | 96              | Y             |

After exploring various PQ protocol and PKI designs, we realized that the web needs more drastic changes than a simple replacement of signature algorithms in X.509 to avoid untenable performance regressions. We also realized that this transition presents the TLS ecosystem with a unique opportunity to address some of the longstanding challenges and deficiencies with the use of PKI authentication in TLS. Since code changes will be needed on every client, server, and certification authority to support PQC, this opens up the possibility of including additional improvements that were infeasible to deploy in isolation.

In order to guide our efforts in this design space, we have compiled a list of design principles, constraints, and requirements. By sharing these principles, we hope to help the broader community understand our approaches and open up additional avenues for collaboration, experimentation, and standardization.

## Design Principles

**1. CAs (or CA-analogues) should be relied upon to provide cryptographic assertions that bind public key(s) to domain name(s)**



*   We’re interested in ways of strengthening domain validation and preventing misissuance, but these are mostly orthogonal to the post-quantum transition, except that in some cases, a transition to a new system for post-quantum authentication could be a good opportunity to introduce stronger controls on certificate issuance (and/or changes to certificate formats or other generic improvements).

**2. The size of transmitted public keys and signatures should be kept as small as possible**



*   It’s not possible to define exact size thresholds, but we have some rough thresholds that can be used to gauge deployment feasibility.
*   Adding ~2KB to the TLS handshake is _very_ painful, but plausible.
*   Adding ~7KB is implausible unless a cryptographically relevant quantum computer (CRQC) is tangibly imminent.
*   These thresholds may change over time as networks improve.

**3. Fast certificate issuance must be supported, but designs may also offer slower issuance paths that provide different tradeoffs**



*   Fast issuance is important for cases when a new site is going online, or needs to rekey or renew in an emergency.
*   A rough upper bound on what constitutes “fast” is issuance in under 60 seconds, and we expect there to be significant pressure on any design to reduce this even further.
*   We may introduce slower issuance paths where doing so provides significant benefits (e.g. smaller certificates), but there must be some supported fast issuance path available at all times.

**4. Certificate issuance should be publicly transparent**



*   What TLS certificates are issued by a trusted CA is of significant security concern to site operators, browsers, and the broader web ecosystem.
*   Designing a PQC TLS/PKI authentication mechanism should integrate issuance transparency directly and explore possible improvements to transparency guarantees, issuance latency, and ecosystem agility.
*   The trust and decentralization assumptions of a PQC PKI transparency system can, and likely will, be revised as compared to [Certificate Transparency](https://certificate.transparency.dev/) (CT) today. This opens up a broader design space than was possible in the past with a number of new possible trade-offs.

**5. Old clients must be supported, but designs may favor modern/updated clients**



*   Here, old clients are defined as those that have been updated to accept certs from a PQC PKI, but that are more than one version behind the latest available version. Despite auto-update mechanisms, this is often a significant portion of active clients.
*   It’s important that older clients are able to support PQC authentication in TLS, but we are open to designs that optimize performance for modern or frequently-updated clients.
*   As the time since a client's last update increases, it may fall into a slower or unoptimized path, but they should still have a viable path for connecting to servers.
*   An example of this principle is demonstrated by [Merkle Tree Certificates](https://datatracker.ietf.org/doc/draft-davidben-tls-merkle-tree-certs/), which present a bandwidth-optimized authentication mechanism with stronger transparency properties than CT currently provides, but slows certificate issuance considerably and requires up-to-date clients to work. A separate fallback solution is required when these requirements are not met.

**6. PKIs should be able to change and evolve without conflicting with server availability**



*   Client PKI policies must be able to change over time to improve user security, performance, etc. This may include CA additions, removals, and key rotations, or other policy changes.
*   As clients evaluate the tradeoffs for PQC PKI support, real-world experimentation is necessary, and clients with different needs may ultimately make different decisions.
*   These transitions naturally result in some divergence in client behavior, such as between older and newer clients.
*   Designs that incentivize client-specific requirements for certificate validation should support client-specific negotiation to limit the negative impacts of this property.

**7. Servers must have some mechanism to interoperate with all supported clients.**



*   For example, servers might provision multiple certificates, with a negotiation mechanism to select the correct one. Without some mechanism, PKI security improvements will conflict with server availability.

**8. Online trusted key material should have a limited lifetime**



*   In order to support ecosystem agility and the security of trusted key material, keys that are kept online (i.e. not in an air-gapped HSM) should not be in use for more than a small, single-digit number of years.
*   Designs that require trusted key material to be online should build in agility mechanisms to allow these keys to be changed routinely.
*   This principle is in line with modern PKI practices where intermediate CA keys may be kept online but often have significantly shorter lifetimes than offline root CAs.

**9. Adding new services in the critical path for certificate issuance should be avoided or mitigated**



*   Adding single points of failure to an already brittle system is an antipattern that should be avoided.
*   Possible mitigations include redundancy (e.g. multiple Certificate Transparency logs), retaining a fallback path (if transparency services in a Merkle Tree Certificates-type solution are offline, the fallback mechanism is available), and negotiation to limit the impact of an offline service to only its associated clients.

**10. Clients must be able to connect to servers without connectivity to additional services**



*   This avoids regressing, e.g. captive portals. We're open to (but don't anticipate) designs that place additional services on this critical path, but there must be a fallback that works without them, and the performance implications must be carefully considered.

**11. The set of domain names visited by a user must be considered privacy-sensitive**



*   Novel designs that would otherwise reveal this information to some new party must appropriately mitigate this.


## Our Projects


#### Trust Anchor Negotiation

[Trust Anchor Identifiers (TAI)](https://datatracker.ietf.org/doc/draft-beck-tls-trust-anchor-ids/): A TLS extension that allows TLS relying parties to communicate information about what trust anchors (i.e. root certification authorities) it trusts to facilitate certificate selection. TAI expresses these trust anchors in a TLS extension that is similar to the existing certificate_authorities TLS extension, but uses significantly fewer bytes to express this information. Additionally, this specifies a way for TLS servers to advertise their supported certification paths in DNS or in connection set up, so that TLS clients may choose a supported option, if available.

[Trust Expressions (TE)](https://datatracker.ietf.org/doc/draft-davidben-tls-trust-expr/): A TLS extension and supporting mechanisms to allow TLS relying parties to communicate an entire set of trust anchors using named, versioned aliases that refer to widely-deployed trust stores. In order for TLS subscribers to interpret these aliases, Trust Expressions describes a new document called Trust Store Manifests, which describe the contents of a trust store over time. This information gets consumed by certification authorities and appended to certificates at issuance time, so that these aliases can be interpreted correctly.


#### Non-X.509 PKIs

[Merkle Tree Certificates (MTC)](https://datatracker.ietf.org/doc/draft-davidben-tls-merkle-tree-certs/): A new type of certificate that integrates issuance transparency directly into the certificate issuance process at the cost of increased issuance time, additional complexity at the CA, and the introduction of a new type of transparency service that verifies issuance behavior and communicates fresh, short-lived trust anchor information to relying parties.

