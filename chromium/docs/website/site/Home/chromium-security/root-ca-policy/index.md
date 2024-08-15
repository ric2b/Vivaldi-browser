---
breadcrumbs:
- - /Home
  - Chromium
- - /Home/chromium-security
  - Chromium Security
page_name: root-ca-policy
title: Chrome Root Program Policy, Version 1.5
---

## Last updated: 2024-01-16
Bookmark this page as <https://g.co/chrome/root-policy>

## Table of Contents
- [Introduction](#introduction)
     - [Apply for Inclusion](#apply-for-inclusion)
     - [Moving Forward, Together](#moving-forward-together)
     - [Additional Information](#additional-information)
- [Change History](#change-history)
- [Minimum Requirements for CAs](#minimum-requirements-for-cas)
     - [1. Baseline Requirements](#1-baseline-requirements)
     - [2. Chrome Root Program Participant Policies](#2-chrome-root-program-participant-policies)
     - [3. Modern Infrastructures](#3-modern-infrastructures)
     - [4. Dedicated TLS Server Authentication PKI Hierarchies](#4-dedicated-tls-server-authentication-pki-hierarchies)
     - [5. Audits](#5-audits)
     - [6. Annual Self-assessments](#6-annual-self-assessments)
     - [7. Reporting and Responding to Incidents](#7-reporting-and-responding-to-incidents)
     - [8. Common CA Database](#8-common-ca-database)
     - [9. Timely and Transparent Communications](#9-timely-and-transparent-communications)

## Introduction
Google Chrome relies on Certification Authority systems (herein referred to as “CAs”) to issue certificates to websites. Chrome uses these certificates to help ensure the connections it makes on behalf of its users are properly secured. Chrome accomplishes this by verifying that a website’s certificate was issued by a recognized CA, while also performing additional evaluations of the HTTPS connection's security properties. Certificates not issued by a CA recognized by Chrome or a user’s local settings can cause users to see warnings and error pages.

When making HTTPS connections, Chrome refers to a list of root certificates from CAs that have demonstrated why continued trust in them is justified. This list is known as a “Root Store.” CA certificates included in the [Chrome Root Store](https://g.co/chrome/root-store) are selected on the basis of publicly available and verified information, such as that within the Common CA Database ([CCADB](https://ccadb.org/)), and ongoing reviews by the Chrome Root Program.

In Chrome 105, Chrome began a platform-by-platform transition from relying on the host operating system’s Root Store to its own on Windows, macOS, ChromeOS, Linux, and Android. This change makes Chrome more secure and promotes consistent user and developer experiences across platforms. Apple policies prevent the Chrome Root Store and corresponding Chrome Certificate Verifier from being used on Chrome for iOS. Learn more about the Chrome Root Store and Chrome Certificate Verifier [here](https://chromium.googlesource.com/chromium/src/+/main/net/data/ssl/chrome_root_store/faq.md).

The Chrome Root Program Policy below establishes the minimum requirements for self-signed root CA certificates to be included as trusted in a default installation of Chrome.

### Apply for Inclusion
CA Owners that satisfy the requirements defined in the policy below may apply for self-signed root CA certificate inclusion in the Chrome Root Store using [these](https://www.chromium.org/Home/chromium-security/root-ca-policy/apply-for-inclusion/) instructions.

### Moving Forward, Together
The June 2022 release (Version 1.1) of the Chrome Root Program Policy introduced the Chrome Root Program’s “Moving Forward, Together” initiative that set out to share our vision of the future that includes modern, reliable, highly agile, purpose-driven PKIs with a focus on automation, simplicity, and security.

Learn more about priorities and initiatives that may influence future versions of this policy [here](https://www.chromium.org/Home/chromium-security/root-ca-policy/moving-forward-together/).

### Additional Information
If you’re a Chrome user experiencing a certificate error and need help, please see [this support article](https://support.google.com/chrome/answer/6098869?hl=en).

If you’re a website operator, you can learn more about [why HTTPS matters](https://web.dev/why-https-matters/) and how to [secure your site with HTTPS](https://support.google.com/webmasters/answer/6073543). If you’ve got a question about a certificate you’ve been issued, please contact the CA that issued it.

If you're responsible for a CA that only issues certificates to your enterprise organization, sometimes called a "private" or "locally trusted" CA, the Chrome Root Program Policy does not apply to or impact your organization’s use cases. Enterprise CAs are intended for use cases exclusively internal to an organization (e.g., a TLS server authentication certificate issued to a corporate intranet site).

Though uncommon, websites can also use certificates to identify clients (e.g., users) connecting to them. Besides ensuring it is well-formed, Chrome passes this type of certificate to the server, which then evaluates and enforces its chosen policy. The policies on this page do not apply to client authentication certificates.

### Change History
<style type="text/css">
.tg  {border-collapse:collapse;border-spacing:0;}
.tg td{border-color:black;border-style:solid;border-width:1px;font-family:Arial, sans-serif;font-size:12px;
  overflow:hidden;padding:10px 5px;word-break:normal;}
.tg th{border-color:black;border-style:solid;border-width:1px;font-family:Arial, sans-serif;font-size:12px;
  font-weight:normal;overflow:hidden;padding:10px 5px;word-break:normal;}
.tg .tg-h1{background-color:#CCC;border-color:inherit;font-weight:bold;text-align:center;vertical-align:top}
.tg .tg-left{border-color:inherit;text-align:left;vertical-align:top}
.tg .tg-center{border-color:inherit;text-align:center;vertical-align:top}

.tg .tg-self{text-align:left;vertical-align:top}
</style>

<table class="tg">

<thead>
   <tr>
      <th class="tg-h1">Version</th>
      <th class="tg-h1">Date</th>
      <th class="tg-h1">Note</th>
   </tr>
</thead>

<tbody>

   <tr>
      <td class="tg-center"><a href=/Home/chromium-security/root-ca-policy/policy-archive/version-1-0/>1.0</a></td>
      <td class="tg-center">2020-12-20</td>
      <td class="tg-left">Initial release</td>
   </tr>

   <tr>
      <td class="tg-center"><a href=/Home/chromium-security/root-ca-policy/policy-archive/version-1-1/>1.1</a></td>
      <td class="tg-center">2022-06-01</td>
      <td class="tg-left">Updated in anticipation of the future Chrome Root Program launch.<p>Updates include, but are not limited to:
         <ul>
            <li>future-dated applicant requirements for dedicated TLS-hierarchies and key-pair freshness</li>
            <li>clarification of audit expectations</li>
            <li>requirements for cross-certificate issuance notification</li>
            <li>description of and requirements related to an annual self-assessment process</li>
            <li>an outline of priority Chrome Root Program initiatives</li>
         </ul>
      </td>
   </tr>

   <tr>
      <td class="tg-center"><a href=/Home/chromium-security/root-ca-policy/policy-archive/version-1-2/>1.2</a></td>
      <td class="tg-center">2022-09-01</td>
      <td class="tg-left">Updated to reflect the launch of the Chrome Root Program.<p>Updates include, but are not limited to:
         <ul>
            <li>removal of pre-launch discussion</li>
            <li>clarifications resulting from the June 2022 Chrome CCADB survey</li>
            <li>minor reorganization of normative and non-normative requirements</li>
         </ul>
      </td>
   </tr>

   <tr>
      <td class="tg-center"><a href=/Home/chromium-security/root-ca-policy/policy-archive/version-1-3/>1.3</a></td>
      <td class="tg-center">2023-01-06</td>
      <td class="tg-left">Updated to include the CCADB Self-Assessment</td>
   </tr>

   <tr>
      <td class="tg-center"><a href=/Home/chromium-security/root-ca-policy/policy-archive/version-1-4/>1.4</a></td>
      <td class="tg-center">2023-03-03</td>
      <td class="tg-left">Updates include, but are not limited to:
         <ul>
            <li>alignment with CCADB Policy Version 1.2 and the Baseline Requirements</li>
            <li>clarify requirements related to the submission of annual self assessments</li>
            <li>clarify requirements to better align with program intent (e.g., CA owner policy document freshness)</li>
            <li>updated audit and incident reporting requirements to promote increased transparency</li>
            <li>require subordinate CA disclosures in CCADB</li>
            <li>clarify CA certificate issuance notification requirements</li>
         </ul>
      </td>
   </tr>

   <tr>
      <td class="tg-center">1.5 (current)</td>
      <td class="tg-center">2024-01-16</td>
      <td class="tg-left">Updates include, but are not limited to:
         <ul>
            <li>incorporated CA Owner feedback in response to policy Version 1.4 (clean-ups and clarifications throughout the policy)</li>
            <li>added new subsections for Root CA Key Material Freshness, Automation Support, and the Root CA Term-Limit</li>
            <li>aligned incident reporting format and timelines with CCADB.org</li>
         </ul>
      </td>
   </tr>
  </tbody> 
</table>

## Minimum Requirements for CAs
The key words "MUST", "MUST NOT", "REQUIRED", "SHALL", "SHALL NOT", "SHOULD", "SHOULD NOT", "RECOMMENDED", "MAY", and "OPTIONAL" in this policy are to be interpreted as described in [RFC 2119](https://datatracker.ietf.org/doc/html/rfc2119).

This policy considers a “CA Owner” to be the organization or legal entity that is either:
*   represented in the subject DN of the CA certificate; or
*   in possession or control of the corresponding private key capable of issuing new certificates, if not the same organization or legal entity directly represented in the subject DN of the certificate.

This policy considers an “Applicant” to be an organization or legal entity that has an open “Root Inclusion Request” submitted to the Chrome Root Store in the [CCADB](https://ccadb.org/).

This policy uses the term “Chrome Root Program Participants” to describe Applicants and CA Owners with:
*   CA certificates included in the Chrome Root Store; or
*   CA certificates that validate to certificates included in the Chrome Root Store.

This policy uses the term “Externally-operated CA” to describe subordinate CA certificates issued where the organization or legal entity in possession or control of the corresponding private key capable of issuing new certificates is not under the sole control of the CA Owner included in the Chrome Root Store.

Chrome Root Program Participants MUST satisfy the requirements defined in this policy, including taking responsibility for ensuring the continued compliance of all corresponding subordinate CAs and delegated third parties participating in the Public Key Infrastructure (PKI).

Google includes or removes self-signed root CA certificates in the Chrome Root Store as it deems appropriate at its sole discretion. The selection and ongoing inclusion of CA certificates is done to enhance the security of Chrome and promote interoperability. CA certificates that do not provide a broad service to all browser users will not be added to, or may be removed from the Chrome Root Store. CA certificates included in the Chrome Root Store must provide value to Chrome end users that exceeds the risk of their continued inclusion.

Chrome maintains a variety of mechanisms to protect its users from certificates that put their safety and privacy at risk, and is prepared to use them as necessary. A Chrome Root Program Participant’s failure to follow the minimum requirements defined in this policy may result in the corresponding CA certificate’s removal from the Chrome Root Store, limitations on Chrome's acceptance of the certificates they issue, or other technical or policy restrictions.

The requirements included in this policy are effective immediately, unless explicitly stated as otherwise.

Any questions regarding this policy can be directed to chrome-root-program [at] google [dot] com.

### 1. Baseline Requirements
Chrome Root Program Participants that issue TLS server authentication certificates trusted by Chrome MUST adhere to the latest version of the CA/Browser Forum [“Baseline Requirements for the Issuance and Management of Publicly-Trusted TLS Server Certificates”](https://cabforum.org/baseline-requirements-documents/) (“Baseline Requirements”).

This policy describes TLS server authentication certificates in [Section 4](#4-dedicated-tls-server-authentication-pki-hierarchies) (“Dedicated TLS Server Authentication PKI Hierarchies”).

### 2. Chrome Root Program Participant Policies
Chrome Root Program Participants MUST accurately describe the policies and practices of their CA(s) within a Certificate Policy (CP) and corresponding Certification Practice Statement (CPS), or preferably, a single document combined as a CP/CPS.

These CA policy documents MUST be:
*   freely publicly available for examination.
*   available in an authoritative English language version.
*   sufficiently detailed to assess the operations of the CA(s) and the compliance with these expectations and those of the Baseline Requirements, and MUST NOT conflict with either of these requirements.

To promote simplicity and clarity, these CA policy documents SHOULD be:
*   free standing, meaning they SHOULD NOT reference requirements located in external CPs, CPSs, or combined CP/CPSs.
*   focused on the specific PKI use case of issuing TLS server authentication certificates to websites, rather than combining multiple use cases into a single document or set of documents.
*   comprehensive and consolidated, whenever possible, such that there are not multiple sets of similar yet slightly different policy and practice statements supporting the same PKI use case.
*   available in Markdown.

The Chrome Root Program considers CA policy documentation in the CCADB to be authoritative. Before corresponding policy changes are put into practice, Chrome Root Program Participants:
*   MUST minimally ensure the updated versions of a CA’s policy documents are uploaded to their own publicly accessible repository, and
*   SHOULD ensure the updated versions of a CA’s policy documents are submitted to the CCADB within 7 days of the policy documents being uploaded to their own publicly accessible repository.

### 3. Modern Infrastructures

#### Root CA Key Material Freshness
The Chrome Root Program will only accept a CCADB “Root Inclusion Request” from Applicant PKI hierarchies with corresponding root CA key material generated within 5 years of application to the Chrome Root Store.

Applicants MUST submit written evidence to the CCADB identifying the date(s) of the key generation ceremony and an attestation to the Applicant’s adherence to the requirements defined in Sections 6.1.1.1 (“CA Key Pair Generation”) and 6.2 (“Private Key Protection and Cryptographic Module Engineering Controls”) of the Baseline Requirements from a Qualified Auditor using an approved format, in accordance with the table below.

<table class="tg">

<thead>
   <tr>
      <th class="tg-h1">Audit Scheme</th>
      <th class="tg-h1">Qualified Auditor Criteria</th>
      <th class="tg-h1">Report Format Criteria</th>
   </tr>
</thead>

<tbody>

   <tr>
      <td class="tg-center">WebTrust</td>
      <td class="tg-center">an <a href=https://www.cpacanada.ca/en/business-and-accounting-resources/audit-and-assurance/overview-of-webtrust-services/licensed-webtrust-practitioners-international>enrolled</a> WebTrust practitioner</td>
      <td class="tg-left">WebTrust “Reporting on Root Key Generation” report</td>
   </tr>

   <tr>
      <td class="tg-center">ETSI</td>
      <td class="tg-center">a <a href=https://www.acab-c.com/members/>member</a> of the Accredited Conformity Assessment Bodies' Council (ACAB’c)</td>
      <td class="tg-left">ACAB’c Audit Attestation Letter template</td>
   </tr>
  </tbody>
</table>

If key material is not used to issue a self-signed root CA certificate on the same date it was generated, Applicants MUST present written evidence from a Qualified Auditor, attesting that keys were minimally protected in a manner consistent with the requirements defined in Section 6.2 (“Private Key Protection and Cryptographic Module Engineering Controls”) of the Baseline Requirements from the time of generation to the time the self-signed certificate was issued. Written evidence MUST be submitted to the CCADB no later than 457 calendar days (i.e., 1 year and 92 days), or 458 calendar days if the audit period falls during a leap year, from when the earliest self-signed certificate was issued.

#### Automation Support
Effective February 15, 2024, the Chrome Root Program will only accept CCADB “Root Inclusion Requests” from Applicant PKI hierarchies that support at least one automated solution for certificate issuance and renewal for each Baseline Requirements certificate policy OID (i.e., IV, DV, OV, EV) the corresponding hierarchy issues. This requirement does not:
*   Prohibit Applicant PKI hierarchies from supporting “non-automated” methods of certificate issuance and renewal.
*   Require website operators to rely on the automated solution(s) for certificate issuance and renewal.

The automated solution MUST minimize "hands-on" input required from humans during certificate issuance and renewal. Acceptable "hands-on" input from humans includes initial software installation and configuration, applying software updates, and updating subscriber account information as needed. Routine certificate issuance and renewal SHOULD NOT involve human input except as needed for identity or business document verification related to IV, OV, or EV certificate issuance.

For each Baseline Requirements certificate policy OID the corresponding hierarchy issues, the Applicant MUST use its automated solution to issue test TLS server authentication certificates (i.e., "automation test certificates") intended to demonstrate its automation capabilities to the Chrome Root Program. Automation test website certificates MUST be renewed at least once every 30 days, however, at any point, the Chrome Root Program may request more frequent renewal. Automation test certificates MUST be served by a publicly accessible website whose URL is disclosed to the CCADB in a Root Inclusion Request. CA Owners are encouraged to issue “Short-lived Subscriber Certificates,” as [introduced](https://cabforum.org/2023/07/14/ballot-sc-063-v4make-ocsp-optional-require-crls-and-incentivize-automation/) in Version 2.0.1 of the Baseline Requirements, for the automation test certificates.

If at any point a self-signed root CA certificate is accepted into the Chrome Root Store after these requirements take effect and the CA Owner intends to issue a Baseline Requirements certificate policy OID not previously disclosed to the Chrome Root Program, the requirements in this section MUST be satisfied before issuing certificates containing the OID to Subscribers from the corresponding hierarchy, with the exception of automation test certificates.

Applicant PKI hierarchies SHOULD support the Automatic Certificate Management Environment (ACME) protocol. If ACME is supported by the Applicant:
*   The CA Owner MUST disclose to the CCADB an ACME endpoint (i.e., directory URL) accessible to the Chrome Root Program for each Baseline Requirements certificate policy OID the corresponding hierarchy issues (i.e., IV, DV, OV, EV).
*   ACME endpoints SHOULD be publicly accessible.
*   Each endpoint MUST support the following capabilities, as specified in [RFC 8555](https://www.rfc-editor.org/rfc/rfc8555):
    *   keyChange
    *   newAccount
    *   newNonce
    *   newOrder
    *   revokeCert
*   Each endpoint’s corresponding issuing CA(s) SHOULD support Certification Authority Authorization (CAA) Record Extensions for Account URI and ACME Method Binding, as specified in [RFC 8657](https://www.rfc-editor.org/rfc/rfc8657).
*   Each endpoint SHOULD support ACME Renewal Information (ARI, RFC [TBD](https://datatracker.ietf.org/doc/draft-ietf-acme-ari/)).
*   Each endpoint SHOULD be hosted using an appropriate and readily accessible online means that is available on a 24x7 basis.

While ACME support is encouraged, Applicant PKI hierarchies MAY support other automated solutions so long as the following characteristics are verifiably demonstrated to the Chrome Root Program:
*   The automated solution MUST:
    *   generate a new key pair for each certificate request by default.
    *   support automated domain control validation (i.e., the automated solution automatically places the Request Token or Random Value in the appropriate location without “hands-on” input from humans), using at least one of the following methods from the Baseline Requirements:
        *   DNS Change (Section 3.2.2.4.7)
        *   Agreed‑Upon Change to Website v2 (Section 3.2.2.4.18)
    *   support automated retrieval of the issued certificate (i.e., the automated solution downloads a copy of the certificate to a well-known location without “hands-on” input from humans).
    *   be sufficiently detailed in a completed “Automated Solution Assessment” form by requesting a copy from chrome-root-program [at] google [dot] com.
*   The automated solution SHOULD:
    *   support automated deployment (i.e., installation and configuration) of the issued certificate without “hands-on” input from humans.
    *   support CAA Record Extensions for Account URI Binding, as specified in Section 3 of RFC 8657.

#### Root CA Term-Limit
Effective February 15, 2024, any root CA certificate with corresponding key material generated more than 15 years ago will be removed from the Chrome Root Store on an ongoing basis.

The age of the key material will be determined by the earliest of either:
*   a key generation report issued by a Qualified Auditor that distinctly represents the corresponding key; or
*   the validity date of the earliest appearing certificate that contains the corresponding public key.

To phase-in these requirements in a manner that reduces negative impact to the ecosystem, affected root CA certificates included in the Chrome Root Store will be removed according to the schedule in the table below.

<table class="tg">

<thead>
   <tr>
      <th class="tg-h1">Key Material Created</th>
      <th class="tg-h1">Approximate Removal Date</th>
   </tr>
</thead>

<tbody>

   <tr>
      <td class="tg-center">Before January 1, 2006</td>
      <td class="tg-center">April 15, 2025</td>
   </tr>

   <tr>
      <td class="tg-center">Between January 1, 2006 and December 31, 2007 (inclusive)</td>
      <td class="tg-center">April 15, 2026</td>
   </tr>

   <tr>
      <td class="tg-center">Between January 1, 2008 and December 31, 2009 (inclusive)</td>
      <td class="tg-center">April 15, 2027</td>
   </tr>

   <tr>
      <td class="tg-center">Between January 1, 2010 and December 31, 2011 (inclusive)</td>
      <td class="tg-center">April 15, 2028</td>
   </tr>

   <tr>
      <td class="tg-center">Between January 1, 2012 and April 14, 2014 (inclusive)</td>
      <td class="tg-center">April 15, 2029</td>
   </tr>

   <tr>
      <td class="tg-center">After April 15, 2014</td>
      <td class="tg-center">15 years from generation</td>
   </tr>

  </tbody>
</table>

To further reduce negative impact to the ecosystem, the Chrome Root Store may temporarily continue to include a root CA certificate past its defined term-limit on a case-by-case basis, if the corresponding CA Owner has submitted a Root Inclusion Request to the CCADB for a replacement root CA certificate at least one year in advance of the approximate removal date.

Other circumstances may lead to the removal of a root CA certificate included in the Chrome Root Store before the completion of its term-limit (e.g., the future [phase-out](https://www.chromium.org/Home/chromium-security/root-ca-policy/moving-forward-together/#focusing-on-simplicity) of root CA certificates included in the Chrome Root Store that are not dedicated to TLS server authentication use cases).

### 4. Dedicated TLS Server Authentication PKI Hierarchies
The Chrome Root Program will only accept CCADB “Root Inclusion Requests” from Applicant PKI hierarchies that are dedicated to TLS server authentication certificate issuance.

A dedicated PKI hierarchy is intended to serve one specific use case, for example, the issuance of TLS server authentication certificates.

To qualify as a dedicated TLS server authentication PKI hierarchy under this policy:

1. All corresponding subordinate CA certificates operated beneath a root CA MUST:
    *   include the extendedKeyUsage extension and only assert an extendedKeyUsage purpose of either:
        1. id-kp-serverAuth, or
        2. id-kp-serverAuth and id-kp-clientAuth
    *   NOT contain a public key corresponding to any other unexpired or non-revoked certificate that asserts different extendedKeyUsage values.

2. All corresponding subscriber (i.e., server) certificates MUST:
    *   include the extendedKeyUsage extension and only assert an extendedKeyUsage purpose of either:
        1. id-kp-serverAuth, or
        2. id-kp-serverAuth and id-kp-clientAuth

### 5. Audits

Chrome Root Program Participant CAs MUST be audited in accordance with the table below.

<table class="tg">
<thead>
  <tr>
    <th class="tg-h1">CA Type</th>
    <th class="tg-h1">EKU Characteristics*</th>
    <th class="tg-h1">Audit Criteria</th>
  </tr>
</thead>
<tbody>
  <tr>
    <td class="tg-3paudit">Root CA</td>
    <td class="tg-3paudit">N/A</td>
    <td class="tg-3paudit" rowspan="3"><span style="text-decoration:underline">If WebTrust…</span>
	<li>WebTrust Principles and Criteria for Certification Authorities; <b>and</b></li>
	<li>WebTrust Principles and Criteria for Certification Authorities – SSL Baseline with Network Security</li>
	<li>[<b>and</b> WebTrust for CAs - EV SSL, if issuing EV]</li>
	<br>or…<br>
	<br><span style="text-decoration:underline">If ETSI**…</span>
	<li>ETSI EN 319 411-1 LCP and [DVCP or OVCP]; <b>or</b></li>
	<li>ETSI EN 319 411-1 [NCP or NCP+] and EVCP (if issuing EV)</li></td>
  </tr>
  <tr>
    <td class="tg-3paudit">Cross-Certified Subordinate CA</td>
    <td class="tg-3paudit"><span style="text-decoration:underline">Either</span>:
	<li>Certificate does not include an EKU; <b>or</b></li>
	<li>EKU is present and includes id-kp-serverAuth <span style="text-decoration:underline">or</span> anyExtendedKeyUsage</li></td>
  </tr>
  <tr>
    <td class="tg-3paudit">TLS Subordinate CA or Technically Constrained TLS Subordinate CA</td>
    <td class="tg-3paudit"><span style="text-decoration:underline">Either</span>:
	<li>Certificate does not include an EKU; <b>or</b></li>
	<li>EKU is present and includes id-kp-serverAuth <span style="text-decoration:underline">or</span> anyExtendedKeyUsage</li></td>
  </tr>
  <tr>
    <td class="tg-self">Technically Constrained Non-TLS Subordinate CA</td>
    <td class="tg-self">EKU is present and does not include id-kp-serverAuth or anyExtendedKeyUsage.</td>
    <td class="tg-self" rowspan="2">Minimally expected to be audited as defined in Section 8.7 of the BRs (self-audit).</td>
  </tr>
  <tr>
    <td class="tg-self">All others</td>
    <td class="tg-self">N/A</td>
  </tr>
</tbody>
</table>

\* while existing CA certificates trusted by Chrome MAY have EKU values as described in this table, Applicant PKI hierarchies MUST remain [dedicated to only TLS server authentication use cases](#4-dedicated-tls-server-authentication-pki-hierarchies)<br>
\** accepted on a discretionary basis

#### Annual Audits
Chrome Root Program Participant CAs MUST retain an unbroken, contiguous audit coverage.

Recurring complete (i.e., “full”, “full system”, or “full re-assessment”) audits MUST occur at least once every 365 calendar days (or 366 calendar days in a leap year). These audits MUST begin once a CA’s key material has been generated and MUST continue until the corresponding root CA’s key material has been destroyed or is no longer included in the Chrome Root Store.

#### Ad-Hoc Audits
When deemed necessary, the Chrome Root Program may require Chrome Root Program Participants undergo additional ad-hoc audits, including, but not limited to, instances of CA private key destruction or verification of incident remediation.

#### Reporting Requirements

Audit reports MUST:
*   satisfy the requirements in Section 5.1 (“Audit Statement Content”) of the CCADB Policy, and
*   be uploaded to the CCADB within 92 calendar days from the audit period ending date specified in the audit letter.

### 6. Annual Self-assessments
CA Owners with certificates included in the Chrome Root Store MUST complete and submit an annual self-assessment to the CCADB. Instructions for completing the self-assessment are included in the required assessment [template](https://www.ccadb.org/cas/self-assessment).

A single self-assessment MAY cover multiple CAs operating under both the same CP and CPS(s), or combined CP/CPS. CAs not operated under the same CP and CPS(s) or combined CP/CPS MUST be covered in a separate self-assessment.

The scope of the self-assessment submission MUST include:
*   each root CA certificate included in the Chrome Root Store.
*   any corresponding subordinate CA technically capable of issuing TLS certificates (i.e., explicitly contains an extendedKeyUsage value of id-kp-serverAuth or the functional equivalent, for example, due to the absence of any extendedKeyUsage values).

The self-assessment submission date is determined by the earliest appearing “BR Audit Period End Date” field specified in any of the CA Owner’s “CA Owner/Certificate” CCADB root records that are included in the Chrome Root Store.
*   The initial annual self-assessment MUST be completed and submitted to the CCADB within 92 calendar days from the CA Owner's earliest appearing root record “BR Audit Period End Date” that is after December 31, 2022.
*   Subsequent annual submissions MUST be made no later than 457 calendar days (i.e., 1 year and 92 days) after the CA Owner's earliest appearing root record's “BR Audit Period End Date” for the preceding audit period. CA Owners SHOULD submit the self-assessment to the CCADB at the same time as uploading audit reports.

Chrome Root Program Participants SHOULD always use the latest available version of the self-assessment template. CA Owners MUST NOT use a version of the self-assessment template that has been superseded by more than 90 calendar days before their submission.

### 7. Reporting and Responding to Incidents
The failure of a Chrome Root Program Participant to meet the commitments of this policy is considered an incident, as is any other situation that may impact the CA’s integrity, trustworthiness, or compatibility.

Chrome Root Program Participants MUST publicly disclose and/or respond to incident reports in [Bugzilla](https://bugzilla.mozilla.org/enter_bug.cgi?product=CA%20Program&component=CA%20Certificate%20Compliance), regardless of perceived impact. Reports MUST be submitted in accordance with the current version of [this](https://www.ccadb.org/cas/incident-report) CCADB incident report format and timelines.

While all Chrome Root Program Participants MAY participate in the incident reporting process, the CA Owner whose corresponding certificate is included in the Chrome Root Store is encouraged to disclose and/or respond to incidents on behalf of the Chrome Root Program Participants included in its PKI hierarchy.

If the Chrome Root Program Participant has not yet publicly disclosed an incident, they MUST notify chrome-root-program [at] google [dot] com and include an initial timeline for public disclosure. Chrome uses the information in the public disclosure as the basis for evaluating incidents.

The Chrome Root Program will evaluate every incident on a case-by-case basis, and will work with the CA Owner to identify ecosystem-wide risks or potential improvements to be made that can help prevent future incidents.

Chrome Root Program Participants MUST be detailed, candid, timely, and transparent in describing their architecture, implementation, operations, and external dependencies as necessary for the Chrome Root Program and the public to evaluate the nature of the incident and the CA Owner’s response. When evaluating an incident response, the Chrome Root Program’s primary concern is ensuring that browsers, other CA Owners, users, and website developers have the necessary information to identify improvements, and that the Chrome Root Program Participant is responsive to addressing identified issues.

Factors that are significant to the Chrome Root Program when evaluating incidents include (but are not limited to):
*   a demonstration of understanding of the [root causes](https://sre.google/sre-book/postmortem-culture/) of an incident,
*   a substantive commitment and timeline to changes that clearly and persuasively address the root cause,
*   past history by the Chrome Root Program Participant in its incident handling and its follow through on commitments, and,
*   the severity of the security impact of the incident.

Due to the incorporation of the Baseline Requirements into CA policy documents, incidents may include a prescribed follow-up action, such as revoking impacted certificates within a certain timeframe. If the Chrome Root Program Participant does not perform the required follow-up actions, or does not perform them in the expected timeframe, the Chrome Root Program Participant SHOULD file a secondary incident report describing any certificates involved, the expected timeline to complete any follow-up actions, and what changes they are making to ensure they can meet these requirements consistently in the future.

#### Audit Incident Reports
For ETSI audits, any non-conformity and/or problem identified over the course of the audit is considered an incident and MUST have an [Audit Incident Report](https://www.ccadb.org/cas/incident-report#audit-incident-reports) created in Bugzilla by the Chrome Root Program Participant prior to or within 7 calendar days of the AAL’s issuance date.

For WebTrust audits, any qualification and/or modified opinion is considered an incident and MUST have an [Audit Incident Report](https://www.ccadb.org/cas/incident-report#audit-incident-reports) created in Bugzilla by the Chrome Root Program Participant prior to or within 7 calendar days of the Assurance Report’s issuance date.

### 8. Common CA Database
The Chrome Root Program relies on the [CCADB](https://ccadb.org/) to identify and maintain up-to-date information for Chrome Root Program Participants and the corresponding PKI hierarchies.

Chrome Root Program Participants MUST:
1. Follow the requirements defined in the [CCADB Policy](https://www.ccadb.org/policy).
    *   In instances where the CCADB Policy conflicts with this policy, this policy MUST take precedence.
    *   When a timeline is not defined for a requirement specified within the CCADB Policy, updates MUST be submitted to the CCADB within 14 calendar days of being completed.
2. Disclose all subordinate CA certificates capable of validating to a certificate included in the Chrome Root Store to the CCADB. Disclosure MUST take place within 7 calendar days of issuance and before the subject CA represented in the certificate begins issuing publicly-trusted certificates.
3. Disclose revocation of all subordinate CA certificates capable of validating to a certificate included in the Chrome Root Store to the CCADB within 7 calendar days of revocation.

### 9. Timely and Transparent Communications
At any time, the Chrome Root Program may request additional information from a Chrome Root Program Participant using email or CCADB communications to verify the commitments and obligations outlined in this policy are being met, or as updates to policy requirements are being considered. Chrome Root Program Participants MUST provide the requested information within 14 calendar days unless specified otherwise.

#### Notification of CA Certificate Issuance
CA Owners included in the Chrome Root Store MUST complete the “Chrome Root Program Notification of CA Certificate Issuance” form, made available by emailing chrome-root-program [at] google [dot] com, at least 3 weeks before a CA in the corresponding hierarchy issues a CA certificate that:
*   extends the Chrome Root Store’s trust boundary (i.e., the subject certificate CA Owner is not explicitly included in the Chrome Root Store at the time of issuance), or
*   replaces an unrevoked and unexpired CA certificate whose subject certificate CA Owner is not explicitly included in the Chrome Root Store.

Examples of the above use cases include cross certificates and Externally-operated CA certificates.

Such CA certificates MUST NOT be issued without the expressed approval of the Chrome Root Program.

No other notification or approval is required.

#### Notification of Procurement, Sale, or other Change Control Events
Chrome Root Program Participants MUST NOT assume trust is transferable.

Where permissible by law, Chrome Root Program Participants MUST notify chrome-root-program [at] google [dot] com at least 30 calendar days before any impending:
*   procurements,
*   sales,
*   changes of ownership or operating control,
*   cessations of operations, or
*   other change control events involving PKI components that would materially affect the ongoing operations or perceived trustworthiness of a CA certificate included in the Chrome Root Store (e.g., changes to operational location(s)).

Not limited to the circumstances above, the Chrome Root Program reserves the right to require re-application to the Chrome Root Store.

