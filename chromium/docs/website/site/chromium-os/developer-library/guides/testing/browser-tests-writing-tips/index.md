---
breadcrumbs:
- - /chromium-os/developer-library/guides
  - ChromiumOS > Guides
page_name: browser-tests-writing-tips
title: Tips on writing C++ Browser tests
---

[TOC]

## How to fix missing a taskEnvironment member error in a ContentBrowserTest

**Prerequisites**

Browsertest, Threadpool

**Problem**

I am adding a browser test that inherits from the ContentBrowserTest. Before
adding a `base::test::TaskEnvironment` member. I got the following error when
running the test:

```
FATAL components_browsertests[3500133:3500133]: [thread_pool.cc(36)] Check
failed: instance. Ref. Prerequisite section of base/task/thread_pool.h.
Hint: if this is in a unit test, you're likely merely missing a
base::test::TaskEnvironment member in your fixture (or your fixture is using a
base::test::SingleThreadTaskEnvironment and now needs a full
base::test::TaskEnvironment).
```

So I added `content::BrowserTaskEnvironment task_environment_;` as the first
member of the test class as shown below.

```
class FeedbackDataBrowserTest : public content::ContentBrowserTest {
 public:
  FeedbackDataBrowserTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
    uploader_ = std::make_unique<feedback::FeedbackUploader>(
        /*is_oF_the_record=*/false, scoped_temp_dir_.GetPath(),
        test_shared_loader_factory_);
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();
  }

  ...

 protected:
  content::BrowserTaskEnvironment task_environment_;

  scoped_refptr<feedback::FeedbackData> CreateFeedbackData() {
    base::WeakPtr<feedback::FeedbackUploader> wkptr_uploader =
        base::AsWeakPtr(uploader_.get());
    return base::MakeRefCounted<feedback::FeedbackData>(
        std::move(wkptr_uploader), ContentTracingManager::Get());
  }

```

Then, I got another error.

```
FATAL components_browsertests[3499191:3499191]: [task_environment.cc(436)] Check
failed: !ThreadPoolInstance::Get(). Someone has already installed a
ThreadPoolInstance. If nothing in your test does so, then a test that ran
earlier may have installed one and leaked it. base::TestSuite will trap leaked
globals, unless someone has explicitly disabled it with
DisableCheckForLeakedGlobals().
```

**Solution**

The root cause was that FeedbackUploader is being instantiated in the
constructor, before the browser process has initialized. After moving that code
into the `void SetUpOnMainThread() override`, the error is gone.

The browser test fixture provides the following hooks to add custom code to be
run before and after each test:

```
  // Configures everything for an in process browser test (e.g. thread pool,
  // etc.) by invoking ContentMain (or manually on OS_ANDROID). As such all
  // single-threaded initialization must be done before this step.
  //
  // ContentMain then ends up invoking RunTestOnMainThreadLoop with browser
  // threads already running.
  void SetUp() override;

  // Restores state configured in SetUp.
  void TearDown() override;

  // Override this to add any custom setup code that needs to be done on the
  // main thread after the browser is created and just before calling
  // RunTestOnMainThread().
  virtual void SetUpOnMainThread() {}
```

NOTE: The feedback uploader has this member
`scoped_refptr<base::SingleThreadTaskRunner> task_runner_;`. All single-threaded
initialization is expected done after a browser process is created. Therefore,
the code instantiating feedback uploader should be added to SetUpOnMainThread.

```
class FeedbackDataBrowserTest : public content::ContentBrowserTest {
 public:
  FeedbackDataBrowserTest()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    EXPECT_TRUE(scoped_temp_dir_.CreateUniqueTempDir());
  }

  void SetUpOnMainThread() override {
    content::ContentBrowserTest::SetUpOnMainThread();

    uploader_ = std::make_unique<feedback::FeedbackUploader>(
        /*is_of_the_record=*/false, scoped_temp_dir_.GetPath(),
        test_shared_loader_factory_);
  }

  FeedbackDataBrowserTest(const FeedbackDataBrowserTest&) = delete;
  FeedbackDataBrowserTest& operator=(const FeedbackDataBrowserTest&) = delete;

  ~FeedbackDataBrowserTest() override = default;

 protected:
  scoped_refptr<feedback::FeedbackData> CreateFeedbackData() {
    base::WeakPtr<feedback::FeedbackUploader> wkptr_uploader =
        base::AsWeakPtr(uploader_.get());
    return base::MakeRefCounted<feedback::FeedbackData>(
        std::move(wkptr_uploader), ContentTracingManager::Get());
  }

  base::ScopedTempDir scoped_temp_dir_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  std::unique_ptr<feedback::FeedbackUploader> uploader_;
};

```

**Example CL**

*   https://crrev.com/c/3846382

**References**

*   https://crsrc.org/c/docs/threading_and_tasks_testing.md

**Comment/Discussion**

Tip: You shouldn't need to create a task environment in browsertests, that's for
unittests that aren't running in a full browser environment already

## How to verify a function with a callback parameter

**Prerequisites**

Callback, Async

**Problem**

I have a function with a callback parameter. I want to verify that the function
will eventually invoke the callback with expected data.

For example, I wanted to test that when the TakeScreenshot function is called,
it will eventually notify the callback whether the action is successful or not.

NOTE: TestFuture is nice to use if you only care about the returned values from
a callback, but for non-trivial callback handlers it may be better to create
your own callback function.

```
  // Take a screenshot of the primary display if any and persist the data in a
  // field.
  // Returns true in callback if screenshot is taken.
  // Returns false in callback if screenshot is not taken or failed.
  void TakeScreenshot(ScreenshotCallback callback);
```

**Solution**

One simple way is to use the TestFuture, a Helper class to test code that
returns its result(s) asynchronously through a callback:

-   Pass the callback provided by TestFuture::GetCallback() to the code under
    test.
-   Wait for the callback to be invoked by calling TestFuture::Wait(), or
    TestFuture::Get() to access the value(s) passed to the callback.

Key Steps:

1.  Import
    [TestFuture](https://crsrc.org/c/base/test/test_future.h;drc=a432cd59d51281057ba2a2673ca645a9600bb927;l=0)
    and
    [TaskEnvironment](https://crsrc.org/c/base/test/task_environment.h;drc=a432cd59d51281057ba2a2673ca645a9600bb927;l=0).

> NOTE: TaskEnvironment is needed to enable the following APIs within its scope:
>
> -   (Thread|Sequenced)TaskRunnerHandle on the main thread
> -   RunLoop on the main thread
> -   posting to base::ThreadPool through base/task/thread_pool.h.

```
#include "base/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"
```

1.  Add a new test {value=2}

```
class OsFeedbackScreenshotManagerTest : public ::testing::Test {
 public:
  OsFeedbackScreenshotManagerTest() = default;
  ~OsFeedbackScreenshotManagerTest() override = default;

 protected:
  void SetTestPngData() {
    OsFeedbackScreenshotManager::GetInstance()->SetPngDataForTesting(
        CreateFakePngData());
  }

  scoped_refptr<base::RefCountedMemory> CreateFakePngData() {
    const unsigned char kData[] = {12, 11, 99};
    return base::MakeRefCounted<base::RefCountedBytes>(kData, std::size(kData));
  }
};

// Test that TakeScreenshot will skip if a screenshot exists already.
TEST_F(OsFeedbackScreenshotManagerTest, DoNotTakeScreenshotIfExists) {
  base::test::SingleThreadTaskEnvironment task_environment;
  base::test::TestFuture<bool> future;
  auto* manager = OsFeedbackScreenshotManager::GetInstance();
  SetTestPngData();
  EXPECT_NE(nullptr, manager->GetScreenshotData());
  EXPECT_EQ(3, manager->GetScreenshotData()->size());

  manager->TakeScreenshot(future.GetCallback());

  // Wait for the callback to be invoked and access the value passed to the
  // callback.
  EXPECT_FALSE(future.Get());
  EXPECT_EQ(3, manager->GetScreenshotData()->size());
}
```

**Example CL**

*   https://crrev.com/c/3645699

**References**

*   https://crsrc.org/c/docs/callback.md

**Comment/Discussion**

Tip: If the callback takes multiple arguments, use TestFuture::Get<0>() to
access the value of the first argument, TestFuture::Get<1>() to access the value
of the second argument, and so on.

## How to verify a metric is recorded successfully

**Prerequisites**

Histogram, browser tests

**Problem**

After adding some metrics, we wanted to write browser tests to verify the
metrics will be recorded correctly.

**Solution**

1.  Step 1: Include header file `base/test/metrics/histogram_tester.h`

```
#include "base/test/metrics/histogram_tester.h"
```

1.  Step 2: Add a new test
2.  Create an instance of `base::HistogramTester`
3.  Write test code to test the code where a metric/histogram will be recorded.
4.  Use the `ExpectBucketCount` method to verify that the number of samples in
    the bucket of the histogram grew by 1.
5.  Use the `ExpectTotalCount` method to verify that the number of samples in
    histogram grew by 1, regardless buckets.

```
void TestOpenOsFeedbackDialog() {
  base::HistogramTester histogram_tester;
  Profile* const profile = ash::ProfileHelper::GetSigninProfile();
  auto login_feedback = std::make_unique<ash::LoginFeedback>(profile);
  // There should be none instance.
  EXPECT_FALSE(HasInstanceOfOsFeedbackDialog());

  base::test::TestFuture<void> test_future;
  // Open the feedback dialog.
  login_feedback->Request("Test feedback", test_future.GetCallback());
  EXPECT_TRUE(test_future.Wait());

  // Verify an instance exists now.
  EXPECT_TRUE(HasInstanceOfOsFeedbackDialog());
  histogram_tester.ExpectBucketCount("Feedback.RequestSource",
                                     chrome::kFeedbackSourceLogin, /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Feedback.RequestSource", /*expected_count=*/1);
}
```

**Example CL**

*   [feedback: Record feedback dialog opened from oobe or login screen](https://crrev.com/c/4902413)
    {.external}

**References**

*   [Histogram Guidelines](https://chromium.googlesource.com/chromium/src.git/+/HEAD/tools/metrics/histograms/README.md#flag-histograms)
    {.external}

## How to skip an individual test when a condition is met

**Prerequisites**

Feature flag, Browser tests

**Problem**

There is a test that is valid only when a feature flag is disabled. Before we
can turn on the feature flag by default, we can't remove the test.

Most feature flags are disabled by default. Before staring A/B testing for a
feature in Finch, we need to update the fieldtrial testing config to enable the
feature in tests. Then, the test incompatible with the feature will break.

**Solution**

1.  Step 1: Include header file `testing/gtest/include/gtest/gtest.h`

```
#include "testing/gtest/include/gtest/gtest.h"
```

1.  In the beginning of the test, call `GTEST_SKIP();` when the condition of not
    running it is true.
2.  Add a TODO for future cleanup.

```
void TestFeedback() {
  // TODO(http://b/309467654): clean up obsolete code.
  if (ash::features::IsOsFeedbackDialogEnabled()) {
    GTEST_SKIP();
  }
  Profile* const profile = ProfileHelper::GetSigninProfile();
  auto login_feedback = std::make_unique<ash::LoginFeedback>(profile);

  base::RunLoop run_loop;
  // Test that none feedback dialog exists.
  ASSERT_EQ(nullptr, FeedbackDialog::GetInstanceForTest());
}
```

NOTE: There are many use cases for GTEST_SKIP. Solving issues with feature flags
is just one of them.

**Example CL**

*   [feedback: Add OsFeedbackDialog to field trial testing config](https://crrev.com/c/5005017)
    {.external}

## How to test code that posts tasks and uses BrowserThread::UI/IO?

**Prerequisites**

Threading and tasks, Browser tests

**Problem**

How to test code that posts tasks? If the test uses BrowserThread::UI/IO,
instantiate a content::BrowserTaskEnvironment for the scope of the test. Call
BrowserTaskEnvironment::RunUntilIdle() to wait until all tasks have run.

Note, the following code using `RunLoop` may not work. When
`run_loop.RunUntilIdle()` exits, there may still be tasks not completed.

```
    base::RunLoop run_loop;
    feedback_service->RedactThenSendFeedback(params, feedback_data_,
                                             std::move(mock_callback));
    base::ThreadPoolInstance::Get()->FlushForTesting();
    run_loop.RunUntilIdle();
```

**Solution**

Step 1: Include header file `content/public/test/test_browser_context.h`

```
#include "content/public/test/test_browser_context.h"
```

Step 2: Instantiate an instance of `content::BrowserTaskEnvironment` in the test
class.

```
 private:
  std::unique_ptr<content::BrowserTaskEnvironment> task_environment_;
```

Step 3: Ensure that all tasks have completed by calling
`task_environment_->RunUntilIdle()` before proceeding with test expectation
verification.

```
  void RunUntilFeedbackIsSent(scoped_refptr<FeedbackService> feedback_service,
                              const FeedbackParams& params,
                              SendFeedbackCallback mock_callback) {
    feedback_service->RedactThenSendFeedback(params, feedback_data_,
                                             std::move(mock_callback));
    base::ThreadPoolInstance::Get()->FlushForTesting();

    // Note: task_environment() returns an instance of
    // content::BrowserTaskEnvironment> defined in base class extensions_test.h;
    task_environment()->RunUntilIdle();
  }

  void TestSendFeedbackConcerningWifiDebugLogs(bool send_wifi_debug_logs) {
    // Code omitted on purpose to highlight the key steps.
    RunUntilFeedbackIsSent(feedback_service, params, mock_callback.Get());
    // Verify the attachment is added if and only if send_wifi_debug_logs is
    // true.
    EXPECT_EQ(send_wifi_debug_logs,
              AttachmentExists("iwlwifi_firmware_dumps.bz2", feedback_data_));
  }
```

TIP: If the test doesn't use BrowserThread::UI/IO, instead of using
content::BrowserTaskEnvironment, instantiate a base::test::TaskEnvironment for
the scope of the test. Call
[base::test::TaskEnvironment::RunUntilIdle()](https://crrev.com/c/2191982) to
wait until all tasks have run.

**Example CL**

*   [feedback: Attach wifi debug logs to reports](https://crrev.com/c/5018998)
    {.external}

**Reference**

*   [Threading and Tasks in Chrome - FAQ](https://crsrc.org/c/docs/threading_and_tasks_faq.md)
    {.external}
