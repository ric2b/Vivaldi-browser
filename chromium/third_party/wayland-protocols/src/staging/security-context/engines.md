# security-context-v1 engines

This document describes how some specific engine implementations populate the
metadata in security-context-v1.

## [Flatpak]

* `sandbox_engine` is always set to `flatpak`.
* `app_id` is the Flatpak application ID (in reverse-DNS style). It is always
  set.
* `instance_id` is the Flatpak instance ID of the running sandbox. It is always
  set.

[Flatpak]: https://flatpak.org/
