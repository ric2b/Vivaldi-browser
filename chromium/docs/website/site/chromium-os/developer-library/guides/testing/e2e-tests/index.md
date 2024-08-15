---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: e2e_tests
title: Running and writing "Tast" automated end to end (E2E) tests
---

[TOC]

## Note

This page's primary audience is for Googlers, since Tast E2E
automation is a common requirement of ChromeOS feature development.

## Prerequisites

### Checkout and set up ChromeOS repo.

If you only work on Chromium and haven’t checked out the ChromiumOS repo, do so
now with instructions at [Getting Started](/chromium-os/developer-library/getting-started).

If you already have ChromeOS set up but haven't touched it in a while, it's
probably a good idea to run `repo sync` and run `cros build-packages
--board=${BOARD}`; the latter will also call `update_chroot` for you.

### Set up DUT with correct version of Chrome and ChromeOS

If you are writing integration tests for a new or relatively new feature, make
sure your test device has the binary with that feature included.

For new Chrome or ChromeOS features, follow instructions at
[Getting Started](/chromium-os/developer-library/getting-started).

## Running Tast tests

Running a Tast test is pretty simple. From your workstation (desktop or
cloudtop), navigate to ChromiumOS repo and start `cros_sdk`. From within your
shell, run:

```shell
(cr)$ tast --verbose run ${DUT} ${TEST_NAME}
```

This will build, deploy, and run the specified integration test on your DUT.

Tip: The name of a test follows the format of `<GO PACKAGE>.<TEST
NAME>.<PARAMETER SUFFIX>`. The `.<PARAMETER SUFFIX>` suffix is only applicable
for
[parameterized tests](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/writing_tests.md#parameterized-tests).
If a test requires a parameter suffix but it's not provided, the test will not
be found. Additionally, you can use the wildcard operator to specify multiple
tests, e.g. `bluetooth.*`.

DUT is short for Device Under Test. Replace ${DUT} with the ip address where
your DUT can be reached. ex) localhost:7777. Test name is the test that you want
to run. It is the combination of package name and function name. For example,
the test name for the test in
[this file](https://source.chromium.org/chromiumos/chromiumos/codesearch/+/main:src/platform/tast-tests/src/chromiumos/tast/local/bundles/cros/diagnostics/launch_diagnostics_from_launcher.go?q=launch_diagnostics)
is “diagnostics.LaunchDiagnosticsFromLauncher”.

Shortly after executing the command, Tast should remotely control your test
device to run the integration test. Your device under test for
"diagnostics.LaunchDiagnosticsFromLauncher" should look like
[this](https://drive.google.com/file/d/1gY0-eP-5VNgr3I1OiPYYrb31fzyxWXB7/view).

### Running a test multiple times

Running a Tast test multiple times can be helpful when trying to debug flaky
failures, or to provide more confidence that your test/changes aren't flaky
themselves. While this would be painful to do with `tast run ...`, we luckily
have a script accessible to us called `repeat-test.sh` that allows us to run a
test an arbitrary number of times using the `-r #` argument. For example,
executing the following command in your shell will run the
`policy.AllowDinosaurEasterEggEnrolled` test `25` times:

```shell
(cr)$ ~/chromiumos/src/platform/tast-tests/tools/repeat-test.sh \
    -r 25 -- 127.0.0.1:2200 policy.AllowDinosaurEasterEggEnrolled
```

### Cross-device tests

Cross-device tests require some additional setup. Make sure that your DUT has a
"Test" build from go/goldeneye, and follow the instructions in
go/run-nearby-tast-tests and go/multi-dut-android-setup to set up your phone.
The phone will need to be connected to the DUT over USB.

#### Cleanup after cross-device tests

If you would like to remove an owner account from your devices added during a
test, run this command from the VT2 terminal:

```shell
$ crossystem clear_tpm_owner_request=1
```

Then, reboot:

```shell
$ reboot
```

## Writing Tast tests

Refer to go/tast-writing for generic information on writing Tast tests.

Tast code is located in the tast-tests git repo. Navigate to
`~/trunk/src/platform/tast-tests` inside the chroot or
`~/chromiumos/src/platform/tast-tests` outside the chroot. Be sure to run any
git commands inside or below this directory.

To add a new test, create a new file in the test directory under the appropriate
package. Under the init() function fill out the description of the test, and add
yourself to the contact information array.

As described in go/tast-writing, be sure to add "group:mainline" and
“informational” string to the attribute array as well. Informational attribute
marks the integration test non-blocking. It’s standard practice to let your
integration test run without a flake for 20 consecutive runs, after which you
can remove the informational attribute tag to promote the test to blocking. You
can monitor flakey-ness of your test at [tastboard](http://tastboard/test).

Other useful documentation worth reviewing include:

*   go/tast-test-intro

### Parameterized Tests

If you need to run multiple Tast tests from a single file, but change specific
variables or metadata/fixture setup between runs, you can parameterize the test.
There are two different methods of parameterization: table-driven tests, and
test parameters.

1.  Table-Driven Tests

    If you need to test multiple scenarios with very slight differences, you
    should use a table-driven test. **Table driven tests are the most common
    pattern**.

    ```go
    for _, tc := range []struct {
        format   string
        filename string
        duration time.Duration
    }{
        {
            format:   "VP8",
            filename: "sample.vp8",
            duration: 3 * time.Second,
        },
        {
            format:   "VP9",
            filename: "sample.vp9",
            duration: 3 * time.Second,
        },
        {
            format:   "H.264",
            filename: "sample.h264",
            duration: 5 * time.Second,
        },
    } {
        s.Run(ctx, tc.format, func(ctx context.Context, s *testing.State) {
            if err := testPlayback(ctx, tc.filename, tc.duration); err != nil {
                s.Error("Playback test failed: ", err)
            }
        })
    }
    ```

    Read more about table-driven tests in the
    [table-driven test documentation](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/writing_tests.md#Table_driven-tests).

2.  Test Parameters

    Table-driven tests are almost always preferred, unless...

    1. Tests should have different attributes: only some tests need an extra attribute, which cannot be facilitated with table-driven tests, as all fields must be defined.

    2. Tests should declare different dependencies.

    3. Test results should be reported separately: defining tests as parameters reports metrics separately. In which case, adding new test parameters may be preferable.

    Add `Params` in the config

    ```go
    Params: []testing.Param{

    {
        Name:      "vp8",
        Val:       "sample.vp8",
        ExtraData: []string{"sample.vp8"},
        ExtraAttr: []string{"informational"},
    }

    {
        Name:      "vp9",
        Val:       "sample.vp9",
        ExtraData: []string{"sample.vp9"},
        // ExtraAttr is excluded here.
    }

    {
        Name:              "h264",
        Val:               "sample.h264",
        ExtraSoftwareDeps: []string{"chrome_internal"}, // H.264 codec is unavailable on ChromiumOS
        ExtraData:         []string{"sample.h264"}, // Additional data is included here.
        ExtraAttr:         []string{"informational"},
    }},
    ```

    `Val` is an arbitrary value that can be accessed in the test body via
    `state.Parameter.Param`. It must be type-asserted immediately. For example:

    ```go
    filename := state.Param().(string)
    ```

    `Extra*`, like `ExtraAttr`, can also be accessed via `testing.Param`. Read
    about the customizable properties of `testing.Param`
    [here](https://pkg.go.dev/chromium.googlesource.com/chromiumos/platform/tast.git/src/go.chromium.org/tast/core/testing#Param).

    TIP: Read more about test parameters in the
    [parameterized test documentation](https://chromium.googlesource.com/chromiumos/platform/tast/+/HEAD/docs/writing_tests.md#Table_driven-tests).

### Adding a Lacros test

[Lacros is an ongoing effort](https://sites.google.com/corp/google.com/lacros/home)
to separate the Chrome browser from the system UI in ChromeOS. This may require
an additional test variant to ensure functionality works as expected when Lacros
is enabled. We can paramatize existing tests to add a Lacros variant.

1.  Enable Lacros through a variant:

    ```go
    import (
        "chromiumos/tast/local/chrome/browser"
        "chromiumos/tast/local/chrome/browser/browserfixt"
    )

    Params: []testing.Param{

    {
        Val:     browser.TypeAsh,
        Fixture: "chromeLoggedIn",
    }

    {
        Name:              "lacros",
        Val:               browser.TypeLacros,
        Fixture:           "lacros",
        ExtraSoftwareDeps: []string{"lacros"},
    }}
    ```

2.  Setup and open the browser

    If your test requires the use of the Chrome browser, Lacros enabled variants
    of the test should open an instance of Chrome before attempting to interact
    with the browser.

    ```go
    br, closeBrowser, err := browserfixt.SetUp(ctx, s.FixtValue().(chrome.HasChrome).Chrome(), s.Param().(browser.Type))
    if err != nil {
        s.Fatal("Failed to set up browser: ", err)
    }
    defer closeBrowser(cleanupCtx)
    ```

TIP: Read more about Lacros tast test porting at go/lacros-tast-porting.

## Tips for working with CLs in ChromiumOS

-   **Every commit is a CL** \
    Unlike in Chromium where a CL is represented by a branch, each commit will
    be treated as a CL when you upload using `repo upload`.
-   **Change-Ids are generated when you run `git commit`** \
    Unlike in Chromium where the Change-Id is added to the commit message when
    you upload the CL, in ChromiumOS the Change-Id is generated locally by a git
    hook when you commit. Take care not to delete the Change-Id!

    If you need to reupload the CL to make changes, then you have to update the
    original commit using `git commit --amend` or by using `git rebase -i`, etc.
    So long as the Change-Id of the original commit remains intact, it will be
    associated with the correct review in Gerrit.

-   **Avoid editing your commit messages in Gerrit** \
    Whenever you upload your changes, your local commit message will overwrite
    what's in Gerrit, so it's probably best to avoid editing your commit
    messages in Gerrit altogether. Instead, use `git commit --amend` to update
    the commit message and upload the CL again.

-   **Commit messages have to be formatted differently**

    ```
    [SmartLock] Adapt tests to use revamp feature

    The new Smart Lock UI introduced by the SmartLockUIRevamp flag doesn't
    quite work with the existing Tast tests. Update the tests to correctly
    select the new UI elements, and enable the flag in the test fixtures.

    BUG=b:223435238
    TEST=tast run 'smartlock.*'

    Change-Id: I8fb62aecbd880476065bf61c3d4002e6a96a5290
    ```

    -   The Change-Id has to be in a paragraph of its own at the end.
    -   BUG (or FIXED) and TEST must be included in the previous paragraph, and
        must use the = syntax. Buganizer bugs have to use the : syntax.

-   **"Verified" has to have +1 before the CL can be submitted** \
    You're allowed to set Verified +1 if you've verified the change. Reviewers
    generally treat Verified +1 as a signal that the change is ready for review.
    ![Verified submit requirement](/chrome/chromeos/system_services_team/dev_instructions/g3doc/images/verified_submit_requirement.png){style="display:block;margin:auto;width:800px"}

## Links

-   Setting up ChromiumOS: go/chromeos-building
-   Extra info on submitting CLs to ChromiumOS: go/cros-contributing
-   Getting cross-device tests running:
    -   go/run-nearby-tast-tests
    -   go/multi-dut-android-setup
-   Cheatsheet for Cellular tests: go/cellular-ui-tast-cheatsheet
-   Tast reference:
    -   go/tast-test-intro
    -   go/tast-quickstart
    -   go/tast-running
    -   go/tast-writing
    -   go/tast-overview
    -   go/cros-connectivity-servo
