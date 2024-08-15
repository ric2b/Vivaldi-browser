---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: code-reviews-and-submitting-code
title: Submitting code best practices

---

[TOC]

## Before You Begin

### Looping In Your Reviewers
Before you start coding, select a lead reviewer (likely this will be your TL)
and run your plan by them. This will prime them for your review, as well as
surface any major concerns early on.

### Draft a Design Doc (If Applicable)
If your CL will take more than 2-3 days to implement, create at least a
one-pager that describes your approach. The larger the CL or set of CL's, the
greater the need for a design doc. At least your TL should review, but it's also
a great idea to socialize with your immediate team and/or
wider team (either via email or presenting in weekly
tech demos) depending on how large/complete the problem is.

How to write a design:

 - Use template go/cros-sw-designdoc and fill out all sections (even if just to
   mark them as N/A with an explanation).
 - Engage reviewers early to take first passes as you iterate on your design.
   This includes presenting weekly demos to the team.
 - Use doc-wrap to finalize the reviewers and send an email to the wider team
   for any extra pairs of eyes.

### Familiarize Yourself with Coding Best Practices

Review style guides outlined in [Writing Code](https://www.chromium.org/chromium-os/developer-library/reference/cpp/intro/) -- this is
more of an ongoing process in order to review these resources and improving your
skills.

## Testing
Testing is an important part of the coding process to ensure code
we add doesn't break anything, and to future-proof our code from
others breaking it. Manual testing is to make sure that code
we add/change can behave well with a DUT and does what we
expect it to do. Unit tests are to test all edge cases for each
file we add in and to protect our code from unexpected changes
in the future.

### Manual tests
Before your CL is ready for review, you need to manually test your changes on
your DUT. Describe your manual testing in your commit message under `Tested:`.
For example,

```
Tested: verified that subsequent, guest, and initial discovery
notifications and associate account notifications auto dismiss after
12 seconds on DUT using JBL 225TWS
```

### Unit tests
Every implementation file in your CL is expected to have corresponding unit test
file. If a file does not need unit tests, make sure to include why in
the commit message.

```
Low coverage reason: actions.cc is not used fully in unit tests,
however this is a minor file with enums that are triggered in manual testing
and are not essential to implementation. Because it's an action enum, testing
one enum tests all enums.
```

Every unit test is expected to contribute to a file's >90% test coverage. Before
you add reviewers to your CL, run `CQ Dry Run` on Gerrit, and once completed,
there will be a summary of code coverage.
![Example of Gerrit code coverage summary showing the file's test coverage](https://screenshot.googleplex.com/BZ9YWQCi7hoi6LV.png)

In addition, within the implementation file, you are able to see which lines are
missing unit test coverage highlighted in orange. Add unit tests that trigger
these lines of code to increase code coverage of a file.
![Example of Gerrit missing code coverage lines highlighted in orange](https://screenshot.googleplex.com/7YQ8PoqmidFGjZ4.png)

Note: If you are adding metrics, you do not need to add a unit test for
coverage; unit tests have historically not caught bugs in UMA metric collection
implementations, because of complex interactions in the systems that emit
metrics. Using `chrome://histograms` for manual testing and the UMA dashboard
for canary/dev channel is a more meaningful integration test that can find bugs
in metric collection implementations. However, metrics can be useful in unit
tests to test isolated code paths where a metric emitted is the best way to
ensure that is is working as expected.

If you are not adding a unit test for a metric, add the following `Tested:` bit.
For example,
```
Tested:Verified on chrome://histograms that the metrics in this CL are logged
when the xyz event happens.
```

## Choosing Code Reviewers

### Who to include as a reviewer
Make sure you always add your TL as a reviewer for the CLs. If you don't have a
TL related to your CL, at least someone from your team needs to be added to your
CL. If external reviewers are needed for your CL, please ask someone from your
team to review and +1 your CL before sending your CL out for review by the
external team, and save the external reviewer time by specifying precisely which
files you need them to review.

### Local Owners
Add the most local code owners for the files you are changing, not the owners
with most coverage. Local owners will have the most context for the changes that
you are making. Beware, Gerrit will prompt you to pick the ones with the most
coverage, and discourage too many code reviewers.

For example, if you wanted to change a file for the Bluetooth Settings page
(`chrome/browser/resources/settings/chromeos/os_bluetooth_page/`) anyone that is
an OWNER of `chrome`, `chrome/browser`, `chrome/browser/resources`,
`chrome/browser/resources/settings`, or
`chrome/browser/resources/settings/chromeos` will be able to review your code.
However, they may not have the most context. Like mentioned above, Gerrit may
not prompt you with the most local OWNERS. To find the most local OWNERS, go to
the closest level directory with an OWNERS file and add one of these OWNERS to
your CL.

![Example of OWNERS local in the Bluetooth directory](https://screenshot.googleplex.com/5gRbAB9N48NVPX9.png)

## Submitting Code Flow
1.  Upload code (`git cl upload`).
2.  Run `CQ Dry Run` and optionally include other debug try bots.
3.  Code-Review: choose reviewers from OWNERS files to review code. Gerrit also
    offers a “Find Owners” button.
4.  Address reviewer comments, upload changes.
5.  Once all reviewers have approved, click “SUBMIT TO CQ” (CQ: commit-queue) at
    the top of the page under the header to submit.


## Trybots
As mentioned above, running `CQ Dry Run` will provide a summary of code
coverage. More importantly, `CQ Dry Run` will run the integration tests that are
required to submit code. Run `CQ Dry Run` before you request a review and make
sure all tests pass; if not, resolve all errors before requesting a review.

By default, any Chromium CL submitted via the Commit Queue must pass all tests
for release builds. Tests on debug builds are skipped on the CQ because (1) they
take a very long time to run and would slow down CL submission and (2) usually
they will produce the same pass/fail result as release builds.

However, in some cases (e.g., debugging memory issues or working on WebUI code
with `optimize_webui = false`), it may be valuable to run your code through
debug trybots. These are the trybots worth testing on:

*   linux-chromeos-dbg
*   win_chromium_dbg_ng
*   linux_chromium_dbg_ng
*   mac_chromium_dbg_ng

To run these tests on your CL, you have two options:

1.  On the command line, run:

    ```shell
    $ git cl try -B chromium/try \
               -b win_chromium_dbg_ng \
               -b linux_chromium_dbg_ng \
               -b mac_chromium_dbg_ng \
               -b linux-chromeos-dbg
    ```

2.  Go to the Gerrit UI and click "Choose Tryjobs", then add the trybots you'd
    like to test.

#### Getting Help

1. When a tryjob fails for the reasons that are unrelated to the change you are
   trying to submit, open a bug (there is a "File bug" link in failure results).

2. If you need help from CrOS sheriffs, contact them via Chat at
   go/cros-oncall-weekly (you can also auto-join weekly chats - see
   go/cros-oncall-chat-tool#auto-joining-gocros-oncall-weekly).

3. Chrome sheriffs are on Slack -- channels #sheriffing and #ops.

## Bug fix iteration and verification

Some additional advice applies when fixing bugs:

### Write regression tests when fixing regressions

Try to reproduce the bug in a unit test before you implement a fix. Then you can
verify that your fix actually fixes the original bug via unit testing. This is
called Test Driven Development (TDD): a style of code development in which you
first write the test, then write the code to be tested. To learn more about TDD,
check out
[C++ Unit Test Code Lab](https://g3doc.corp.google.com/codelab/unittesting/cpp/g3doc/index.md#test-driven-development)
and
[this presentation](https://docs.google.com/presentation/d/1L3QAGYo1nbZbumLeEk2he5B2XhCclA0vverkaODXPkM/edit?usp=sharing)
which discuss the importance of TDD.

In addition, in the CL that fixes the bug, please add a comment above your unit
test as follows: “// Regression test for b/xxxxx”. For example,

```cpp
// Regression test for b/261041950.
TEST_F(RetroactivePairingDetectorTest,
      FastPairPairingEventCalledDuringBluetoothAdapterPairingEvent) {
…
}
```

Note: see [Debugging tips and tricks](https://www.chromium.org/chromium-os/developer-library/guides/debugging/debugging/) to learn some methods
available to debug a regression.

### Don’t close bugs whose outcome is uncertain

When you fix a bug, ask yourself if there is some method to quantify/verify your
fix when it hits public, and create a plan to check that method if so.

The Smart Lock team provides the following example of verifying your fix, well
after your fix lands:

> The Smart Lock team landed a super impactful fix, b/216829464, that
> dramatically improved feature reliability, but the bug was just marked closed
> as soon as the CL landed -- so when we got a huge positive metrics spike a
> couple months later (b/237892911) we didn't know why, and spent a ton of time
> tracking down the original fix.

> In this case, we could have used bugjuggler (see go/bugjuggler) to track
> follow up verification on the bug fix. In this case, we could have set a
> bugjuggler reminder on b/216829464 for 2/4/8 weeks to check if the expected
> metrics improved in dev/beta/stable channel. Your particular bug might require
> verifying that crash stats have fallen, etc., and bugjuggler is the best way
> to circle back and verify.
