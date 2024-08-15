---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: green-stainless-report
title: Common Green Stainless updates
---

[TOC]

## Exclude flaky or pre-release from report

**Problem**

Want to exclude a test from being reported in Stainless weekly email temporarily
while working on the cause of the flake or until the feature is ready to be
enabled by default.

**Solution**

Temporarially you can update the "In Development" sheet in
go/cros-green-stainless-all to `In Development` for the test(s) that should
not be listed in the report. Once tests are ready remove from sheet.

See "Status Types" document for background.

## Update bug in report

**Problem**

Bug related to failing/flakey test in weekly report or on dashboard is wrong or
missing.

**Solution**

Add an entry to the "BUG" sheet in go/cros-green-stainless-all
with the name of your test and the ID for the bug. If multiple bugs you can
comma separate them.

<!--*
# TODO: Add a blurb on how to get a missing test loaded into the spreadsheet.
*-->

## Stainless References

*   Introduction to
    [Stainless](https://g3doc.corp.google.com/company/teams/chromeos/subteams/platforms/tpgm/reference-material/stainless-introduction.md)
*   [Green Stainless Goals - Google Docs](http://doc/1TTA62izAYHylqPSFc9hyNeqr0IEVz200C3Nzs05aEK4#heading=h.y11rhgktwtrv)
*   Current Stainless [Tast Spreadsheet](http://go/cros-green-stainless-all)
*   Status Tab columns and usage
    *   [Status Types](http://doc/1zuw3MJxAwQ0Y2SQgv94OvVN2zIebWN9LWUhOwevFavg#heading=h.o1c5a82u1jb1)
    *   [Gating Types](http://doc/1zuw3MJxAwQ0Y2SQgv94OvVN2zIebWN9LWUhOwevFavg#heading=h.a7uvxmpqp0i)
*   CrOS Automated Test [Dashboard](http://go/cros-automated-test-dashboard)
*   Green stainless report
    [code](https://source.corp.google.com/piper///depot/google3/googleclient/chrome/chromeos_pmo/platform/green_stainless/)
