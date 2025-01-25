---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: how-to-create-metrics
title: How to Create Metrics?
---

[TOC]

## Why?

We want to collect data from users to track user actions, and help offer better
user experience. This document provides instructions on how to use histogram in
code and how to document the histograms for the dashboard.

## Solution

#### Step 1: Define Useful Metrics.

Measure exactly what you want and think ahead

*   Why do we want this metric?
*   When will the metric emitted?
*   What value does the metric add on?
*   What can be improved after seeing the graph in the dashboard?

##### For example, [Feedback.ChromeOSApp.ExitPath](https://crsrc.org/c/tools/metrics/histograms/metadata/others/histograms.xml;l=5756?q=Feedback.ChromeOSApp.exitpath)

*   Why do we want this metric?

    *   Track how the user exited from the feedback app

*   When will the metric emitted?

    *   Fires when user closes the feedback app

*   What value does the metric add on?

    *   Determine rate of submit vs cancel
    *   Understand where users dropped off in flow

*   What can be improved after seeing the data?

    *   If *kQuitNoHelpContentClicked* is high -> This may indicate help content
        is not useful, therefore should increase the help content article
        quality.
    *   If *kQuitNoResultFound*(Offline or Search is down) is high -> Improve
        search reliability, remind user to be online when submitting feedback
        etc.

Step 2: Create Metrics (`Feedback.ChromeOS.ExitPath` as example.)

*   Picking your histogram type, common types are:

    *   Enum Histogram
    *   Boolean Histogram
    *   Time Histogram

*   Add your histogram in histograms.xml e.g.

    *   [Enum Histogram](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/others/histograms.xml;l=5756?q=Feedback.ChromeOSApp.exitpath)
    *   [Boolean Histogram](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/others/histograms.xml;l=5756?q=Feedback.ChromeOSApp.exitpath)
    *   [Time Histogram](https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/others/histograms.xml;l=5874?q=Feedback.ChromeOSApp.exitpath)

e.g. `Feedback.ChromeOS.ExitPath` will be an Enum Histogram

```
<histogram name="Feedback.ChromeOSApp.ExitPath" enum="FeedbackAppExitPath"
    expires_after="2023-07-27">
  <owner>longbowei@google.com</owner>
  <owner>xiangdongkong@google.com</owner>
  <owner>cros-device-enablement@google.com</owner>
  <summary>
    Records a summary of the actions the user took before exiting the app. Fires
    when user closes the feedback app, case includes: User quits on search page
    after clicking some help contents; User quits on search page without
    clicking some help contents; User closes on search page because no help
    content is shown(Offline or Search is down); User closes on share data page
    after clicking help content; User closes on share data page without clicking
    help content; User submits feedback after clicking the help content; User
    submits feedback without clicking the help content.
  </summary>
</histogram>
```

*   For Enum Type, please also add a enum in
    `tools/metrics/histograms/enums.xml`

```
<enum name="FeedbackAppExitPath">
  <int value="0" label="kQuitSearchPageHelpContentClicked"/>
  <int value="1" label="kQuitSearchPageNoHelpContentClicked"/>
  <int value="2" label="kQuitNoResultFound"/>
  <int value="3" label="kQuitShareDataPageHelpContentClicked"/>
  <int value="4" label="kQuitShareDataPageNoHelpContentClicked"/>
  <int value="5" label="kSuccessHelpContentClicked"/>
  <int value="6" label="kSuccessNoHelpContentClicked"/>
</enum>
```

*   Add EmitMetricFunc() to histogram_util.h, histogram_util.cc

    *   histogram_util.h

        ```
         void EmitFeedbackAppExitPath(mojom::FeedbackAppExitPath exit_path);
        ```

    *   histogram_util.cc

        ```
        void EmitFeedbackAppExitPath(mojom::FeedbackAppExitPath exit_path) {
            base::UmaHistogramEnumeration(kFeedbackAppExitPath, exit_path);
        }
        ```

    *   [ash/webui/os_feedback_ui/backend/histogram_util.h](https://crsrc.org/c/ash/webui/os_feedback_ui/backend/histogram_util.h)
        [ash/webui/os_feedback_ui/backend/histogram_util.cc](https://crsrc.org/c/ash/webui/os_feedback_ui/backend/histogram_util.cc)

*   Import and use function to emitMetricFunc().

    *   **Only Backend** - Simply Use metrics::EmitMetricFunc().

        ```
         os_feedback_ui::metrics::EmitFeedbackAppOpenDuration(app_open_duration);
        ```

        *   <https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/os_feedback_ui/backend/feedback_service_provider.cc;l=62?q=ash%2Fwebui%2Fos_feedback_ui%2Fbackend%2Ffeedback_service_provider.cc>
        *   CL: <https://crrev.com/c/3782183>

    *   **Involve Frontend** - Add function in mojom and call the cuntion

        *   Define Enum in mojom:

        ```
        // User exit paths.
        enum FeedbackAppExitPath {
            // User quit on search page after clicking some help contents.
            kQuitSearchPageHelpContentClicked,
            // User quit on search page without clicking some help contents.
            kQuitSearchPageNoHelpContentClicked,
            // User close on search page because no help content is shown (Offline or
            // Search is down).
            kQuitNoResultFound,
            // User close on share data page after clicking some help contents.
            kQuitShareDataPageHelpContentClicked,
            // User close on share data page without clicking some help contents.
            kQuitShareDataPageNoHelpContentClicked,
            // User submit feedback and click the help content.
            kSuccessHelpContentClicked,
            // User submit feedback without clicking the help content.
            kSuccessNoHelpContentClicked
        };
        ```

        <https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom;l=170?q=ash%2Fwebui%2Fos_feedback_ui%2Fmojom%2Fos_feedback_ui.mojom>

        *   Add EmitMetricFunc() in mojom

        ```
        // Record metrics of users' exit paths of feedback app.
        RecordExitPath(FeedbackAppExitPath exit_path);
        ```

        <https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom;l=233-234?q=ash%2Fwebui%2Fos_feedback_ui%2Fmojom%2Fos_feedback_ui.mojom>

        *   Use mojom function in polymer.js

        ```
        /**
        * @param {!FeedbackAppExitPath} pathHelpContentClicked
        * @param {!FeedbackAppExitPath} pathNoHelpContentClicked
        * @private
        */
        recordExitPath_(pathHelpContentClicked, pathNoHelpContentClicked) {
            this.helpContentClicked_ ?
                this.feedbackServiceProvider_.recordExitPath(pathHelpContentClicked) :
                this.feedbackServiceProvider_.recordExitPath(pathNoHelpContentClicked);
        }
        ```

        <https://source.chromium.org/chromium/chromium/src/+/main:ash/webui/os_feedback_ui/resources/feedback_flow.js;l=402-406?q=ash%2Fwebui%2Fos_feedback_ui%2Fresources%2Ffeedback_flow.js&ss=chromium%2Fchromium%2Fsrc>

    *   Final step: Remember to go to `chrome://histograms/<METRICS_NAME>` to
        check the metrics are emitted as expected.

--------------------------------------------------------------------------------

#### Example CLs

*   3811341: Feedback: Metrics to record user's exit path |
    <https://chromium-review.googlesource.com/c/chromium/src/+/3811341>
*   3829034: Feedback: Metrics to track help content search result count. |
    <https://chromium-review.googlesource.com/c/chromium/src/+/3829034>
*   3792916: Feedback: Add metrics to record number of clicks on viewScreenshot
    | <https://chromium-review.googlesource.com/c/chromium/src/+/3792916>
*   3782183: Feedback: Introduce metric to capture open duration of feedbackApp
    | <https://chromium-review.googlesource.com/c/chromium/src/+/3782183>
*   3827808: Feedback: Metrics to track the outcome of viewing helpcontent |
    <https://chromium-review.googlesource.com/c/chromium/src/+/3827808>
