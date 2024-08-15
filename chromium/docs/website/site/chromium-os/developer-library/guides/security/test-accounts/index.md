---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: test-accounts
title: Owned Test Accounts (OTAs) for Manual Testing
---

Team members are encouraged to obtain their own OTAs via [Rhea].
[go/get-test-accounts] has instructions and more.

## Exercising flows that require OTAs with passwords

OTAs generally do not allow authentication using passwords for security reasons.
Instead they use a challenge-response; usually against your @google.com account.
See [go/cross-account-challenge-for-ota]. This works with almost all manual
authentication flows. One example exception is the ChromeOS flow for adding a
supervisor for a child account - a la Family Link.

To work around the exceptions, we have a few shared accounts that are exempt
from the challenge-response requirement and are able to use passwords for
authentication. We don't have many of these so please **do not** use these
shared accounts unless you have this password requirement.

The list of exempt accounts can be found on the [team's Rhea dashboard]. Account
IDs are of the format test.crossec.ota.pass%d@gmail.com

1.  Make sure you're a member of the [chromeos-security-core.prod] group.
2.  Randomly select an account. We don't expect much usage so there's no
    arbitration in place.
3.  Credentials > Change Password
    -   Generate a random password using your favourite generator.
    -   Passwords expire and have to be manually refreshed every 24hrs.
4.  Use the account.
5.  Once done, please undo any significant changes such as having Family Linked
    the shared account to your own OTA.

[Rhea]: https://rhea.corp.google.com/
[go/get-test-accounts]: https://goto.google.com/get-test-accounts
[go/cross-account-challenge-for-ota]: https://goto.google.com/cross-account-challenge-for-ota
[team's Rhea dashboard]: https://rhea.corp.google.com/list?mdb_group=chromeos-security-core
[chromeos-security-core.prod]: https://ganpati2.corp.google.com/group/chromeos-security-core.prod?tab=children
