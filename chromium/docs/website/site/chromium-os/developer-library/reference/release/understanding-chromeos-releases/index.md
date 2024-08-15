---
breadcrumbs:
- - /chromium-os/developer-library/reference
  - ChromiumOS > Reference
page_name: understanding-chromeos-releases
title: Understanding ChromeOS releases
---

## Release cycles

ChromeOS operates on a 4 week cycle, and operates on 5 channels:

- Extended Stable: The stable channel, but longer. This channel is only
available to enrolled devices.
- Stable: Most users should be on this.
- Beta: A good balance between stability & new features for users.
- Dev: Passed automated tests, but can still be a bit unstable. Most users
should not use this unless there's a specific feature they want to test out.
- Canary: Might not have passed all tests. Meant for developers only to verify
the latest code. Not intended to be used as a primary or day-to-day system by
anyone.

Read more about ChromeOS releases
[here](https://www.chromium.org/chromium-os/developer-library/reference/release/releases/).

To see the current schedule of ChromeOS release, see go/chrome-schedule.
To easily stay up to date with the release schedule, add the
[Chrome Milestone calendar](https://calendar.google.com/calendar/u/0/embed?src=google.com_c1f795f7cg83sv2qu15c4v1kfk%40group.calendar.google.com)
to your calendar by following
[this link](https://calendar.google.com/calendar/u/0/embed?src=google.com_c1f795f7cg83sv2qu15c4v1kfk%40group.calendar.google.com)
and clicking the "+" button in the bottom right.

## Release version numbers

Releases are fully declared like so: `13950.0.0, 92.0.4490.0`. You can find all
releases at go/goldeneye.

The first value is the ChromeOS, or Platform, version. It iterates with each
ChromeOS release. The second value is the Chrome browser version. The first
number of this value (92) is the "milestone". The same version of Chrome can be
packaged with several subsequent ChromeOS releases. That means you will often
see, on go/goldeneye, lists of releases like: `13950.0.0, 92.0.4490.0`,
`13951.0.0, 92.0.4490.0`, `13952.0.0, 92.0.4490.0`, etc.

Because ChromeOS and Chrome version are not coupled together each release,
you'll often see people use the shorthand of milestone + ChromeOS version,
e.g., `R92-13950.0.0`.

You can find these version numbers as well as the current revision that your
device is running at the top of chrome://version.

## How do I determine which release a commit landed in?

### Why do I want to do this?

Example: You fix a crash, and a couple weeks later want to examine the crash
dashboard at [crash/](https://crash.corp.google.com/) to understand if your fix
really worked. You'd want to know which version of Chrome or ChromeOS your fix
landed in, and then only look at that release or later.

### Method 1 (preferred): Chromium CL Finder extension

Install the
[Chromium CL Finder extension](https://chrome.google.com/webstore/detail/chromium-cl-finder/egncfhncpaakcfegigpnijpdlffhljcc),
and then navigate to any CL. See the instructions in the link for how to find
the "Landed in" information.

### Method 2: go/omahaproxy

Copy the commit hash from your CL of interest, and paste it into go/omahaproxy.

## Additional Resources

- See go/cros-merge-guidelines for information on merging code to release
branches.
- See go/chromeos-release-blockers for information on release block issues.
- See go/flt-process for information about ChromeOS Feature Launch Process.