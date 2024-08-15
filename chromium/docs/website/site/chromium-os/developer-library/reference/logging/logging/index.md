---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: logging
title: Logging on ChromeOS
---

## Syslog Logs (managed by rsyslog)

Services write the logs into files using syslog similar to Linux. Applications can call the syslog APIs which writes the messages into the syslog socket (/dev/log). The syslog daemon (rsyslogd) receives the message and the daemon writes it to the file.

## Locations

System logs are collected by rsyslog daemon from applications and stored in `/var/log/`. The rule of destinations is defined in the configuration files.

Logs defined in [rsyslog.chromeos](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/init/rsyslog.chromeos):

* `/var/log/messages`: general system logs.
* `/var/log/net.log`: network-related logs.
* `/var/log/boot.log`: boot messages.
* `/var/log/secure`: logs with authpriv facility. May contain sensitive information.
* `/var/log/upstart.log`: upstart logs.

Logs defined in [rsyslog.arc.conf](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/arc/scripts/rsyslog.arc.conf):

* `/var/log/arc.log`: ARC-related logs gathered by rsyslogd.

Logs defined in [rsyslog.hammerd.conf](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/hammerd/rsyslog/rsyslog.hammerd.conf):

* `/var/log/hammerd.log`: network-related logs gathered by rsyslogd.

Logs defined in [rsyslog.secagentd.conf](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/secagentd/rsyslog/rsyslog.secagentd.conf):

* `/var/log/secagentd.log`: secagentd-related logs gathered by rsyslogd.

## Creating a new log file

**Note:** These steps must be done concurrently or it will not work correctly.

**Create a tmpfiles.d config file.**
* Creates the new log file that will be used. [(Example)](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/secagentd/tmpfiles.d/secagentd.conf)

**Add the new log file to sepolicy.**
* Sets up configuration with SELinux. [(Example CL)](https://chromium-review.googlesource.com/c/chromiumos/platform2/+/4136753)

**Create a rsyslog config file.**
* Sets up rules for logging, i.e. setting destination or changing format. [(Example)](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/secagentd/rsyslog/rsyslog.secagentd.conf)

## Format

We are using a custom format for ChromeOS system log. It has microsec precision and a time zone depending on the system setting.

Example of format:

> 2020-03-10T14:02:08.470152+09:00 INFO processname[12345]: This is the log message.

The format is defined in [rsyslog.chromeos](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/init/rsyslog.chromeos).

## Journald Deprecation

Jounald is deprecated and is about to be removed. Journal log has a storage and CPU overheads and it’s not suitable for, at-least, ChromeOS devices. Journal log has multiple non-important fields and has multiple wide (64 or 128bit) integers for IDs. These metadata consumes storage in devices. And journald has a dedup future which saves storage for the same value entries. But in ChromeOS use-case, we don’t have much duplicated entries and dedup needs CPU time to look up a duplicated previous entry. The impact is not ignorable especially for low-end devices. So we have decided to remove journald from ChromeOS until these problems are addressed .


## Log Rotation

Major log files are rotated by [chromeos-cleanup-logs](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/init/chromeos-cleanup-logs) script, which rotates logs every 24 hours and keeps 7 histories. Add your log path to [kLogsToRotate](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/croslog/constants.h), for it to be rotated.

Chrome logs are rotated by itself (to be more specific, a new log file is generated when every process starts). And old logs are removed by chromeos-cleanup-logs script.

## User Feedback Reports

To include log files in the User Feedback Reports add your filepath to [kVarLogFileLogs](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/debugd/src/log_tool.cc). To manually limit the size of your logs use `kCommand` with `tail`.

## Logs from Chrome processes

See the chrome side document: [Chrome Logging on ChromeOS](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/chrome_os_logging.md)
