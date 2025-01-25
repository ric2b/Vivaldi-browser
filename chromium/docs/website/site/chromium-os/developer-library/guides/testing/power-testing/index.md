---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: power-testing
title: Power testing
---

## Description
Modern mobile computers are sold with an advertised battery life, ranging from a
few to tens of hours. Nonetheless, when these devices are used on a daily basis,
many users report that their actual battery life doesn’t match up with the
advertised numbers. For ChromeOS devices, we wanted to try and report battery
life that is as close as possible to what an average user experiences. Thus
`power_LoadTest` was created to emulate average user behavior and measure the
resultant battery life. This test is as an [open source][1] Chrome extension
anyone can install and run.
The `power_LoadTest` runs as a series of one hour long iterations until battery
exhaustion. Within each iteration, a load mix known as **60/20/10/10** is run:
* **[First 60%]** Browsing: a new website is loaded every minute. The web page
  loaded is scrolled down one page (600px) every 10 seconds, then scrolled back
  up at the same rate.
* **[Next 20%]** Email: Gmail is loaded in the foreground tab and audio is
  streamed from a background tab.
* **[Next 10%]** Documents: Various Google Docs are loaded.
* **[Final 10%]** Video: A full screen 720p YouTube Video is played.
The parameters of the device under test (DUT) are as follows:
* Backlight
  * Screen: Set to platform default brightness on battery or 40% if default can
    not be determined via
    [backlight_tool --get_initial_brightness --lux=150][2].
  * Keyboard: Depends on existance of ALS / Hover sensor
  
    | ALS | hover | keyboard backlight level               |
    |-----|-------|----------------------------------------|
    | No  |  No   | default                                |
    | Yes |  No   | 40% of default                         |
    | No  |  Yes  | System with this config does not exist |
    | Yes |  Yes  | 30% of default                         |
* Power management:
  * Dimming, blanking the screen and transitions to standby state are disabled.
  * Ambient light sensor readings are ignored.
* Battery:
  * Device only powered by battery (no AC power).
  * Battery charged to 100% prior to initializing test. Test continues in 1 hour
    iterations until battery passes low threshold (typically set at 3%). Initial
    & remaining battery charge is recorded.
* USB: No external devices connected
* Network: Device is associated with a wireless access point via WiFi.
* Audio: Built-in speakers at 10% volume, Built-in microphone at 10% volume.
Throughout the duration of the test, there are 5 sites loaded in background
tabs. These sites were chosen to represent typical actions of a user on a daily
basis:
* Searching
* Reading news
* Checking on finance
* Shopping
* Communication
## Running
### Via cros_sdk & autotest
If you are interested in running `power_LoadTest` on a ChromeOS system, you
will need a ChromiumOS test image that can be built by following [Build your
own Chromium image][3] instruction with `./build_image --board=${BOARD} test`
command. After the test image is built, you can follow the [Installing Chromium
OS on your device][4] instruction to install the test image to your DUT.
Since running `power_LoadTest` requires that the device is disconnected from the
wired Ethernet (including USB-Ethernet) as well as from the AC power source, it
is trickier to run it compared to [running other autotests][5] if you do not
have both the build machine and the DUT under a same private WiFi network
connected to the Internet.
If your build machine and the DUT are in the same WiFi network, you can run
`power_LoadTest` by running the following command in scripts directory inside
[chroot][6]. (Make sure you have battery fully charged with AC power source and
Ethernet disconnected from the DUT before running the test.)
```bash
test_that ${DUT_ipaddr} power_LoadTest
```
If your build machine is not on the same private WiFi network as the DUT
(applicable for most Googlers and any who have the build machine connected to
a corporate network), follow the instructions below.
* Keep the AC power source plugged and the wired Ethernet connected to the same
  network as your build machine.
* Run the same command as above. It is expected to fail as the pure purpose is
  to copy the test suite over to the DUT from the build machine.
  ```bash
  test_that ${DUT_ipaddr} power_LoadTest
  ```
* Disconnect the wired Ethernet.
* On the DUT, enter VT2 by pressing `Ctrl + Alt + F2` (right arrow or refresh
  key on the Chrome keyboard), and login as `root` (May require password.
  Default is likely `test0000`. See additional details [here][7] if that doesn't
  work.
  ```bash
  cd /usr/local/autotest*
  # remove your AC power source right before running the test.
  bin/autotest tests/power_LoadTest/control*
  ```
* Re-enter VT1 by pressing `Ctrl + Alt + F1` (left arrow on the Chrome keyboard).
* Now you just need to wait until the battery runs out. (Don't worry, the test
  result is stored on the DUT.)
* Reconnect the AC power source, boot up, and enter VT2 as `root`.
  ```bash
  cd /usr/local/autotest/results/default/power_LoadTest/results/
  ```
### Via extension only
As mentioned earlier, `power_LoadTest` uses [chrome extension][8] to drive the
various workloads. As such it can be run directly on a 'normal mode' machine
once the extension is installed.
To run with this method,
* Download the `power_LoadTest` [extension][9] to your device and unpack
  tarball.
* Navigate to `chrome://extensions` and click *load unpacked extension*.
* Choose the *extension* directory from tarball.
You should now have the extension installed and clicking on it will start the
test but before you do that read the following caveats & hints to make this run
go smoother.
* Before starting, note the battery state-of-charge (SOC). The extension will
  only run workload for 1 hour so you'll need to extrapolate total runtime from
  that.
* If device has keyboard backlight be sure its off. See [keyboard backlight][10]
  documentation for details on manually controlling keyboard backlight.
* In order to remove impact of ambient light changes, use brightness keys to set
  the panel brightness to your preferred brightness first.  Note, autotest
  typically sets brightness to ~80nits.
* When test completes be sure to note battery SOC again for calculating battery
  life.
Estimate your total battery life by this calculation,
    100 / (battery_soc_start - battery_soc_end)
For example if you started test at 50% SOC and it ended with 40% SOC your
battery life would be,
    100 / (50 - 40) = 10 hours
##  Interpreting Results
If you ran via cros_sdk & autotest there will be a keyvals file at
`power_LoadTest/results/keyval`. The test will publish *minutes_battery_life*
which we use to track platforms battery life. However that only tells part of
the story. As with any other real world test, the results have other
measurements that should be examined to ensure the battery life estimate is
genuine.
Keyvals of particular interest beyond *minutes_battery_life* to judge quality of
test results are:
* ext_*\_failed_loads
  * Any non-zero values for these keyvals can be problematic. The keyval itself
  is a string with counts separated by underscores. Each count is the failures
  logged by the particular loop. For example ‘0_1_2’ would mean no failures for
  loop0, 1 for loop1 and 2 for loop2. While failures aren’t good if the
  subsequent ext_*\_successful_loads equals the correct quantity then data may
  be deemed ok.
* percent_cpuidle_*
  * These numbers typically stay roughly the same for a particular platform so
  be cogniscent that they roughly match previously qualified PLT runs or the
  differences should be investigated.
* percent_cpufreq_*
  * Similar to cpuidle above this represents the P-state the processor frequency
  runs in. These too stay roughly the same and should be compared to previously
  qualified numbers.
* loop*\_system_pwr_avg
  * Each loop should be roughly the same average power. Often the first loop
  (loop0) consumes more as webpage caching hasn’t occurred yet.
* percent_usb_suspended_*
  * Should be ~100% unless run is being done with expected USB device that has
  been attached externally or has had selective suspend disabled for some
  functional reason.
##  Conclusion
While the initial version of `power_LoadTest` seems to emulate well what users
experience every day on ChromeOS devices, this test will be constantly
improved. As we learn more about how users use ChromeOS devices and how
experienced battery life differs from tested battery life, we will use this
data to refine the test, potentially changing the load mix or the parameters
of the test. Our goal is to ensure that when you purchase a device, you know -
with reasonable certainty - how long that device will last in your daily use.

[1]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/main/client/site_tests/power_LoadTest/
[2]: https://chromium.googlesource.com/chromiumos/platform2/+/main/power_manager/tools/backlight_tool.cc#154
[3]: https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide/#build-a-disk-image-for-your-board
[4]: https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide/#installing-chromiumos-on-your-device
[5]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+/HEAD/docs/user-doc.md#running-tests
[6]: https://www.chromium.org/chromium-os/developer-library/guides/development/developer-guide/#create-a-chroot
[7]: https://www.chromium.org/chromium-os/developer-library/guides/debugging/debugging-tips/
[8]: https://developer.chrome.com/extensions
[9]: https://chromium.googlesource.com/chromiumos/third_party/autotest/+archive/main/client/site_tests/power_LoadTest.tar.gz
[10]: https://chromium.googlesource.com/chromiumos/platform2/+/main/power_manager/docs/keyboard_backlight.md
[11]: http://go/cros-plt-doc
