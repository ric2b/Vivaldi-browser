---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: open-app-as-swa-vs-swd
title: How to open other SWA/SWD from your SWA
---

[TOC]

## How to open other SWA/SWD from your SWA

**Problem**

I want to open other app as a SWA (system web app) or a SWD (system web dialog).

**Solution**

***SWA***

SWA is a standalone app. You can resize it and do anything like other standalone
apps. To open as a SWA:

```c++
web_app::LaunchSystemWebAppAsync(profile_, ash::SystemWebAppType::DIAGNOSTICS);
```

e.g. Opens Diagnostics App and Explore App from Feedback App.
[Code link](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/os_feedback/chrome_os_feedback_delegate.cc;l=224-231).

***SWD***

SWD is always on top of other windows. You cannot resize it. To open as a SWD:

```c++
chromeos::DiagnosticsDialog::ShowDialog();
```

e.g. Open Diagnostics Dialog from Shimless RMA.
[Code link](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/shimless_rma/chrome_shimless_rma_delegate.cc;l=29-36).

**Example CL**

*   Open as SWA: https://crrev.com/c/3688711.
*   Open as SWD: https://crrev.com/c/3257886.

**References**

*   [System web app](https://chromium.googlesource.com/chromium/src/+/HEAD/chrome/browser/ash/system_web_apps/README.md)
*   [More about SWA vs SWD](https://docs.google.com/document/d/1vbE-Dl9Z2jWdYWsNlQMDJxFJnr6TqTN8Q_cGuDfU_zI/edit#)
