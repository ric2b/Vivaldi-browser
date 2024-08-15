---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: bazel-remote-caching
title: Bazel Remote Caching
---

*** note
**NOTE**: **Check with your organization to see if this feature is available.**
***

Packages that build under Bazel have the ability to interact with a remote cache
service, allowing faster builds. Since the use of a cache comes with a variety
of concerns including cost, latency, and security, the decision whether to
enable this feature will be made by individual organizations. Google will use a
remote cache hosted in RBE, but another option or no remote cache at all might
be the right decision for your organization.

[TOC]

## Available Remote Caches

https://bazel.build/remote/caching provides an overview of how remote caching
works in Bazel and lists a few options (though there are others). If your
organization has not yet configured a remote cache server/service, work with the
org to choose one to use and to procure/configure it.

## Configuring Bazel to use the remote cache

Specify settings such as the following in a make.conf file that will be loaded
by your Portage profile for your builds:
* BAZEL_USE_REMOTE_CACHING=true
* BAZEL_REMOTE_CACHE_ENDPOINT=<*YOUR ENDPOINT*>
* BAZEL_REMOTE_CACHE_SILO_KEY=cros
* BAZEL_REMOTE_CACHE_INSTANCE_NAME=projects/<*YOUR GCP PROJECT NAME*>/instances/<*YOUR RBE INSTANCE NAME*>
* CHROMEOS_CI_USERNAME=<*THE USERNAME USED BY YOUR CI BUILD HOSTS*>

Note that `BAZEL_REMOTE_CACHE_INSTANCE_NAME` is only needed when using the RBE
remote cache, and `CHROMEOS_CI_USERNAME` is only needed if you're running builds
on CI hosts you control and they should write to the cache.

## User setup

In addition to the config settings listed above, a user looking to leverage the
remote Bazel cache for locally-executed builds needs to run the following
command to store credentials for Bazel to use:

```bash
(outside)
$ gcloud auth application-default login
```

This will store authentication credentials in `~${HOME}/.config/gcloud` that
will be mapped into the chroot, and which will allow Bazel executions within
your builds to authenticate as you to RBE to access the remote cache it hosts.
If you use a remote cache that is not hosted in Google Cloud, you will need
this file to exist in order to enable remote cache interaction, but can just
`touch ~${HOME}/.config/gcloud/application_default_credentials.json` so it
exists but not worry about its content.

## Packages

Remote caching is currently enabled only for the `sys-cluster/fcp` package.
This feature will eventually be enabled for all Bazel-built packages within
ChromeOS.
