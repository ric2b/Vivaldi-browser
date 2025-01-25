// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/ios_password_manager_driver.h"

#import "base/memory/raw_ptr.h"
#import "base/strings/sys_string_conversions.h"
#import "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/password_manager/core/browser/password_manager.h"
#import "components/password_manager/core/browser/password_manager_client.h"
#import "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#import "components/password_manager/core/browser/stub_password_manager_client.h"
#import "components/password_manager/ios/ios_password_manager_driver_factory.h"
#import "components/password_manager/ios/shared_password_controller.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "testing/gtest_mac.h"
#import "testing/platform_test.h"
#import "third_party/ocmock/OCMock/OCMock.h"

using autofill::AutofillJavaScriptFeature;
using base::SysNSStringToUTF8;
using password_manager::PasswordManager;
using testing::Return;

#define andCompareStringAtIndex(expected_string, index) \
  andDo(^(NSInvocation * invocation) {                  \
    const std::string* param;                           \
    [invocation getArgument:&param atIndex:index + 2];  \
    EXPECT_EQ(*param, expected_string);                 \
  })

// This is a workaround for returning const GURL&, for which .andReturn and
// .andReturnValue don’t work.
@interface URLGetter : NSObject

- (const GURL&)lastCommittedURL;

@end

@implementation URLGetter

- (const GURL&)lastCommittedURL {
  return GURL::EmptyGURL();
}

@end

class MockPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  MOCK_METHOD(bool,
              IsSavingAndFillingEnabled,
              (const GURL&),
              (const, override));
  MOCK_METHOD(password_manager::PasswordStoreInterface*,
              GetProfilePasswordStore,
              (),
              (const, override));
};

class IOSPasswordManagerDriverTest : public PlatformTest {
 public:
  IOSPasswordManagerDriverTest() : PlatformTest() {
    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    web_frames_manager_ = web_frames_manager.get();
    web::ContentWorld content_world =
        AutofillJavaScriptFeature::GetInstance()->GetSupportedContentWorld();
    web_state_.SetWebFramesManager(content_world,
                                   std::move(web_frames_manager));

    auto web_frame = web::FakeWebFrame::Create(SysNSStringToUTF8(@"main-frame"),
                                               /*is_main_frame=*/true, GURL());
    auto web_frame2 =
        web::FakeWebFrame::Create(SysNSStringToUTF8(@"frame"),
                                  /*is_main_frame=*/false, GURL());
    web::WebFrame* frame = web_frame.get();
    web::WebFrame* frame2 = web_frame2.get();
    web_frames_manager_->AddWebFrame(std::move(web_frame));
    web_frames_manager_->AddWebFrame(std::move(web_frame2));

    password_controller_ = OCMStrictClassMock([SharedPasswordController class]);

    IOSPasswordManagerDriverFactory::CreateForWebState(
        &web_state_, password_controller_, &password_manager_);

    driver_ = IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
        &web_state_, frame);
    driver2_ = IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(
        &web_state_, frame2);
  }

 protected:
  raw_ptr<web::FakeWebFramesManager> web_frames_manager_;
  web::FakeWebState web_state_;
  raw_ptr<IOSPasswordManagerDriver> driver_;
  raw_ptr<IOSPasswordManagerDriver> driver2_;
  id password_controller_;
  testing::StrictMock<MockPasswordManagerClient> password_manager_client_;
  PasswordManager password_manager_ =
      PasswordManager(&password_manager_client_);
};

// Tests that the drivers have the correct ids.
TEST_F(IOSPasswordManagerDriverTest, GetId) {
  ASSERT_EQ(driver_->GetId(), 0);
  ASSERT_EQ(driver2_->GetId(), 1);
}

// Tests the IsInPrimaryMainFrame method.
TEST_F(IOSPasswordManagerDriverTest, IsInPrimaryMainFrame) {
  ASSERT_TRUE(driver_->IsInPrimaryMainFrame());
  ASSERT_FALSE(driver2_->IsInPrimaryMainFrame());
}

// Tests the SetPasswordFillData method.
TEST_F(IOSPasswordManagerDriverTest, SetPasswordFillData) {
  autofill::PasswordFormFillData form_data;

  OCMExpect([[password_controller_ ignoringNonObjectArgs]
                processPasswordFormFillData:form_data
                                 forFrameId:""
                                isMainFrame:driver_->IsInPrimaryMainFrame()
                          forSecurityOrigin:driver_->security_origin()])
      .andCompareStringAtIndex(driver_->web_frame_id(), 1);
  driver_->SetPasswordFillData(form_data);
  [password_controller_ verify];
}

// Tests the InformNoSavedCredentials method.
TEST_F(IOSPasswordManagerDriverTest, InformNoSavedCredentials) {
  const std::string main_frame_id = SysNSStringToUTF8(@"main-frame");
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
                onNoSavedCredentialsWithFrameId:""])
      .andCompareStringAtIndex(main_frame_id, 0);
  driver_->InformNoSavedCredentials(true);
  [password_controller_ verify];
}

// Tests the FormEligibleForGenerationFound method.
TEST_F(IOSPasswordManagerDriverTest, FormEligibleForGenerationFound) {
  autofill::PasswordFormGenerationData form;

  scoped_refptr<password_manager::MockPasswordStoreInterface> store(
      new password_manager::MockPasswordStoreInterface);
  EXPECT_CALL(password_manager_client_, GetProfilePasswordStore)
      .WillRepeatedly(testing::Return(store.get()));

  EXPECT_CALL(*store, IsAbleToSavePasswords).WillOnce(Return(true));
  EXPECT_CALL(password_manager_client_, IsSavingAndFillingEnabled(GURL()))
      .WillOnce(Return(true));
  EXPECT_CALL(*password_manager_client_.GetPasswordFeatureManager(),
              IsGenerationEnabled())
      .WillOnce(Return(true));

  URLGetter* url_getter = [URLGetter alloc];
  OCMStub([password_controller_ lastCommittedURL])
      .andCall(url_getter, @selector(lastCommittedURL));

  OCMExpect([password_controller_ formEligibleForGenerationFound:form]);
  OCMExpect([[password_controller_ ignoringNonObjectArgs]
      attachListenersForPasswordGenerationFields:form
                                      forFrameId:""]);
  driver_->FormEligibleForGenerationFound(form);
  [password_controller_ verify];
}
