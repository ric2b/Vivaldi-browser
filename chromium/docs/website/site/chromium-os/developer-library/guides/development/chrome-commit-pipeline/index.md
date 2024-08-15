---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: chrome-commit-pipeline
title: Life of a Chrome commit on ChromeOS
---

This document provides a brief overview of how Chrome changes get committed
and tested on ChromeOS.

For details on non-Chrome **ChromeOS** changes, see
[Life of a ChromeOS commit](/chromium-os/developer-library/reference/development/cros-commit-pipeline/).

## Create a Chrome change

### Make and upload changes

See the section on [contributing code] at [chromium.org]
for how to create a branch and make changes.

Once a change is completed and tested locally, upload it to [codereview]:

```git cl upload```

### Have your change reviewed

Before submitting the code, use [Gerrit] to have it reviewed like any standard
ChromeOS CL. See [contributing code] for details.

## The Chrome commit pipeline

### The Commit Queue and Tryservers

The Chrome [commit queue] has a very large pool of builders that will apply
individual changes to the master, build them, and test them.

#### Before the patch is approved

A developer can click on 'Choose trybots' to select specific builders to run
(there are a lot of them).

Alternately they can click 'CQ dry run' to run all of the builders that the
CQ will run in advance, without scheduling a commit.

#### The commit queue

Once a change has been reviewed and approved, the developer can apply the CQ+2
label, which marks the change ready for the CQ.

Depending on what was changed, the CQ selects a suite of trybots for
[android], [cros], [linux/fuchsia], [mac/ios], and [win].

The [cros] trybots include:
- [linux-chromeos-rel]: a linux builder with `target_os = "chromeos"` that runs
`browser_tests` and `unit_tests` for Chrome on ChromeOS.
- [chromeos-amd64-generic-rel]: a Simple Chrome builder that runs telemetry and
dep:Chrome Tast tests in amd64-generic VMs.

If the CQ builders succeed then the change will be committed to the master.
Otherwise the 'CQ+2' label will need to be reapplied once the failure is fixed
or determined to be unrelated to the change.

### The chromium waterfall

Once a change is committed on the master, it is picked up by the
[chromium waterfall]. This includes a very large number of builders that will
thoroughly test the commit, including a number of [public ChromeOS builders]
and a few [private builders]. Their tests can run on linux-chromeos, ChromeOS
VMs, and ChromeOS DUTs (Device Under Test).

*Note: Due to lab limitations not every builder in the waterfall is included
in the Commit Queue. For ChromeOS there are a few Debug and device testers that
only exist on the waterfall. Failures there are infrequent but possible, so keep
an eye out!*

## The ChromeOS commit pipeline for Chrome changes

Once a Chrome change lands in the master, it needs to be landed via [PUpr]
uprev before it will be picked up by ChromeOS. PUpr watches chrome tags
as they are created each day and cuts a CL to the current gardener to land.
Changes are included in ChromeOS builds once the uprev CL makes it through
the ChromeOS CQ. There is a [PUpr Dashboard] that shows the current status
of the uprev CLs as well as the age of the current chrome version on ChromeOS.

[contributing code]: /chromium-os/developer-library/guides/development/contributing/
[commit queue]: https://chromium.googlesource.com/chromium/src/+/HEAD/infra/config/generated/cq-builders.md
[chromium.org]: https://www.chromium.org
[Gerrit]: https://chromium-review.googlesource.com/
[android]: https://ci.chromium.org/p/chromium/g/tryserver.chromium.android
[cros]: https://ci.chromium.org/p/chromium/g/tryserver.chromium.chromiumos
[mac/ios]: https://ci.chromium.org/p/chromium/g/tryserver.chromium.mac
[linux/fuchsia]: https://ci.chromium.org/p/chromium/g/tryserver.chromium.linux
[win]: https://ci.chromium.org/p/chromium/g/tryserver.chromium.win
[linux-chromeos-rel]: https://ci.chromium.org/p/chromium/builders/luci.chromium.try/linux-chromeos-rel
[chromeos-amd64-generic-rel]: https://ci.chromium.org/p/chromium/builders/luci.chromium.try/chromeos-amd64-generic-rel
[chromium waterfall]: https://ci.chromium.org/p/chromium/g/main/console
[public ChromeOS builders]: https://ci.chromium.org/p/chromium/g/chromium.chromiumos/builders
[private builders]: https://ci.chromium.org/p/chrome/g/chrome/builders
[PUpr]: https://g3doc.corp.google.com/company/teams/chrome/ops/chromeos/chromeos-infra/continuous_integration/pupr.md
[PUpr Dashboard]: http://go/chromeos-dash-pupr-chrome
