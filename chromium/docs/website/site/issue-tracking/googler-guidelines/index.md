
# Chromium Issue Tracker Guidelines for Google employees

The new Chromium issue tracker will be used by lots of Google teams, and is very
flexible in terms of workflows. This documents guidance for Chrome teams within
Google that is in alignment with our open source project norms. It’s important
to Chrome that we preserve our culture of being open by default.

## Guidelines

- **Bugs should be open by default, unless there is specific information that
  should be restricted to Googlers.**
  Chromium is an open-source project.

- **Even security bugs should not normally be restricted to Googlers.**
  Chromium’s reputation depends upon us being open across all our open-source
  contributors. Security bugs start as restricted to the Chromium security team,
  instead of being restricted to Googlers. (Unless embargoed, security bugs are
  made public after their fix is rolled out.)

- **Use the Chromium issue tracker not the internal Google view, where
 possible.** If you see this banner, click the button:\
  <img src="banner.png" alt="This issue is part of the chromium tracker." width=770 height=58>\
 Using Google’s internal view of the same bugs will result in new bugs or
 attachments being hidden from the open source community _by default._\
  <img src="access-warning1.png" alt="Because this issue will be on a public tracker, the following Issue Access Limit will be applied: Limited visibility + Google. You may modify this access level before creating the issue." width=603 height=97>\
  <img src="access-warning2.png" alt="Because this issue is displayed on a public tracker, this comment will be Restricted. You may modify the restriction level before submitting the comment." width=504 height=96>

- **If you use the Google bug tracker (b.corp.google.com) please take extra
  steps to make things open:** For issues: change this setting\
  <img src="issue-access.png" alt="Limited visibility + Google" width=247 height=65>\
  For comments adding attachments: change this setting \
  <img src="attachment-access.png" alt="Restricted" width=139 height=47>

- **Using Google-internal tools is fine, but ensure sufficient information is
  available externally.** Some Google-internal tools will store metadata on bug
  entries that may not be available outside Google. Consider if that’s
  appropriate. Just as seriously, such tools may direct you towards Google’s
  internal view of the bug entries, which may cause you to run into the previous
  problems. Be careful!

- **When referencing issues please use the external/Chromium tracker link**:
  use [crbug.com](https://crbug.com) or [issues.chromium.org](https://issues.chromium.org)
  rather than b/ or g-issues.chromium.org. Internal / g-issues.chromium.org
  links may redirect to a Google SSO login page and create an inefficient
  and sub-optimal workflow for non-Googlers. Similarly, other
  corp.google.com links are not OK, e.g. crbug/.

- **Usage of severity and Priority should align with Chromium severity and
  priority guidelines, not Google guidelines**. For example, severity and
  priority for security bugs should align with [Chromium Security Severity
  Guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/security/severity-guidelines.md),
  with S0 = Critical Severity, S1 = High Severity, S2 = Medium Severity, and S3
  = Low Severity. S4 should be considered the lack of a security severity
  (equivalent to the absence of a severity label on Monorail.)

- **There should be only one, canonical version of a Chromium issue rather than
  an external version and an internal version.** Any duplicate issues should be
  merged as Duplicate to the older or other canonical version of the issue.

  - Duplicate internal versions should not be created as a means for restricting
    access, please use the appropriate visibility / Issue Access levels.

- **Avoid using go/links to link to templates** - they’re not accessible
  externally.

- **When flipping a bug from Vulnerability to Bug (i.e. from a security bug to a
  functional bug), also flip visibility to Default Access.** Don’t just remove
  the [**security@chromium.org**](mailto:security@chromium.org) collaborator,
  because then it will become an orphan bug which needs to be rescued.

- Any additional issues, please reach out to
[chromium-issue-tracker-admins@google.com](chromium-issue-tracker-admins@google.com)
