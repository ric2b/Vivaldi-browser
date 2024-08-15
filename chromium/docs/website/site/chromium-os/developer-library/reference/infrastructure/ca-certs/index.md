---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: ca-certs
title: Root CA Certificates on ChromiumOS
---

TLS clients need a set of root CA certificates to establish the chain of trust
when connecting to a service on the Internet. On ChromiumOS systems, three sets
of certificates exist for different purposes:

  - The ChromeOS root store for Google distributed ChromeOS systems
  - The Mozilla NSS root store for other ChromiumOS systems
  - A store of roots for connecting to Google services

This document explains in detail the goal of each store, and how various clients
use them.

[TOC]

## The ChromeOS root store

Google distributes a ChromeOS root store for ChromeOS systems. It is
maintained by Google explicitly for the purpose of making safe and secure TLS
connections. Clients like Chrome and programs that link [libchrome] source this
store for roots.

The certificates in this store are installed as a shared library (libnss) by the
package `chromiumos-overlay/dev-libs/nss`. It is a fork of a Gentoo upstream
package with the same name, with added/removed certificates based on Google's
policy.

## The Mozilla NSS root store

For ChromiumOS systems _not distributed_ by Google, the [Mozilla NSS store] is
provided. It includes a broad selection of CAs, across a variety of use cases
(such as trusting both TLS and S/MIME CAs). Users of this store should be aware
that because it does not express trusted purposes, it may contain CAs that are
not suitable for their use case. Applications and distributors are encouraged to
manage and maintain their own root store, and to ensure its fitness for purpose
and policy.

The certificates in this store are installed by the
`portage-stable/app-misc/ca-certificates` package at `/etc/ssl/certs`, both
individually and as a concatenated PEM file at
`/etc/ssl/certs/ca-certificates.crt`. This is done so because some clients like
OpenSSL expect individual certificates (with hashed names, `man
X509_LOOKUP_HASH_DIR`), while others (Golang) look for a single file at
`/etc/ssl/certs/ca-certificates.crt`.

The `portage-stable/app-misc/ca-certificates` package unpacks the certificate
list directly from NSS release tarballs, but uses the `update-ca-certificate`
tool from the Debian [ca-certificates] package to install them to the locations
mentioned above. Hence the package is versioned with both NSS and
`ca-certificate`. For example, `app-misc/ca-certificates-20170717.3.36.1` uses
the scripts from the 20170717 release of the `ca-certificate` Debian package and
installs certificates from the 3.36.1 release of the Mozilla NSS module.

## For connecting to Google services

While the NSS root store can serve a variety of applications, and the ChromeOS
root store is suitable for Chrome on ChromeOS, clients that will/should only
ever connect to Google services use a more restrictive set. This set,
synchronized with [pki.goog/roots.pem], contains those CAs that Google does or
may use for various services, and is a significantly smaller set than a more
general root store intended for interacting with the Web at large. You can find
more details at this [FAQ page].

The certificates in this store are installed by the
`chromiumos-overlay/chromeos-base/chromeos-ca-certificates` package at
`/usr/share/chromeos-ca-certificates`. Clients that should only connect to
Google, e.g., the update engine and crash sender, are explicitly configured to
source these certificates as roots.

## The ChromeOS store v.s. the Mozilla NSS store

While both stores trace their roots to the Mozilla NSS module (through
`dev-libs/nss` and `app-misc/ca-certificates` respectively) and they are likely
to have a large overlap, Google may and have added/removed certificates to
ensure that only CAs trusted for TLS (via policy) are trusted for TLS (via
code). The biggest difference of the two is that the NSS store includes CAs
trusted for S/MIME CAs while the ChromeOS store doesn't.

To ensure that any changes to both stores are reviewed, and any differences
expected, the [security.RootCA] test checks to ensure there are no accidental
additions or removals, and that all differences are intentional.

## The old chromeos-base/root-certificates

This was an attempt to replace `portage-stable/app-misc/ca-certificates` with a
ChromiumOS fork, `chromeos-base/root-certificates`. It existed from 2013 to May
2018 and was predicated on the risk due to the lack of Gentoo policies - that
they would pick any CA. That has changed now and as explained above
`app-misc/ca-certificates` now takes the certificates directly from NSS.
Therefore chromeos-base/root-certificates was deleted.

[libchrome]: /chromium-os/developer-library/guides/infrastructure/libchrome/
[Mozilla NSS store]: https://wiki.mozilla.org/CA
[pki.goog/roots.pem]: https://pki.goog/
[ca-certificates]: https://packages.debian.org/sid/ca-certificates
[security.RootCA]: https://chromium.googlesource.com/chromiumos/platform/tast-tests/+/HEAD/src/chromiumos/tast/local/bundles/cros/security/root_ca.go
[pki.goog/roots.pem]: https://pki.goog/roots.pem
[FAQ page]: https://pki.goog/faq/
