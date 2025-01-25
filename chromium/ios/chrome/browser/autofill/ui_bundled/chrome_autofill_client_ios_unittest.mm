// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_UNITTEST_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_UNITTEST_H_

#import "ios/chrome/browser/autofill/ui_bundled/chrome_autofill_client_ios.h"

#import <memory>
#import <utility>

#import "base/memory/raw_ptr.h"
#import "components/autofill/core/browser/autofill_client.h"
#import "components/autofill/core/browser/browser_autofill_manager.h"
#import "components/autofill/core/browser/test_autofill_manager_waiter.h"
#import "components/autofill/core/common/autofill_test_utils.h"
#import "components/autofill/core/common/form_data.h"
#import "components/autofill/core/common/form_field_data.h"
#import "components/autofill/ios/browser/autofill_agent.h"
#import "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/autofill_driver_ios_factory.h"
#import "components/autofill/ios/browser/test_autofill_manager_injector.h"
#import "components/infobars/core/infobar_manager.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/test_chrome_browser_state.h"
#import "ios/chrome/browser/web/model/chrome_web_client.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/test/scoped_testing_web_client.h"
#import "ios/web/public/test/task_observer_util.h"
#import "ios/web/public/test/web_state_test_util.h"
#import "ios/web/public/test/web_task_environment.h"
#import "testing/gmock/include/gmock/gmock.h"
#import "testing/gtest/include/gtest/gtest.h"
#import "testing/platform_test.h"

namespace autofill {

namespace {

class TestAutofillManager : public BrowserAutofillManager {
 public:
  explicit TestAutofillManager(AutofillDriverIOS* driver)
      : BrowserAutofillManager(driver, "en-US") {}

  TestAutofillManagerWaiter& waiter() { return waiter_; }

 private:
  TestAutofillManagerWaiter waiter_{*this, {AutofillManagerEvent::kFormsSeen}};
};

}  //  namespace

class ChromeAutofillClientIOSTest : public PlatformTest {
 public:
  ChromeAutofillClientIOSTest()
      : web_client_(std::make_unique<ChromeWebClient>()) {
    browser_state_ = TestChromeBrowserState::Builder().Build();

    web::WebState::CreateParams params(browser_state_.get());
    web_state_ = web::WebState::Create(params);
    web_state_->GetView();
    web_state_->SetKeepRenderProcessAlive(true);
  }

  void SetUp() override {
    PlatformTest::SetUp();

    AutofillAgent* autofill_agent =
        [[AutofillAgent alloc] initWithPrefService:browser_state_->GetPrefs()
                                          webState:web_state_.get()];
    InfoBarManagerImpl::CreateForWebState(web_state_.get());
    autofill_client_ = std::make_unique<ChromeAutofillClientIOS>(
        browser_state_.get(), web_state_.get(),
        InfoBarManagerImpl::FromWebState(web_state_.get()), autofill_agent);
    autofill::AutofillDriverIOSFactory::CreateForWebState(
        web_state_.get(), autofill_client_.get(), autofill_agent, "en");
    autofill_manager_injector_ =
        std::make_unique<TestAutofillManagerInjector<TestAutofillManager>>(
            web_state_.get());
  }

  void TearDown() override {
    web::test::WaitForBackgroundTasks();
    PlatformTest::TearDown();
  }

 protected:
  bool LoadHtmlAndWaitForFormsSeen(NSString* html,
                                   size_t expected_number_of_forms) {
    web::test::LoadHtml(html, web_state_.get());
    return main_frame_manager()->waiter().Wait(1) &&
           main_frame_manager()->form_structures().size() ==
               expected_number_of_forms;
  }

  ChromeAutofillClientIOS& client() { return *autofill_client_; }

  TestAutofillManager* main_frame_manager() {
    return autofill_manager_injector_->GetForMainFrame();
  }

 private:
  test::AutofillUnitTestEnvironment autofill_environment_{
      {.disable_server_communication = true}};
  web::WebTaskEnvironment task_environment_;
  web::ScopedTestingWebClient web_client_;
  std::unique_ptr<TestChromeBrowserState> browser_state_;
  std::unique_ptr<ChromeAutofillClientIOS> autofill_client_;
  std::unique_ptr<web::WebState> web_state_;
  std::unique_ptr<TestAutofillManagerInjector<TestAutofillManager>>
      autofill_manager_injector_;
};

// Tests that ClassifyAsPasswordForm correctly classifies a login form.
TEST_F(ChromeAutofillClientIOSTest, ClassifyAsPasswordForm) {
  ASSERT_TRUE(LoadHtmlAndWaitForFormsSeen(
      @"<form>"
       "<input name='username' autocomplete='username'>"
       "<input type='password' name='password' autocomplete='current-password'>"
       "</form>",
      1));
  const FormStructure& form =
      *(main_frame_manager()->form_structures().begin()->second);
  FormData form_data = form.ToFormData();
  EXPECT_EQ(client().ClassifyAsPasswordForm(*main_frame_manager(),
                                            form_data.global_id(),
                                            form_data.fields()[0].global_id()),
            AutofillClient::PasswordFormType::kLoginForm);
}

}  // namespace autofill

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_IOS_UNITTEST_H_
