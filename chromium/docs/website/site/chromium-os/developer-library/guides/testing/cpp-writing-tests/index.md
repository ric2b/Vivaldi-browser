---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: cpp-writing-tests
title: Writing Tests in C++
---

[TOC]

## Unit Tests vs Integration Tests (Browser Tests, End-to-End Tests)

It's important to be aware of the purpose of each kind of test.

Unit tests are meant to test individual components ("units", typically classes).
When a unit test fails, it should direct you to the exact component that needs
fixing. Unit tests should be very fast to run, since they require a only a
minimal test environment.

Integration tests are meant to test interactions between components. They tend
to be larger, more complicated tests, and are slower to run. Chrome has several
kinds of integration tests, most importantly browser tests and end-to-end (E2E)
tests.

[Browser tests](https://chromium.googlesource.com/website/+/HEAD/site/developers/testing/browser-tests/index.md),
unlike unit tests, run inside a browser process instance and are attached to a
window for rendering. These are most often used for testing UIs, but have other
uses as well.

[E2E tests](/chromium-os/developer-library/guides/testing/e2e-tests) are automated tests performed on actual Chromebooks. A
complete ChromeOS build is flashed to the hardware and a test framework called
Tast is used to run tests against the device. Multiple devices, including both
Chromebooks and Android phones, can be tested in tandem, allowing for full tests
of Bluetooth interactions, for example. These tests are by far the slowest and
flakiest kind of test, but they can detect a wide array of problems that would
not show up in other kinds of tests.

This just scratches the surface. Check out these resources for even more kinds
of tests:

- [Noogler Book - Googler only](https://engdoc.corp.google.com/eng/production/docs/noogler-book/g3doc/testing.md?cl=head)
- [Chromium Docs](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/testing_in_chromium.md)

## Tips for writing good unit tests

### Test individual components in isolation

When testing a complicated component with many dependencies, it can be tempting
to use real components to satisfy the dependencies. This tends to produce lower
quality quasi-integration tests. You're no longer testing just the component
under test, but also its dependencies. When a breaking change occurs, many tests
may fail at once, and the tests are both more cumbersome to maintain and less
informative when they break.

The way to fix this problem is to use
[fakes](https://www.chromium.org/chromium-os/developer-library/reference/cpp/cpp-patterns/index.md##abstract-base-class-impl-fake). As a bonus, code
written in this way tends to be more modular and flexible.

### Test the API, not the implementation

Unit tests should focus on the public API of the component rather than testing
the internal implementation. This has several benefits:

-   The internal implementation can change without breaking the tests. As long
    as the component maintains the same semantics in its public API, you are
    free to use any algorithm or data structure internally to achieve the
    result, and the tests just work.
-   The tests provide a good example of how to use the component. Unlike
    documentation, tests exercising the API are never out-of-date.
-   You don't need to find ways to circumvent access modifiers like `private` or
    `protected` in the tests.
-   The tests function more similarly to how the code is used in production.

## Test coverage

"Test coverage" or "code coverage" refers to the percentage of lines in a file
that are executed by unit tests. If a file has low test coverage, then that is a
strong indication that the code in question needs to be tested more thoroughly.

Note: The inverse is not always true. A file with high test coverage may not be
well-tested, since test coverage does not care what you're testing for, only
that the code is run by tests.

Test coverage is a somewhat crude metric for testing quality, but it has the
virtue of being easy to understand and calculate. For this reason, it is the
primary method for gauging whether the team is performing adequate testing, and
efforts to improve "operational excellence" will often target this metric.

### How much test coverage is enough?

As a rough guideline, aim for at least 80%, preferably 90% test coverage.

Trying for more than 90% test coverage is often counterproductive. It is often
the case that code will contain some lines that are never meant to be run under
normal circumstances. These should be guarded with `NOTREACHED()`, `LOG(FATAL)`,
etc. Tests that target this kind of code will usually require a lot of abuse of
the code under test to even make it possible for these lines to be run, and they
don't add much value over a simple `CHECK()` statement.

In some cases, e.g. short boilerplate files, getting to 80% test coverage can be
difficult or unreasonable simply because there isn't anything worth testing.
This isn't a problem necessarily, so long as you are able to justify it for your
CL reviewer.

That said, experienced CL reviewers will be suspicious of code with test
coverage less than 80%, and will usually request that you write more tests. It's
better to be proactive and write thorough tests for your code before sending it
out for review.

### Checking test coverage in Gerrit

Gerrit automatically calculates and displays test coverage for each file in a
CL. All you need to do is click the "DRY RUN" button and wait for the tests to
finish running. It's best to do this before sending a CL for review, since your
reviewer will want to know that the tests are passing and see the test coverage.

![](images/gerrit_test_coverage.png)

For each file, Gerrit displays two types of coverage, absolute coverage (|Cov|)
and incremental coverage (ΔCov). Absolute coverage is based on all lines in the
file, regardless of whether they have been changed in the CL or not. Incremental
coverage only considers the lines that have been changed. For the purpose of
reviewing a CL, incremental coverage matters most. It's unreasonable to expect
the author to write tests for code unrelated to the change.

There is also a "Code Coverage Check" warning that will be displayed if test
coverage is below 70%.

Finally, Gerrit color-codes individual lines so that you can see whether they
have test coverage or not. This makes it easy to decide what tests to write in
order to increase coverage.

![](images/gerrit_test_coverage_line_by_line.png)

### Looking at test coverage by directory

The code coverage dashboard (go/chrome-coverage) is a good way to find
up-to-date test coverage information by file or by directory. Be sure to select
"ChromeOS" for the platform.

### Looking at test coverage using CL hashtags

When developing a new feature, we often want to get a quick idea of overall test
coverage for the feature without having to tease apart which directories were
affected by the new feature, and which were not. This handy PLX dashboard
(go/chrome-feature-code-coverage) allows you to specify a Gerrit hashtag and see
the incremental test coverage for all CLs with that tag.

For this to work, you need to apply Gerrit hashtags to each of your CLs. This is
most easily done by agreeing with your team on what hashtag to use before
beginning development.

There are two ways to set hashtags:

-   When uploading a new CL, put the hashtag in the title of the CL enclosed
    with square brackets. On first upload, Depot Tools will parse the title for
    tags and apply them automatically.
-   Inside Gerrit, the left pane has a "Hashtags" field where you can change the
    hashtags later on. You may need to click the "SHOW ALL" button for this
    field to be displayed.

WARNING: Changing a CL's title after the first upload will not add/remove
hashtags from the CL. It only works on the very first `git cl upload`.

![](images/gerrit_hashtags.png)

## Common testing patterns

### Friending the tests

Using the `friend` keyword is a way to give another class exclusive access to
the private members of a class. It should usually be avoided since it's a sign
of poor design, but it can be handy for writing tests against helper functions
or for setting private members of a class in a test. Just be aware that it's
better style to first try to get it working
[using only public members](#test-the-api-not-the-implementation).

The pattern looks like this:

```cpp
class ClassUnderTest {
 private:
  friend class ClassUnderTestFixture;

  int PrivateHelperFunction(bool arg);
};

// Unit tests
class ClassUnderTestFixture {
 public:
  int CallPrivateHelperFunction(bool arg) {
    // We're allowed to call this private function because this text fixture
    // was declared to be a friend of ClassUnderTest.
    return PrivateHelperFunction(arg);
  }
};
```

## Testing asynchronous code

### Asynchronous work and task runners

Task runners are used to post tasks to be executed asynchronously (see
[Chromium documentation](https://chromium.googlesource.com/chromium/src/+/main/docs/threading_and_tasks.md)
to learn more). We also encounter asynchronous code in the form of methods that
accept completion callbacks, or when working with Mojo.

### Asynchronous code and continuation passing style

Asynchronous code in Chromium is most often written in continuation passing
style, e.g.

```cpp
void DoTheThing(int arg1, bool arg2, base::OnceClosure callback);
```

The idea here is that the function can return quickly after beginning the
(potentially long-running) operation, but the operation hasn't actually been
completed until `callback` has been called.

We often need to write unit tests for functions like this. Naively, you might
consider just creating a callback

```cpp
TEST(TestExamples, DoTheThing) {
  bool callback_called = false;
  base::OnceClosure callback = base::BindLambdaForTesting(
    [&callback_called]() {
      callback_called = true;
    }
  );

  DoTheThing(1, true, std::move(callback));
  EXPECT_TRUE(callback_called); // Might not succeed !!
}
```

There are a couple of problems with this example:

1.  Lambdas introduce some boilerplate and can be difficult to read
2.  Depending on whether `DoTheThing()` posts tasks to the thread pool, the
    callback may not be called until after `EXPECT_TRUE(callback_called)`.

We can fix (2) by adding a call to `RunLoop::RunUntilIdle()`, but this makes the
tests flaky (see below) and should be avoided, and it doesn't help with (1).

```cpp
TEST(TestExamples, DoTheThing) {
  bool callback_called = false;
  base::OnceClosure callback = base::BindLambdaForTesting(
    [&callback_called]() {
      callback_called = true;
    }
  );

  DoTheThing(1, true, std::move(callback));
  base::RunLoop::RunUntilIdle(); // AVOID DOING THIS !!
  EXPECT_TRUE(callback_called);
}
```

A better solution is to use `TestFuture`.

### TestFuture

To test asynchronous code, consider using a
[TestFuture](https://source.chromium.org/chromium/chromium/src/+/main:base/test/test_future.h).
This allows you to wait for a return value from an asynchronous method using a
very concise syntax, for example:

```cpp
TestFuture<ResultType> future;
object_under_test.DoSomethingAsync(future.GetCallback());
const ResultType& actual_result = future.Get();
```

`TestFuture::Get()` will synchronously block the thread until a result is
available, similar to if you had created a `RunLoop` (see below).

See the documentation in
[test_future.h](https://source.chromium.org/chromium/chromium/src/+/main:base/test/test_future.h)
for more details on usage.

### RunLoops - Prefer QuitClosure()+Run() to RunUntilIdle()

Another recommended option (per the
[Chromium style guide](https://www.chromium.org/chromium-os/developer-library/guides/testing/unit-tests))
is to use `base::RunLoop`. A RunLoop will run the message loop asynchronously
and verify the behavior is expected, or injecting a task runner so tests can
control where tasks are run. Chromium best practice for these types of tests is
as follows: when writing a unit test for asynchronous logic, prefer
`base::RunLoop`'s `QuitClosure()` and `Run()` methods to target precisely which
ongoing tasks you want to wait for to finish -- instead of relying on
`RunUntilIdle()` to let all tasks finish. As per the
[documentation](https://source.chromium.org/chromium/chromium/src/+/main:base/run_loop.h;l=89?q=base::runloop),
`RunUntilIdle()` can cause flaky tests for the following reasons:

-   May run long (flakily timeout) and even never return
-   May return too early. For example, if used to run until an incoming event
    has occurred but that event depends on a task in a different queue -- e.g.
    another TaskRunner or a system event.

`QuitClosure()` and `Run()` also provide the benefit of being able to block on
specific conditions.

To use `QuitClosure()` + `Run()`:

-   Pass the `QuitClosure()` into the async call that is being tested
-   `Run()` the closure when the task is expected to be run
-   Explicitly call the blocking `Run()` method on the `base::RunLoop` to
    guarantee that the test will not progress until the quit closure is invoked.
-   Verify the results are expected

#### Example 1: Binding the `QuitClosure` to a callback

```cpp
void OnGetGroupPrivateKeyStatus(base::OnceClosure callback,
                                GroupPrivateKeyStatus status) {
  get_group_private_key_status_response_ = status;
  std::move(callback).Run();
}

TEST_F(DeviceSyncClientImplTest, TestGetGroupPrivateKeyStatus) {
  SetupClient();

  base::RunLoop run_loop;
  client_->GetGroupPrivateKeyStatus(
      base::BindOnce(&DeviceSyncClientImplTest::OnGetGroupPrivateKeyStatus,
                     base::Unretained(this), run_loop.QuitClosure()));

  SendPendingMojoMessages();

  fake_device_sync_->InvokePendingGetGroupPrivateKeyStatusCallback(
      expected_status);
  run_loop.Run();

  EXPECT_EQ(expected_status, get_group_private_key_status_response_);
}
```

#### Example 2: Using [base::BindLambdaForTesting](https://source.chromium.org/chromium/chromium/src/+/main:base/test/bind.h;l=52?q=BindLambdaForTesting)

```cpp
TEST_F(NearbyPresenceTest, UpdateRemoteSharedCredentials_Success) {
  std::vector<mojom::SharedCredentialPtr> creds = CreateSharedCredentials();

  base::RunLoop run_loop;
  nearby_presence_->UpdateRemoteSharedCredentials(
      std::move(creds), kAccountName,
      base::BindLambdaForTesting([&](mojom::Status status) {
        EXPECT_EQ(mojom::Status::kOk, status);
        auto creds = fake_presence_service_->GetRemoteSharedCredentials();
        EXPECT_FALSE(creds.empty());
        EXPECT_EQ(3u, creds.size());
        EXPECT_EQ(std::string(kSecretId1.begin(), kSecretId1.end()),
                  creds[0].secret_id());
        EXPECT_EQ(std::string(kSecretId2.begin(), kSecretId2.end()),
                  creds[1].secret_id());
        EXPECT_EQ(std::string(kSecretId3.begin(), kSecretId3.end()),
                  creds[2].secret_id());
        run_loop.Quit();
      }));
  run_loop.Run();
}
```

## Testing With multiple Feature Flags

The most robust way to test a class affected by feature flags is to run all unit
tests under all combinations of affecting feature flags enabled/disabled. This
is easily achieved using
[Value-Parametrized Tests](https://g3doc.corp.google.com/third_party/googletest/g3doc/advanced.md?cl=head#value-parameterized-tests).

The idea is to pass feature bit masks as parameters to the test suite in which
each bit represents a feature flag being enabled (`1`) or disabled (`0`). Every
combination of flags can be represented with `num flags` bits by counting up
from `0` to `2^(num flags)-1`: when passed in as parameters, these bit masks are
translated into enabled and disabled flags upon test suite creation.

**Example:**

Consider 2 flags, `Flag0` and `Flag1`. To represent each combination of their
enabled/disabled state, 2 bits are used:

*   `00` -> both disabled
*   `01` -> `Flag0` enabled, `Flag1` disabled
*   `10` -> `Flag1` enabled, `Flag0` disabled
*   `11` -> both enabled

### Instantiate Test Suite

A parameterized test suite is defined using the `INSTANTIATE_TEST_SUITE_P` macro
called below the unit tests in its suite.

The first argument to `INSTANTIATE_TEST_SUITE_P` is a name unique to the test
suite. The second is the name of the test pattern. The third argument is the
parameter generator, which in this case generates the range of whole numbers
`[0, 2^num flags)`.

```cpp
INSTANTIATE_TEST_SUITE_P(UniqueTestSuiteName,
                         TestPatternName,
                         testing::Range<size_t>(0, 1 << kTestFeatures.size()));
```

`kTestFeatures` should be defined at the top of the file in an unnamed namespace
per the
[Chromium c++ style guide](https://chromium.googlesource.com/chromium/src/+/refs/heads/master/styleguide/c++/c++.md#unnamed-namespaces).

```
const std::vector<base::test::FeatureRef> kTestFeatures = {
    features::Flag0, features::Flag1, features::Flag2};
```

Note: `size_t` only represents `2^16` numbers, so for feature flag lists with
size `> 16` use type `int` for the feature mask instead. If that many flags are
in use, the class should probably be tested differently anyways.

### Create Feature List

Inside the test class, the feature mask needs to be translated into enabled and
disabled features. This is accomplished by mapping each bit to a feature flag in
`kTestFeatures` by index and enabling it if the bit is `1` and disabling it if
`0`.

```cpp
void CreateFeatureList(size_t feature_mask) {
  std::vector<base::test::FeatureRef> enabled_features;
  std::vector<base::test::FeatureRef> disabled_features;

  for (size_t i = 0; i < kTestFeatures.size(); i++) {
    if (feature_mask & 1 << i) {
      enabled_features.push_back(kTestFeatures[i]);
    } else {
      disabled_features.push_back(kTestFeatures[i]);
    }
  }

  scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
}
```

```cpp
base::test::ScopedFeatureList scoped_feature_list_;
```

Call `CreateFeatureList` somewhere inside of the test class' constructor. The
test suite parameter is obtained with `GetParam()`.

```cpp
class UniqueTestSuiteName : public testing::Test {
 public:
  explicit UniqueTestSuiteName
    : CreateFeatureList(GetParam())
...
}
```

### Write Parameterized Unit Tests

Parameterized unit tests require the `TEST_P` macro. The first argument to
`TEST_P` is the same unique test suite name that is the first argument to
`INSTANTIATE_TEST_SUITE_P`. The second is the name of the test.

If the unit test's contents should only be run for a subset of flag conditions,
remember to sequester them using an `if` branch: this both prevents the test
from failing and the test's contents from executing unnecessarily, which saves
computation.

```cpp
TEST_P(UniqueTestSuiteName, Flag0Enabled_Test) {
  if(IsFlag0Enabled()) {
    ...
  }
}
```

## Mojo

See [Stubbing Mojo Pipes](https://www.chromium.org/chromium-os/developer-library/reference/cpp/cpp-mojo/#stubbing-mojo-pipes) for pointers on how
to unit test Mojo calls.
