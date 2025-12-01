---
breadcrumbs:
- - /Home
  - Chromium
- - /Home/chromium-security
  - Chromium Security
- - /Home/chromium-security/root-ca-policy
  - Root Program Policy
page_name: apply-for-inclusion
title: Apply for Inclusion
---

## Last updated: 2025-02-15

The Chrome Root Program Policy defines the [minimum requirements](/Home/chromium-security/root-ca-policy/) that must be met by Certification Authority (CA) Owners for both initial and continued inclusion in the Chrome Root Store.

Google includes or removes self-signed root CA certificates in the Chrome Root Store as it deems appropriate at its sole discretion. The selection and ongoing inclusion of CA certificates is done to enhance the security of Chrome and promote interoperability. CA certificates that do not provide a broad service to all browser users will not be added to, or may be removed from the Chrome Root Store. CA certificates included in the Chrome Root Store must provide value to Chrome end users that exceeds the risk of their continued inclusion.

### Inclusion Processing

The Chrome Root Program and corresponding Root Store processes inclusion requests and requests for changes through the Common CA Database (CCADB). CA Owners who satisfy all of the requirements in the Chrome Root Program [Policy](/Home/chromium-security/root-ca-policy/) may apply.

The application process includes:

1. A CA Owner [requests](https://www.ccadb.org/cas/request-access) and gains access to CCADB (if not already granted access).
2. A CA Owner adds a root CA certificate to CCADB and completes one or more “[Add/Update Root Request](https://www.ccadb.org/cas/updates)” cases in the CCADB to populate all tabs (i.e., CA Owner, Audits, Non-Audit Documents, Root Information, and Test Websites) with information.
3. A CA Owner submits a “[Root Inclusion Request](https://www.ccadb.org/cas/inclusion)” in CCADB.
4. The Chrome Root Program performs an initial review of the information included in CCADB to ensure completeness and compliance with the minimum requirements.
5. A [CCADB public discussion](https://www.ccadb.org/cas/public-group) period ensues.
6. The Chrome Root Program performs a detailed review of all information provided in CCADB and publicly available (to include output from the CCADB public discussion).
7. The Chrome Root Program makes a final determination and communicates it to the CA Owner.

Typically, applications are processed on a first-in, first-out basis, with priority given to those:

*   replacing an existing root CA certificate which is already included in the Chrome Root Store and in good standing, and
*   whose disclosed and observed operational practices yield a perceived transformative benefit to the security and stability of the Internet ecosystem, significantly benefiting Chrome users.

The Chrome Root Program takes as much time to process applications as needed to ensure user security, and makes no guarantees on application processing time. The Chrome Root Program may apply additional application review weighting criteria as it sees necessary or valuable to Chrome user security. At any point, the Chrome Root Program may contact the Applicant during its review seeking additional or clarifying information. Applicants are expected to provide the requested information in a timely manner.

### Inclusion Acceptance

Ultimately, in order for a CA Owner’s inclusion request to be accepted, it must clearly demonstrate the value proposition for the security and privacy of Chrome’s end users exceeds the corresponding risk of inclusion.

Illustrative behaviors demonstrating value include:

*   supporting customers in multiple geographic markets and in multiple native languages.
*   freely-available guidance, help articles, or FAQ to support the user community in requesting/renewing certificates or configuring TLS.
*   not relying on “cached" domain validation information during certificate issuance.
*   leveraging operational practices consistent with those described in [Moving Forward, Together](/Home/chromium-security/root-ca-policy/moving-forward-together/) at the time of application submission. For example, reliably issuing TLS server authentication certificates that are valid for a much shorter period of time than the maximum validity allowed by the Baseline Requirements for the Issuance and Management of Publicly-Trusted TLS Server Certificates.
*   supporting the Automatic Certificate Management Environment (ACME) protocol and the ACME Renewal Information (ARI) extension, complemented by technical controls that encourage cryptographic agility.
*   responsibly operating [Certificate Transparency](https://googlechrome.github.io/CertificateTransparency/) log(s) qualified in Chrome.

Actions in this list are only illustrative and do not guarantee inclusion application acceptance.

Root CA certificates approved for distribution will be added to the Chrome Root Store on approximately, but not limited to, a quarterly basis. However, the Chrome Root Program offers no guarantees related to the timeliness of CA certificate distribution.

CA Owners should not anticipate receiving application coaching beyond what is specified on this page and the Chrome Root Program Policy. CA Owners may seek clarification on Chrome Root Program policies or processes, and members of the Chrome Root Program will respond in a timely manner.

### Inclusion Rejection

The Chrome Root Program will reject inclusion requests where an applicant does not meet the minimum requirements defined by the Chrome Root Program [Policy](/Home/chromium-security/root-ca-policy/) or the application is deemed incomplete or inaccurate.

The Chrome Root Program may reject requests for inclusion into the Chrome Root Store as deemed appropriate, and is not obligated to justify any inclusion decision.

Illustrative factors for application rejection may include:

*   a failure to demonstrate broad value for Chrome users and why the benefits of inclusion outweigh the risks to user safety and privacy.
*   a corresponding Public Key Infrastructure (PKI) certificate hierarchy where leaf certificates are not primarily intended to be used for server authentication facilitating a secure connection between a web browser and a corresponding website (e.g., client authentication certificates, Internet of Things (IoT) device certificates, smart cities, transportation, medical devices, etc.).
*   a corresponding PKI hierarchy that currently or previously allowed, facilitated, or enabled “Monster in the Middle” (MITM) attacks (either successful or attempted) where a certificate was issued for the purposes of impersonation, interception, or to alter communications.
*   where the corresponding CA Owner has ever been:
    *   determined to have acted in an untrustworthy manner or created unnecessary ecosystem risk, or
    *   associated with a certificate that was previously distrusted by Chrome or any other public root program.
*   has an incident history that does not convey the [factors](/Home/chromium-security/root-ca-policy/#51-incident-reports) significant to Chrome.
*   completion of a CCADB root inclusion public discussion that casts doubt over the CA Owners security, honesty or reliability.
*   discovery of false or misleading information provided by the CA Owner.
*   significant delays in response from the CA Owner when seeking additional or clarifying information.

Actions in this list are only illustrative and considerations for rejection are not limited to this list.

Depending on the reason for application rejection, the Chrome Root Program, at its sole discretion, may:

*   require a period of time to elapse before the CA Owner may re-apply, or
*   reject all future applications from the CA Owner.