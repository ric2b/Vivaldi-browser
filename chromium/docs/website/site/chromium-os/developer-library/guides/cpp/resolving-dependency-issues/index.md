---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: resolving-dependency-issues
title: Using delegates to Resolve Dependency Issues
---

[TOC]

## Chromium Repo Dependency Checks

The Chromium repository has strict restrictions on which modules are able to
depend on the top level **//ui**, **//chrome**, or **//content** directories.

Directly from the **//chromeos** [README](http://chromeos/README.md):

*   “This directory contains low-level support for Chrome running on ChromeOS.”
*   “//chromeos does not contain any user-facing UI code, and hence it has "-ui"
    in its DEPS. The contents of //chromeos should also not depend on //chrome
    or //content.”

These limitations are enforced using "-chrome" in the DEPS file and apply to the
**//chromeos**, **//ash/webui**, and other low-level directories in the
repository.

When these rules are violated (by including a file from **//chrome/…**),
attempts to upload the offending CL will display the following message:

```python
Running Python 3 presubmit upload checks ...
** Presubmit ERRORS: 1 **
You added one or more #includes that violate checkdeps rules.
  ash/webui/file_with_violation.cc
      Illegal include: "chrome/browser/utils.h"
    Because of "-chrome" from ash/webui's include_rules.

Presubmit checks took 5.4s to calculate.
There were Python 3 presubmit errors.

```

The diagram below gives a rough visualization of the separation of components in
ChromeOS.

![ChromeOS Dependency Chart](/chromium-os/developer-library/guides/cpp/resolving-dependency-issues/chromeos_dependency_chart.png) *ChromeOS
architecture pre-LaCrOs*

## Delegate pattern to use Chrome from Ash

There will be numerous occasions where services in **//ash/webui** will need
access to functions and data only available in **//chrome** (to access the
logged-in user’s
[Profile](https://osscs.corp.google.com/chromium/chromium/src/+/main:chrome/browser/profiles/profile.h;l=204;drc=5e23336d543816202a70de6dc6cdf721350adf22;bpv=1;bpt=1)
for example).

To avoid repo dependency violations, in these situations we can utilize the
delegate pattern to get the data/invoke the actions needed. The delegate pattern
([summary](https://medium.com/@rajkumar_p/delegate-design-pattern-959fd0aa8e95))
in the diagram below shows a simplified version of a function in **//ash/webui**
using a delegate to invoke a function in **//chrome**.

![Sample delegate pattern diagram](/chromium-os/developer-library/guides/cpp/resolving-dependency-issues/delegate_diagram.jpg) ***Sample
delegate pattern diagram***

## Creating a delegate

The below steps will use the existing
[Scan App](https://osscs.corp.google.com/chromium/chromium/src/+/main:ash/webui/scanning/)
implementation in **//ash/webui** to demonstrate examples of the delegate
pattern.

### Define the functions in the header

Here the functions are defined which will be invoked by our **//ash/webui**
service. This header file will be created in **//ash/webui** so the service’s
.cc file will be allowed to call/depend on it.

```c++
class ServiceDelegate {
 public:
  virtual ~ServiceDelegate() = default;

  virtual std::string GetStringFromChromeBrowser() = 0;
};
```

Scan app
[example](https://osscs.corp.google.com/chromium/chromium/src/+/main:ash/webui/scanning/scanning_app_delegate.h)

### Create the implementation class

Since functions in **//chrome** can’t be called from **//ash/webui**, the
functions defined in the delegate header need to be implemented by a file
located in **//chrome**.

*.h file*

```c++
class ChromeServiceDelegate : public ServiceDelegate {
 public:
  ChromeServiceDelegate() override;

  std::string GetStringFromChromeBrowser() override;
};
```

Scan app
[example](https://osscs.corp.google.com/chromium/chromium/src/+/main:chrome/browser/ash/scanning/chrome_scanning_app_delegate.h;bpv=0;bpt=1)

*.cc file*

```c++
ChromeServiceDelegate::ChromeServiceDelegate() = default;

std::string ChromeServiceDelegate::GetStringFromChromeBrowser() {
  return chrome::ChromeBrowserFunction();
}
```

Scan app
[example](https://osscs.corp.google.com/chromium/chromium/src/+/main:chrome/browser/ash/scanning/chrome_scanning_app_delegate.cc;bpv=0;bpt=1)

### Pass the delegate implementation to the service

Probably the most difficult part in the process, the service in **//ash/webui**
needs to be given access to an instance of the delegate implementation created
in **//chrome**. This requires the app to be created/launched from **//chrome**.
Here are two examples of correctly setting up and passing the impl: Scan app
[example](https://osscs.corp.google.com/chromium/chromium/src/+/main:chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc;l=595;drc=5e23336d543816202a70de6dc6cdf721350adf22;bpv=1;bpt=1)
*via chrome_web_ui_controller_factory.cc* Diagnostics Log Controller
[example](https://osscs.corp.google.com/chromium/chromium/src/+/main:chrome/browser/ash/chrome_browser_main_parts_ash.cc;l=1371;drc=5e23336d543816202a70de6dc6cdf721350adf22;bpv=1;bpt=1)
*via chrome_browser_main_parts_ash.cc*

### Invoke the delegate function from the service

Once the delegate impl is passed to the service and stored as a private local
variable, any function defined in the delegate header can now be invoked without
worry of dependency conflicts.

*.h file*

```c++
class Service {
 private:
  std::string UseStringFromChrome();

  // Provides browser functionality from //chrome to the Service UI.
  std::unique_ptr<ServiceDelegate> service_delegate_;
};
```

Scan app
[example](https://osscs.corp.google.com/chromium/chromium/src/+/main:ash/webui/scanning/scanning_handler.h;l=104;drc=5e23336d543816202a70de6dc6cdf721350adf22)

*.cc file*

```c++
std::string Service::GetStringFromChromeBrowser() {
  service_delegate_->GetStringFromChromeBrowser();
}
```

Scan app
[example](https://osscs.corp.google.com/chromium/chromium/src/+/main:ash/webui/scanning/scanning_handler.cc;l=218;drc=5e23336d543816202a70de6dc6cdf721350adf22;bpv=1;bpt=1)

### Example CL
* crrev.com/c/3290952
