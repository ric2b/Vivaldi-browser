---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: test-with-servo
title: Run Tast tests with servo
---

[TOC]

## How to run tast test with servo

**Problem**

I want to run a tast test with servo. Here is an
[example tast test](https://crsrc.org/o/src/platform/tast-tests/src/chromiumos/tast/remote/bundles/cros/feedback/launch_feedback_from_power_button.go)
with Servo.

**Solution**

Step 1: Lease a servo device.

When leasing a DUT with crosfleet, a servo device will also be leased.

```
crosfleet dut lease -board octopus
```

Step 2: SSH to leased DUT and Servo

The above command will get back the location of the leased DUT and servo. With
that, we can ssh into the DUT and the servo. For example, if the DUT is on
`chromeos6-row6-rack12-host14` and the servo is on
`chromeos6-row15-rack8-labstation3`, we can do below command to ssh into them.

```
ssh -L 7778:localhost:22 chromeos6-row6-rack12-host14
ssh -L 7779:localhost:22 chromeos6-row15-rack8-labstation3
```

Step 3: Start the servo

```
start servod PORT=9990
```

Step 4: Run the test

```go
tast -verbose run -var=servo=localhost:9999:ssh:7779 localhost:7778 feedback.LaunchFeedbackFromPowerButton
```

## Useful links

-   [About Crosfleet](http://go/crosfleet-cli#dut-lease)
-   [What is a Servo](http://g3doc/company/teams/chrome/ops/fleet/fleet-ops/docs/team-ref/team-ref-servo-tips)
-   [Running tast test with Servo](http://go/cros-cheat#running-a-tast-test-with-a-servo-dependency-on-a-lab-device)
