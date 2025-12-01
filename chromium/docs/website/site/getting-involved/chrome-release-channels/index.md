---
breadcrumbs:
- - /getting-involved
  - Getting Involved
page_name: chrome-release-channels
title: Chrome Release Channels
---

[TOC]

Chrome supports a number of different release channels. We use these channels to
gradually roll out changes to users, starting with our twice-daily Canary
channel builds, all the way up to our Stable channel releases that happen every
4 weeks.

### Channels

#### Windows, macOS, and Linux
All channels may be run in parallel -- they install into distinct directories
and use dedicated User Data directories.
*   [Stable](https://google.com/chrome)
*   [Beta](https://google.com/chrome/beta)
*   [Dev](https://google.com/chrome/dev)
*   [Canary](https://google.com/chrome/canary)

To download Chrome for a specific architecture or OS, use the "Other Platforms"
link at the bottom of the page.

#### Android
*   [Stable](https://play.google.com/store/apps/details?id=com.android.chrome)
*   [Beta](https://play.google.com/store/apps/details?id=com.chrome.beta)
*   [Dev](https://play.google.com/store/apps/details?id=com.chrome.dev)
*   [Canary](https://play.google.com/store/apps/details?id=com.chrome.canary)

#### iOS
*   [Stable](https://itunes.apple.com/us/app/chrome-web-browser-by-google/id535886823?mt=8)
*   [Beta](https://testflight.apple.com/join/LPQmtkUs)

### How do I choose which channel to use?

The release channels for Chrome range from the most stable and tested (Stable
channel) to least stable (Canary channel). On Windows, macOS, and Linux, you can
use multiple channels at once. This allows you to play with our latest code,
while still keeping a tested version of Chrome around.

*   **Stable channel:** This channel has gotten the full testing and
            blessing of the Chrome test team, and is the best bet to avoid
            crashes and other issues. It's updated every week for minor
            releases, and every four weeks for major releases.
*   **Beta channel:** If you are interested in seeing what's next, with
            minimal risk, Beta channel is the place to be. New features spend
            about a month in Beta before being promoted to Stable.
*   **Dev channel:** If you want to see what's happening quickly, then
            you want the Dev channel. The Dev channel gets updated once or twice
            weekly, and it shows what we're working on right now. There's no lag
            between major versions, whatever code we've got, you will get. While
            this build does get tested, it is still subject to bugs, as we want
            people to see what's new as soon as possible.
*   **Canary build:** Canary builds contain changes as soon as we make them, and
            are released twice daily with only automated testing.
*   **Other builds:** It is also possible to download specific Chromium builds
            from [this Google Storage
            bucket](http://commondatastorage.googleapis.com/chromium-browser-continuous/index.html).

**Note**: Early access releases (Canary builds and Dev and Beta channels) will
be only partly translated into languages other than English. Text related to new
features may not get translated into all languages until the feature is released
in the Stable channel.

### Reporting Dev channel and Canary build problems

Remember, Dev channel browsers and Canary builds may still crash frequently.
Before reporting bugs, consult the following page:

*   [Bug Life Cycle and Reporting
            Guidelines](/for-testers/bug-reporting-guidelines)

If after reading the above, you think you have a real bug,
[file it](https://crbug.com/new)!
