---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: faq
title: Build FAQ
---

[TOC]

# Google Storage

## What bucket can I use for testing?

If you are a Googler, you can use gs://chromeos-throw-away-bucket for testing.
Keep in mind that the contents can be read or deleted by anyone at Google.

We also have another similar bucket named gs://chromeos-shared-team-bucket .
Probably using gs://chromeos-throw-away-bucket makes more sense since it is
clear that the contents there can be deleted by anyone at Google.

If you are not a Googler, you should create your own bucket in Google Storage.

# Portage/Emerge

## How do I force emerge to use prebuilt binaries?

```none
emerge -g --usepkgonly
(Note: The --getbinpkg only flag for emerge does not actually work)
```

## How do I check the USE flags for a package I am emerging?

Emerge will tell you what USE flags are used when the **-pv** flags are provided
e.g.

```none
emerge-$BOARD chromeos-chrome -pv
```

# Development

FAQ items covering general development questions

## What should I read first/Where do I start?

<https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide/>

## How can I make changes depend on each other with Cq-Depend?

<https://www.chromium.org/chromium-os/developer-library/guides/development/contributing/#cq-depend>

## How do I create a new bucket in gsutil?

[gsutil FAQ](/chromium-os/developer-library/reference/tools/gsutil/#FAQ)

## How do I port a new board to Chromium OS?

<https://www.chromium.org/chromium-os/developer-library/guides/chromiumos-board-porting-guide/>

## Making changes to the Chromium browser on ChromiumOS (AKA Simple Chrome)

<https://www.chromium.org/chromium-os/developer-library/guides/development/simple-chrome-workflow/>

## How to install an image on a DUT via cros flash?

<https://www.chromium.org/chromium-os/developer-library/reference/tools/cros-flash/>
