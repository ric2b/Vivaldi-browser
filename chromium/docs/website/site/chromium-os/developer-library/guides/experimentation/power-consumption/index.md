---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: power-consumption
title: Power consumption & Finch experimentation
---

[TOC]

## Overview

CrOS has defined
[Guardrail metrics](http://go/chromeos-experimentation-metrics#core-metrics-and-guardrails)
which track critical areas of product health, such as Daily Active Users and
crash count. A lesser known Guardrail metric which is often applicable for our
team's features is
[Power.BatteryDischargeRate](https://uma.googleplex.com/p/chrome/variations?sid=6799af3f695a4b2aaa6e755c5c4f7711),
otherwise known as power consumption or battery life. When a Finch-controlled
feature launch rolls out, we'll receive Chirp alerts if there is an effect on
any
[Core or Guardrail metrics](http://go/chromeos-experimentation-metrics#core-metrics-and-guardrails)
(including power consumption). If you suspect your feature will have an effect
on OS performance, it's recommended to follow the procedure below.

<div align="center">
    <img alt="Power consumption UMA" src="/chromium-os/developer-library/guides/experimentation/power-consumption/power_consumption_uma_1.png" width=800>

  Figure 1: The UMA Power Consumption <a href="https://uma.googleplex.com/p/chrome/variations?q=%7B%22overviewTabCohortParams%22%3A%7B%22cohortAnalysis%22%3A%22Recommended%22%7D%2C%22vitalsTabCohortParams%22%3A%7B%22cohortAnalysis%22%3A%22Recommended%22%7D%2C%22compactMode%22%3Atrue%2C%22filters%22%3A%22finch%2CSTGR_FILTER%2C4136685412%7C1255540085%7C3788246804%2Cplatform%2CEQ%2CC%2Cchannel%2CEQ%2C4%22%2C%22graphOptions%22%3A%7B%22dateString%22%3A%22latest%22%2C%22interval%22%3A%227%22%7D%2C%22histogram%22%3A%7B%22Power.BatteryDischargeRate%22%3A%7B%22enablePerClientAggregation%22%3Afalse%2C%22views%22%3A%7B%7D%2C%22clientLevelAnalysisBucket%22%3A%22%22%2C%22aggregationModel%22%3A%7B%22perClientAggregationType%22%3A%22Threshold%22%2C%22threshold%22%3A%22100%22%2C%22percentile%22%3A%2299%22%2C%22selectedBucket%22%3A%220%22%2C%22minimumSamples%22%3A%221%22%7D%2C%22cohortParams%22%3A%7B%22setting%22%3A%22Recommended%22%2C%22analysisType%22%3A0%7D%7D%7D%2C%22showObsoleteHistograms%22%3Afalse%2C%22showExpiredHistograms%22%3Afalse%2C%22seeCounts%22%3Afalse%2C%22stability%22%3A%7B%7D%2C%22tab%22%3A%22%23histogram-tab%22%2C%22enablePreperiodCorrectionIfAvailable%22%3Atrue%7D">histogram</a> for a Finch experiment.
</div>

Feature launches and A/B experiments are distinct processes, with different
requirements. Both the Fast Pair and Nearby Share Background Scanning features
used as examples throughout this document wanted a feature launch *and* a power
consumption experiment. For that reason, the Finch rollout process was
**shared** for the A/B experiment and the feature launch; i.e. the GCL config we
wrote set up an A/B experiment as a piece of launch criteria *and* launched the
feature to Stable. This is not required for A/B experiments.

### Experimentation Plan

The
[ChromeOS Data team](https://g3doc.corp.google.com/company/teams/chromeos-data/CrOSExperimentation.md)
provides support for ChromeOS developers who want A/B experimentation. The steps
below are based on the Data team's [process](http://go/cros-exp-process).

1.  Make a copy of [go/cros-exp-launch-report](http://goto.google.com/cros-exp-launch-report), and fill out the Pre-Consultation
    section.
2.  Optional: Set up an [Office Hours](http://go/cros-exp-oh) appointment with a
    Data team analyst.
3.  Set up the Finch experiment and analyze the results with your analyst as the
    experiment progresses.

References

-   [Fast Pair Power Consumption analysis doc](https://docs.google.com/document/d/1-FZiDcfO4O_Yw8kYCeBiGJCz3MDfxJ9nE5g_Si9waEg/edit?resourcekey=0-ZfqbSr7sL5zYQHDdh_7yxg#heading=h.oec101c7k279)
-   [Nearby Share Background Scanning Power Consumption analysis doc](https://docs.google.com/document/d/1C4TCQJuDdl7K1VDF7SjK5DcFdi9Zr_-RNZrwvuTA6fY/edit#heading=h.oec101c7k279)

### Setting up the Finch experiment

Read through [go/finch101](http://goto.google.com/finch101) for a more in depth walkthrough of Finch best
practices.

There are several steps common to all Finch launches, which can be found at
[go/finch-best-practices.](http://goto.google.com/finch-best-practices.) An abridged list of these steps is below:

1.  Implement the feature behind a new `DISABLED_BY_DEFAULT`
    [base::Feature](http://go/finch-feature-api) flag.
2.  Create a new Finch
    [config](https://g3doc.corp.google.com/analysis/uma/g3doc/finch/cookbook.md)
    for the experiment. The new feature will be `enabled` in half the
    population, and `disabled` in the other half of the population (hence an A/B
    experiment).
3.  Create a [field trial testing config](http://go/field-trial-testing-config)
    for experiments that plan to go to Beta or Stable.
4.  Slowly rollout the experiment on Canary, Dev, Beta, and then Stable. It is
    recommended to have a very fine grained Stable rollout, such as from 1%
    Stable to 10% Stable to 50% Stable, rather than the usual 1% to 100% Stable
    used in Finch-controlled [feature launches](http://go/finch-best-practices).

Tip: g/finch-users is a great resource for tricky questions about the GCL
config, such as holdback groups, phased rollouts, and any PRESUBMIT errors.

For power consumption, you want one of the key histograms evaluating performance
to be `'Power.BatteryDischargeRate'`. You can optionally add other key
histograms if you're trying run another A/B experiment with your launch (e.g.,
success rate)." See examples of these GCL configs below. Rollouts are performed
by modifying the existing GCL config to enable the feature on a larger portion
of the population.

References

-   [Fast Pair Power Consumption GCL #1 (Canary and Dev)](https://source.corp.google.com/piper///depot/google3/googledata/googleclient/chrome/finch/gcl_studies/chrome_os/FastPairPowerConsumption.gcl;bpv=0;bpt=0;rcl=427800103)
-   [Fast Pair Power Consumption GCL #2 (Canary, Dev, and Beta)](https://source.corp.google.com/piper///depot/google3/googledata/googleclient/chrome/finch/gcl_studies/chrome_os/FastPairPowerConsumption.gcl;bpv=1;bpt=0;rcl=445988024)
-   [Fast Pair Power Consumption GCL #3 (Canary, Dev, Beta, and 1% Stable)](https://source.corp.google.com/piper///depot/google3/googledata/googleclient/chrome/finch/gcl_studies/chrome_os/FastPairPowerConsumption.gcl;bpv=0;bpt=0;rcl=501297907)
-   [Fast Pair Power Consumption GCL #4 (Canary, Dev, Beta, and 10% Stable)](https://source.corp.google.com/piper///depot/google3/googledata/googleclient/chrome/finch/gcl_studies/chrome_os/FastPairPowerConsumption.gcl;bpv=1;bpt=0;rcl=514758593)
-   [Fast Pair Power Consumption GCL #5 (Canary, Dev, Beta, and 50% Stable)](https://source.corp.google.com/piper///depot/google3/googledata/googleclient/chrome/finch/gcl_studies/chrome_os/FastPairPowerConsumption.gcl;bpv=1;bpt=0;rcl=520721954)
-   [Fast Pair Power Consumption GCL #6 (launched)](http://google3/googledata/googleclient/chrome/finch/gcl_studies/chrome_os/FastPairPowerConsumption.gcl;rcl=528583637)
-   [Nearby Share Background Scanning Power Consumption GCL](https://source.corp.google.com/piper///depot/google3/googledata/googleclient/chrome/finch/gcl_studies/chrome_os/NearbyShareBackgroundScanningPower.gcl)

### Evaluating experiments with UMA

You should keep a close watch on your experiment's metrics, especially with how
it affects ChromeOS Guardrail metrics, including power consumption. This is
done using UMA's
"[Finch](https://uma.googleplex.com/p/chrome/variations?sid=6799af3f695a4b2aaa6e755c5c4f7711)"
tab. **These metrics should be evaluated before every stage of the experiment
rollout**. If you're unsure of what to make of the metrics, reach out to your
advisor at [cros-exp-help@google.com](http://g/cros-exp-help).

#### Example: Fast Pair power consumption A/B study

Here's a snapshot of our Fast Pair power consumption A/B
[experiment](https://uma.googleplex.com/p/chrome/variations?sid=cfa58ef370552483922d6b1ec791417d)
taken on Dec 27, 2022.

<div align="center">
  <img alt="Power consumption Fast Pair" src="/chromium-os/developer-library/guides/experimentation/power-consumption/power_consumption_uma_2.png" width=800>

  Figure 2: The UMA Power Consumption <a href="https://uma.googleplex.com/p/chrome/variations?sid=cfa58ef370552483922d6b1ec791417d">histogram</a> for Fast Pair on Dec 27, 2022.
</div>

There's a couple of things to notice in this histogram:

1.  The power consumption increased by about ~0.8%, which was within the bounds
    set in our
    [launch criteria](https://docs.google.com/document/d/1-FZiDcfO4O_Yw8kYCeBiGJCz3MDfxJ9nE5g_Si9waEg/edit?resourcekey=0-ZfqbSr7sL5zYQHDdh_7yxg#heading=h.kh80ii6glxlu)
    (3.3% or less).
2.  The p-value of our "increase" is very low (gray signifies weak confidence,
    and 2+ diamonds indicate some confidence). In general, the higher the
    p-value, the higher confidence you have that your experiment is having an
    effect on your chosen metric.
3.  The confidence interval (CI) at the 50th percentile was "95% CI [+0.11,
    +1.62]%". A narrower CI (like this one with a range of 1.51%) provides more
    confidence in the value of the median delta (+0.86%).
4.  Since the percent impact on power consumption was within bounds and our
    confidence was low, our analyst agreed with our decision to increase the
    rollout to 1% stable. Larger populations generally increase the p-value and
    confidence in experiments, if a correlation exists.

Note: You can consult the analyst at any point of the rollout to go over these
histograms. You **must** consult the analyst before ramping up to Beta and
Stable channel.

### Clean-up

As mentioned earlier, the example studies used in this document are *launched
features*, so the end result is the same as a Finch feature rollout.
Specifically, that means we did the following after evaluating the last Stable
experiment group:

1.  Enable the feature by default on ToT
2.  Modify the GCL config to be
    [LAUNCHED](http://google3/googledata/googleclient/chrome/finch/gcl_studies/chrome_os/NearbyShareBackgroundScanningPower.gcl;l=38;rcl=532605694),
    with the max version being the last version that had your feature flag
    disabled by default.

Alternatively, your A/B experiment might end at Beta or even earlier. To end
your experiment, simply
[delete the GCL config](http://go/finch-best-practices-g3doc#stopping-an-experiment).
**Launched features** should not delete the GCL config.

### Notes

For Fast Pair and Nearby Share Background Scanning, having a Finch experiment
helped us confirm our hypothesis (and launch criteria) that there was not a
large effect on the battery life. This allowed us to make data-driven decisions
about going to launch.

In addition to an A/B Finch experiment, which is recommended, you can also try a
local power consumption experiment. This helped us in the past when
hypothesizing the impact of
[Nearby Share Background Scanning](https://docs.google.com/document/d/1YLwmEpWw84ryH4md0eBf0ny5jeyKRbOq-kSrOYJmMtU/edit#heading=h.r5jblgdg0a17),
however we eventually did use an A/B Finch experiment to confirm the local
results.