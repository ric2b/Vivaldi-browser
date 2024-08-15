---
breadcrumbs:
- - /chromium-os
  - Chromium OS
- - /chromium-os/chromiumos-design-docs
  - Design Documents
page_name: timekeeping
title: ChromiumOS Timekeeping
---

[TOC]

> **NOTE: This is up-to-date as of CrOS release R120 (LTS).**

## Overview

An accurate up-to-date clock is critical to modern devices. Secure network
communication is fundamentally based on systems having clocks in sync with each
other. Even being out of sync by as little as O(hours) is enough to disrupt some
services.

(See [Is accurate time really necessary?].)

A quick summary of how things work:

*   Time is maintained in the hardware's battery-backed RTC when the system is
    off
*   The Linux kernel syncs the RTC to the the system clock during boot
*   CrOS early init roughly checks the system clock is within a reasonable range
*   tlsdate talks to Google servers to precisely synchronize the system clock &
    RTC at runtime

Note on terminology used in this document:

*   We use "RTC", but it's often more generically referred to as "the hardware
    clock" in wider literature
*   We use "system clock" to refer to the main software clock

A note on scope: this document covers current releases of CrOS. It is not meant
to be exhaustive as to how every release & device behaved. Historical
information is included when it serves to guide decision making.

## Technical Details

When a CrOS device is off, the current time is maintained in the
[real time clock (RTC)][RTC] in [UTC format][UTC]. The exact details vary by
device, but generally speaking, the RTC is integrated into one of the microchips
and is constantly powered even when everything else is off. It might have a
dedicated battery, or be connected to the main laptop battery. Unfortunately, if
those completely drain, then the RTC will reset/clear. Typically that means the
clock is initialized to the [(Unix) epoch][unix-epoch] (1 Jan 1970). Sometimes,
simply entering recovery mode will trigger an RTC reset (e.g. if the RTC is
integrated in the CrOS EC, and the EC is reset during recovery).

When the Linux kernel boots, it initializes the system clock
([`CLOCK_REALTIME`]) from the first available RTC that it discovers. See
[`CONFIG_RTC_HCTOSYS_DEVICE`] in [`drivers/rtc/class.c`]. While there are some
validity checks here, it's more for keeping the value within the range of the
Linux kernel's system clock (roughly, from 1970 -- [2232](#faq-2232)).

When userland starts executing, one of the very first programs run is
[`platform2/init/chromeos_startup`]. The first thing it does is check the system
clock and compare it to a [minimum valid time], a
[compile-time constant][init/startup/constants.h]. If the system clock is older
than that, we immediately set the clock to that time.

(*R121: a [maximum valid time] check added which is [minimum valid time] + 30
years*)

The [tlsdate] service starts in the background after the UI/login screen is
shown, and [system-services][boot-design] has otherwise finished. We first
[sync the system clock to the RTC][init/tlsdated.conf], then launch tlsdated.
From now on, the system clock & RTC are entirely maintained via tlsdated.

[`CLOCK_REALTIME`]: https://pubs.opengroup.org/onlinepubs/9699919799/functions/clock_getres.html
[`CONFIG_RTC_HCTOSYS_DEVICE`]: https://chromium.googlesource.com/chromiumos/third_party/kernel/+/HEAD/drivers/rtc/Kconfig
[`drivers/rtc/class.c`]: https://chromium.googlesource.com/chromiumos/third_party/kernel/+/HEAD/drivers/rtc/class.c
[`platform2/init/chromeos_startup`]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/init/startup/
[init/startup/constants.h]: https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/init/startup/constants.h
[init/tlsdated.conf]: https://chromium.googlesource.com/chromiumos/third_party/tlsdate/+/HEAD/init/tlsdated.conf

### tlsdate

On startup, tlsdated first initializes its own internal timestamp. It starts
with the current system time (which was initialized from the RTC, and roughly
checked by init). It then loads a saved timestamp from the disk that it created
during previous sessions -- if it is within bounds
([minimum][minimum valid time] or [maximum][maximum valid time]), and is newer
than the current system time, then it is used. If the internal timestamp is out
of bounds ([minimum][minimum valid time] or [maximum][maximum valid time]), the
timestamp is reset to the [minimum valid time]. Then tlsdated syncs that
timestamp to the system clock & RTC.

tlsdated periodically runs tlsdate in a sandbox which securely communicates with
[specific Google servers](#faq-which-hosts) to query the time.

Certificates are validated using normal rules, except for the time-based fields
notBefore & notAfter (in order to avoid X509_V_ERR_CERT_NOT_YET_VALID ("cert not
yet valid") and X509_V_ERR_CERT_HAS_EXPIRED ("cert expired") errors). Instead of
using the current system time, the server's time is used, if the server time is
within a valid range ([minimum][minimum valid time] or
[maximum][maximum valid time]). This helps recover from an inaccurate local
clock. Specifically, we utilize tlsdate's `--leap` option.

If the time returned by the server is out of a valid range
([minimum][minimum valid time] or [maximum][maximum valid time]), it is ignored,
and the time is not updated. Otherwise, the time is considered valid, the system
clock is updated, and tlsdated updates the RTC & saved disk timestamp. That
local timestamp is used on the next boot to quickly update the system clock &
RTC if the RTC was behind (e.g. due to powerloss and RTC reset). This means the
clock is allowed to jump forwards or backwards as needed -- the server time is
completely trusted.

Whenever the user tries to [manually set the clock](#faq-manual), all requests
are sent to tlsdated over [D-Bus]. tlsdated will ignore the request if the time
if it is not within a valid range ([minimum][minimum valid time] or
[maximum][maximum valid time]). Otherwise, it will update the system clock, RTC,
and saved disk timestamp immediately.

### Minimum Valid Time

When we build CrOS, we hardcode Jan 1st of the current year as the minimum valid
time. We update this constant yearly. It makes sure that if the RTC has an old
time in it (for any reason), we pull the clock up quickly. It also allows us to
set a valid lower bound when processing time changes (like network syncs).

Hopefully this is recent enough to make the system reasonably usable as we
continue to boot and initialize the UI & network services.

### Maximum Valid Time

When we build CrOS, we hardcode a time as the maximum valid time the system will
accept. This is independent of any particular hardware or software limitation
(see the various year-based FAQs below). Any timestamps that are newer than this
will be rejected, and clocks rolled back to the [minimum valid time] as needed.

tlsdate currently hardcodes to 18 May 2033.

(*R121: updated to [minimum valid time] + 15 years*)

See the [What about tlsdate's max time check?] FAQ for more details.

## FAQ

### Is accurate time really necessary? <a name="faq-really-needed"></a>

[Is accurate time really necessary?]: #faq-really-needed

Here's a non-exhaustive list of situations where accurate time is required. Many
of these are direct from customer reports.

*   HTTPS/TLS connections use [X.509 certificates] with "Not Before" and "Not
    After" fields that are dates that define when the certificate is valid
    *   Some certificates might be valid for years, but it's common practice
        nowadays for these to be valid for only 6 months or less
    *   If the certificates cannot be validated, then secure connections cannot
        be established to many websites
    *   CrOS devices won't be able to login new users since it requires secure
        communication with Google authentication servers
    *   [Chrome sync](https://support.google.com/chrome/answer/165139) will stop
        working (bookmarks/etc...)
*   Older [X.509 certificates] often used weaker algorithms that could be
    feasibly be broken at this point
*   If devices had their clocks rolled back, the broken certificate could be
    reused
*   [EAP-authenticated networks][EAP] (common in enterprise environments)
    require an encrypted connection to an authentication server which requires
    certificate validation which includes date checking
    *   See [What about EAP-authenticated networks?] too
*   HTTP cookies, used to store user credentials, have expiration dates in them
*   CrOS device enrollment checks the clock is accurate within O(hours) and
    blocks attempts otherwise
*   Secure [DNS](whether [DoT], [DoH], or [DNSSEC]) requires accurate time
    keeping in order to resolve hostnames to IP addresses
*   Some protocols include [replay attack] protection, and those frequently
    embed timestamps as one factor as a quick check
    *   [Kerberos] requires clocks to be synced within 5 minutes of each other
    *   While not normally used in CrOS, it is available to enterprise users who
        have their own [SSO] service

A few things to note that should still work even with incorrect local system
time:

*   tlsdate will still synchronize
*   CrOS updates can still download as long as HTTP is working
    *   We check signatures on the files themselves, not on the connection, and
        the signatures do not involve timestamps
    *   If the HTTP connection is blocked, or the downloads fail (e.g.
        corruption, malicious or otherwise), we will fallback to trying HTTPS

### How do I manually fix my clock? <a name="faq-manual"></a>

While CrOS tries very hard to automatically synchronize the clock for you, there
are a few known situations where the only option is to manually adjust it
yourself. Please see the
[Google ChromeOS documentation](https://support.google.com/chromebook/answer/177871)
for more details.

Some undocumented methods:

*   `chrome://set-time`
    *   NB: This stops working once the system is able to sync with the network.
*   [crosh] -> `set_time <date>`
    *   NB: This stops working once the system is able to sync with the network.
*   In the BIOS on non-CrOS hardware (e.g. ChromeOS Flex)

### Why not use htpdate?

CrOS used [htpdate] in the past to get the time via [HTTP HEAD requests] to
<http://www.google.com/> to get the [HTTP Date header]. We switched to tlsdate
with R25 (circa Oct 2012). We ran into a couple of issues:

*   htpdate connections are insecure
    *   HTTP requests and results are all plain text
    *   Subject to [MITM attacks][MITM]
*   <https://crrev.com/c/28564> -- www.google.com redirects yield packets too
    big for htpdate to handle due to redirects when outside of the US
    *   htpdate only checked first 1 KiB of the result
    *   Was changed to 8 KiB in Nov 2021 in v1.2.3 with patch from CrOS
*   htpdate only manages the system clock, not the RTC
    *   CrOS had a shutdown script to sync the system clock to the RTC, but it
        only worked on graceful shutdowns
*   htpdate doesn't play well with proxies, only direct connections
    *   It fails silently if direct connections fail, so the clock never updates
*   htpdate doesn't support subpaths, only the / resource
    *   Was fixed in Nov 2021 in v1.2.3 with patch from CrOS
*   htpdate doesn't check the validity of the result and fully trusts whatever
    the server returns
    *   We added a custom check to compare to build time as a lower bound

[HTTP Date header]: https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Date
[HTTP HEAD requests]: https://developer.mozilla.org/en-US/docs/Web/HTTP/Methods/HEAD

### What if the device doesn't have an RTC?

While annoying during early boot, we should recover quickly. Our init will set
the system clock to the current year, and once tlsdate runs, will catch up to
the last timestamp it synced, and then fully recover to the current time with
network access.

All CrOS devices come with an RTC. This would only affect unsupported (non-CrOS)
hardware.

### Why not NTP? <a name="faq-ntp"></a>

[Why not NTP?]: #faq-ntp

[NTP (Network Time Protocol)][NTP] is complicated, much more than we need. It
also uses protocols (UDP) & ports (123) that might be blocked depending on the
network, and it doesn't support proxies. There are benefits to using protocols
that people can't reasonably block (i.e. HTTP/HTTPS).

There is also a good amount of
[research on the security of NTP](https://www.cs.bu.edu/~goldbe/NTPattack.html).

### Why not Chrony (the NTP client)?

[Chrony], while simpler than the [canonical ntpd implementation][ntpd], is still
based on NTP, and our concerns about NTP are due to the protocol itself, not the
software implementation. See [Why not NTP?] for more details.

### What about recovery/factory/RMA/miniOS?

These environments will initialize the system clock from the RTC while the
kernel boots, and early init will reset the clock to the [minimum valid time] as
needed. But beyond that, they don't synchronize the clock. Generally they don't
need to as they're just installing a new OS image which will boot and follow the
standard procedure outlined above.

(*R121: minimum time changed from 1970*)

NB: miniOS (Network Boot Recovery (NBR)) uses update engine which utilizes HTTP
by default which does not involve timechecks.

### How does OOBE time update work?

There is no difference during the out of box experience (OOBE) flow. It is
exactly the same as described in the
[Technical Details section](#technical-details).

### Why not use the build time as the lower bound?

We could utilize the current build time in our init & tlsdate code instead of
manually maintaining the current year. However, this would break
[reproducible builds] & artifact caching. The increase in accuracy (less than 1
year) is not worth the downsides. Our builds aren't immediately sent to users
either, so it can be O(weeks) from the image build time to when users run it.

It also means we don't need to rely on the build system's clock, and it being
fully available to the compiler. Probably not a concern in practice, but best to
add a dependency on it.

### What about tlsdate's max time check? <a name="faq-tlsdate-max"></a>

[What about tlsdate's max time check?]: #faq-tlsdate-max

When the actual time reaches tlsdate's own [maximum valid time], tlsdate will
permanently ignore updates. This means CrOS will stop synchronizing the clock
with the network, automatically reset its clock back to the
[minimum valid time], and reject any manual update attempts.

This is not ideal, and we're discussing it in
[b/307794426](https://issuetracker.google.com/issues/307794426).

### Which hosts are synchronized with? <a name="faq-which-hosts"></a>

[Which hosts are synchronized with?]: #faq-which-hosts

tlsdate talks to clients3.google.com. This setting is not configurable. If you
want CrOS devices to synchronize & maintain accurate clocks, access to this host
must be allowed. It is not used for anything else (e.g. Gmail).

Please see the
[Enterprise Set up a hostname allowlist](https://support.google.com/chrome/a/answer/6334001)
for more information.

### What if the network hosts are blocked?

If [our synchronization host](#faq-which-hosts) is blocked by the network, then
the clock won't be able to automatically sync. The user will have to
[manually sync it](#faq-manual).

Please see the
[Enterprise Set up a hostname allowlist](https://support.google.com/chrome/a/answer/6334001)
for more information.

### Can users/admins change the host used to synchronize time?

No. See [Which hosts are synchronized with?] for more details.

### Can users/admins change the protocol used to synchronize time?

No. See [Which hosts are synchronized with?] for more details.

### What about TLS connections (not) including timestamps (TLS 1.3)?

While historically the server's time was encoded in the TLS handshake with
clients, and that's how tlsdate fundamentally works, current TLS 1.3 versions
(client & server) randomize the field. See this [SO post] and
[tlsdate GitHub issue] and [IETF post] for more details.

However, the [server we communicate with](#faq-which-hosts) still encodes the
correct time in TLS 1.2 ServerHello messages. The Google team that manages that
service will make sure it works as long as CrOS relies on it. This should hold
us over at least until 2030.

[SO post]: https://security.stackexchange.com/questions/71364
[tlsdate GitHub issue]: https://github.com/ioerror/tlsdate/issues/195
[IETF post]: https://mailarchive.ietf.org/arch/msg/tls/_clS-TIIlZUcid_2S4WPej9iMWk/

### Which certificates are used to verify the network host?

A reduced set of CA ([certificate authority]) certificates is used to verify the
connection. See the [Root CA Certificates on ChromiumOS document] for more
details. The key point is that only the small set of CA's that Google itself
uses are checked, not the entire ecosystem that a browser (e.g. Chrome or
Firefox) would trust, and device administrators cannot inject their own
certificates for this connection.

[Root CA Certificates on ChromiumOS document]: /chromium-os/developer-library/reference/infrastructure/ca-certs/#For-connecting-to-Google-services

### Isn't ignoring certificate errors a security risk?

Sure. In theory, an expired certificate could be leaked, and then loaded &
abused in a [MITM attack][MITM]. Since only Google servers are used, it would
mean Google certificates would have to leak. The chances of this are fairly low,
and the worst that could happen is the clock is changed (within
[minimum][minimum valid time] & [maximum][maximum valid time] constraints). The
attack would have to be sustained -- as soon as the device accessed the real
host again, the time would be restored. It would also be fairly obvious to users
as their clock is usually fairly prominently displayed. Basically, this boils
down to a [DoS] attack -- user data would still be secure.

If root certificates leak, then new intermediate certificates could be generated
with correct timestamps, so that's not really worse than the status quo.

NB: While we aren't ignoring the certificate validity period, we are trusting
the time returned by the server, so if someone has hijacked the connection with
a stolen certificate, they could easily inject a bad timestamp that is within
the bounds of the certificate. So we're effectively ignoring the errors.

### What if HTTPS is restricted?

Then the clock won't be able to automatically sync.

If your clock is out of date, you will need to
[manually fix it yourself](#faq-manual).

### What about EAP-authenticated networks? <a name="faq-eap"></a>

[What about EAP-authenticated networks?]: #faq-eap

[EAP-authenticated networks][EAP], often used in enterprise environments,
require a reasonably accurate clock in order to securely communicate with
authentication servers, and that must complete before the client is allowed to
access the network. If the clock is too far behind, and the client isn't able to
connect, then tlsdate will be unable to communicate with the servers to
synchronize.

In order to recover, you'll have to [manually change the clock](#faq-manual), or
temporarily connect to a different network that does not require EAP.

### Do HTTP headers matter?

tlsdate does not make an HTTP connection, only establishes a TLS connection, and
extracts the time from that. So we don't rely on the server e.g. including an
HTTP Date header, or worry about redirects, or any other HTTP-level issue.

### How is the clock synchronized (step/slew/etc...)?

When a valid timestamp is received, it is set immediately via [settimeofday]. In
other words, we "step the clock". We do not slowly "slew" it to bring it in
sync, nor do we utilize [adjtimex] APIs.

[adjtimex]: https://man7.org/linux/man-pages/man2/adjtimex.2.html
[settimeofday]: https://man7.org/linux/man-pages/man2/gettimeofday.2.html

### What about Roughtime?

[Roughtime] is a Google project that aims to provide secure time
synchronization.

Maybe someday we'll consider switching, but there are no plans currently.

### How is time managed in VMs?

Our VMs have a virtual RTC device that uses the host system clock, so time is
automatically kept in sync.

### How are timezones managed?

All time synchronization in CrOS is based off of UTC which is timezone
independent. That means timezones are out of scope for this document, and
largely irrelevant to keeping the clock up-to-date. Timezones are typically
handled as an offset added to the clock rather than changing the clock to use
the timezone-adjusted value.

The
[Google ChromeOS documentation](https://support.google.com/chromebook/answer/177871)
has some more details for users to manage it.

### What about Y2038 (signed 32-bit time_t)? <a name="faq-2038"></a>

[Y2038] is a bug where the system clock will stop working correctly on 19 Jan
2038. Instead it will overflow and behave as if it's 13 Dec 1901. This is
because time_t is a signed 32-bit value.

This largely affects 32-bit systems whose time_t is 32-bits big. CrOS
transitioned to pure 64-bit x86 long ago, and arm is in progress. None of them
are affected by Y2038 as a result.

As of Dec 2023, Oak (MT8173) devices still run 32-bit arm userland.

Any 32-bit systems that went EOL before that will encounter this bug, but since
they haven't been updated in 10+ years by the time this bug hits, it's unlikely
that any such systems will still be used.

[Y2038]: https://en.wikipedia.org/wiki/Year_2038_problem

### What about Y2059 (15-bit RTC days)? <a name="faq-2059"></a>

Some RTC's store time by counting days with a 15-bit field. That means the limit
is (2^15 / 365 + 1970) -> 2059. Such devices will stop being able to remember
the correct time in the RTC, but tlsdate would recover fairly quickly before the
UI is up.

CrOS isn't tracking which devices contain RTCs with these limits as 2059 is far
beyond any device's EOL date.

### What about Y2106 (unsigned 32-bit time offset)? <a name="faq-2106"></a>

[Y2106] is the limit of unsigned 32-bit offsets from the epoch in seconds.
tlsdate reads 32-bits from the TLS 1.2 connection, so it will definitely stop
working at this point. Hopefully we transition to a different design by then.

CrOS has [b/308741228](https://issuetracker.google.com/308741228) for this (more
to make sure Y2038 doesn't affect us), but it's not a priority.

[Y2106]: https://en.wikipedia.org/wiki/Time_formatting_and_storage_bugs#Year_2106

### What about Y2149 (16-bit RTC days)? <a name="faq-2149"></a>

Some RTC's store time by counting days with a 16-bit field. That means the limit
is (2^16 / 365 + 1970) -> 2149. Such devices will stop being able to remember
the correct time in the RTC, but tlsdate would recover fairly quickly before the
UI is up.

CrOS isn't tracking which devices contain RTCs with these limits as 2059 is far
beyond any device's EOL date.

### What about Y2262 (64-bit nanosecond time_t)? <a name="faq-2232"></a><a name="faq-2262"></a>

The Linux kernel uses a 64-bit time_t that stores time as an offset from the
epoch in nanoseconds. This limits the valid time to 2262. This affects all Linux
based systems, including CrOS. Considering that's more than 200 years from now,
it's unlikely that any software written today will be relevant then, or still
actively running. Anyone reading this document certainly won't be alive.

In reality, the Linux kernel will last boot in 2232:

It's offset from 1 Jan 1970

*   It's signed 64-bit -> only 63-bits
*   It stores nanoseconds -> scale down by 10^9
*   Linux has a max uptime of 30 years (see [`TIME_UPTIME_SEC_MAX`])
*   1970 + ( (2 ^ 63) / (365 * 24 * 60 * 60 * 1000 * 1000 * 1000) ) - 30 -> 2232

[`TIME_UPTIME_SEC_MAX`]: https://chromium.googlesource.com/chromiumos/third_party/kernel/+/HEAD/include/linux/time64.h
[boot-design]: /chromium-os/chromiumos-design-docs/boot-design
[certificate authority]: https://en.wikipedia.org/wiki/Certificate_authority
[Chrony]: https://chrony-project.org/
[crosh]: /chromium-os/developer-library/reference/device/crosh
[D-Bus]: https://en.wikipedia.org/wiki/D-Bus
[DNS]: https://en.wikipedia.org/wiki/Domain_Name_System
[DNSSEC]: https://en.wikipedia.org/wiki/Domain_Name_System_Security_Extensions
[DoH]: https://en.wikipedia.org/wiki/DNS_over_HTTPS
[DoS]: https://en.wikipedia.org/wiki/Denial-of-service_attack
[DoT]: https://en.wikipedia.org/wiki/DNS_over_TLS
[EAP]: https://en.wikipedia.org/wiki/Extensible_Authentication_Protocol
[htpdate]: https://www.vervest.org/htp/
[Kerberos]: https://en.wikipedia.org/wiki/Kerberos_(protocol)
[maximum valid time]: #maximum-valid-time
[minimum valid time]: #minimum-valid-time
[MITM]: https://en.wikipedia.org/wiki/Man-in-the-middle_attack
[NTP]: https://en.wikipedia.org/wiki/Network_Time_Protocol
[ntpd]: https://www.ntp.org/
[reproducible builds]: https://en.wikipedia.org/wiki/Reproducible_builds
[replay attack]: https://en.wikipedia.org/wiki/Replay_attack
[Roughtime]: https://roughtime.googlesource.com/roughtime
[RTC]: https://en.wikipedia.org/wiki/Real-time_clock
[SSO]: https://en.wikipedia.org/wiki/Single_sign-on
[tlsdate]: https://github.com/ioerror/tlsdate
[UTC]: https://en.wikipedia.org/wiki/Coordinated_Universal_Time
[unix-epoch]: https://en.wikipedia.org/wiki/Unix_time
[X.509 certificates]: https://en.wikipedia.org/wiki/Public_key_certificate
