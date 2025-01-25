// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_manager.h"

#include <memory>
#include <optional>

#include "ash/test/ash_test_base.h"
#include "base/types/expected.h"
#include "chromeos/ash/components/boca/boca_app_client.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/boca/session_api/get_session_request.h"
#include "chromeos/ash/services/network_config/public/cpp/cros_network_config_test_helper.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/common/request_sender.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArg;

namespace ash::boca {

namespace {
class MockSessionClientImpl : public SessionClientImpl {
 public:
  explicit MockSessionClientImpl(
      std::unique_ptr<google_apis::RequestSender> sender)
      : SessionClientImpl(std::move(sender)) {}
  MOCK_METHOD(void,
              GetSession,
              (std::unique_ptr<GetSessionRequest>),
              (override));
};

class MockObserver : public BocaSessionManager::Observer {
 public:
  MOCK_METHOD(void,
              OnSessionStarted,
              (const std::string& session_id,
               const ::boca::UserIdentity& producer),
              (override));
  MOCK_METHOD(void,
              OnSessionEnded,
              (const std::string& session_id),
              (override));
  MOCK_METHOD(void,
              OnBundleUpdated,
              (const ::boca::Bundle& bundle),
              (override));
  MOCK_METHOD(void,
              OnSessionCaptionConfigUpdated,
              (const std::string& group_name,
               const ::boca::CaptionsConfig& config),
              (override));
  MOCK_METHOD(void,
              OnLocalCaptionConfigUpdated,
              (const ::boca::CaptionsConfig& config),
              (override));
  MOCK_METHOD(void,
              OnSessionRosterUpdated,
              (const std::string& group_name,
               const std::vector<::boca::UserIdentity>& consumers),
              (override));
};

class MockBocaAppClient : public BocaAppClient {
 public:
  MOCK_METHOD(signin::IdentityManager*, GetIdentityManager, (), (override));
  MOCK_METHOD(scoped_refptr<network::SharedURLLoaderFactory>,
              GetURLLoaderFactory,
              (),
              (override));
};

constexpr char kTestGaiaId[] = "123";
constexpr char kTestUserEmail[] = "cat@gmail.com";
}  // namespace

class BocaSessionManagerTest : public testing::Test {
 public:
  BocaSessionManagerTest() = default;
  void SetUp() override {
    // Sign in test user.
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestGaiaId);
    const std::string username_hash =
        user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
    fake_user_manager_.Reset(std::make_unique<user_manager::FakeUserManager>());
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, username_hash,
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    wifi_device_path_ =
        cros_network_config_helper_.network_state_helper().ConfigureWiFi(
            shill::kStateIdle);

    session_client_impl_ =
        std::make_unique<StrictMock<MockSessionClientImpl>>(nullptr);

    observer_ = std::make_unique<StrictMock<MockObserver>>();

    boca_app_client_ = std::make_unique<StrictMock<MockBocaAppClient>>();
    // Start with empty session
    EXPECT_CALL(*session_client_impl_, GetSession(_))
        .WillOnce(testing::InvokeWithoutArgs([&]() {
          boca_session_manager()->ParseSessionResponse(base::ok(nullptr));
        }));

    // Expect to have registered session manager for current profile.
    EXPECT_CALL(*boca_app_client_, GetIdentityManager())
        .WillOnce(Return(identity_test_env_.identity_manager()));

    boca_session_manager_ = std::make_unique<BocaSessionManager>(
        session_client_impl_.get(), AccountId::FromUserEmail(kTestUserEmail));
    boca_session_manager_->AddObserver(observer_.get());
    ToggleOffline();
    ToggleOnline();

    // Trigger first load.
    task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 1 +
                                      base::Seconds(1));
  }

 protected:
  void ToggleOnline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_, shill::kStateProperty,
        base::Value(shill::kStateOnline));
  }

  void ToggleOffline() {
    cros_network_config_helper_.network_state_helper().SetServiceProperty(
        wifi_device_path_, shill::kStateProperty,
        base::Value(shill::kStateDisconnecting));
  }

  MockSessionClientImpl* session_client_impl() {
    return session_client_impl_.get();
  }
  MockObserver* observer() { return observer_.get(); }
  BocaSessionManager* boca_session_manager() {
    return boca_session_manager_.get();
  }
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>&
  fake_user_manager() {
    return fake_user_manager_;
  }
  MockBocaAppClient* boca_app_client() { return boca_app_client_.get(); }
  signin::IdentityManager* identity_manager() {
    return identity_test_env_.identity_manager();
  }
  content::BrowserTaskEnvironment* task_environment() {
    return &task_environment_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::string wifi_device_path_;
  network_config::CrosNetworkConfigTestHelper cros_network_config_helper_;
  // BocaAppClient should destruct after identity env.
  std::unique_ptr<StrictMock<MockBocaAppClient>> boca_app_client_;
  signin::IdentityTestEnvironment identity_test_env_;
  // Owned by BocaSessionManager, destructed before it.
  std::unique_ptr<StrictMock<MockSessionClientImpl>> session_client_impl_;
  std::unique_ptr<StrictMock<MockObserver>> observer_;
  user_manager::TypedScopedUserManager<user_manager::FakeUserManager>
      fake_user_manager_;
  std::unique_ptr<BocaSessionManager> boca_session_manager_;
};

TEST_F(BocaSessionManagerTest, DoNothingIfSessionUpdateFailed) {
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            base::unexpected<google_apis::ApiErrorCode>(
                google_apis::ApiErrorCode::PARSE_ERROR));
      }));

  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);
  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenSessionFlipBetweenEmpty) {
  const std::string session_id = "123";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_state(::boca::Session::ACTIVE);
  session_1->set_session_id(session_id);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(base::ok(nullptr));
      }));

  EXPECT_CALL(*observer(), OnSessionStarted(session_id, _)).Times(1);
  EXPECT_CALL(*observer(), OnSessionEnded(session_id)).Times(1);

  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenBothSessionIsEmpty) {
  auto current_session = std::make_unique<::boca::Session>();
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(
            std::move(current_session));
      }));

  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval +
                                    base::Seconds(1));
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);
  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(0);
}

TEST_F(BocaSessionManagerTest,
       NotifySessionUpdateWhenPreviousSessionStateChanged) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_state(::boca::Session::ACTIVE);
  session_1->set_session_id(session_id);
  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_state(::boca::Session::PLANNING);
  session_2->set_session_id(session_id);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionStarted(session_id, _)).Times(1);
  EXPECT_CALL(*observer(), OnSessionEnded(session_id)).Times(1);

  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenSessionStateIsTheSame) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_state(::boca::Session::ACTIVE);
  session_1->set_session_id(session_id);
  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_state(::boca::Session::ACTIVE);
  session_2->set_session_id(session_id);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionStarted(_, _)).Times(1);
  EXPECT_CALL(*observer(), OnSessionEnded(_)).Times(0);

  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenLockModeChanged) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_id(session_id);
  ::boca::SessionConfig session_config_2;
  auto* active_bundle_2 =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle_2->set_locked(false);
  active_bundle_2->mutable_content_configs()->Add()->set_url("google.com");
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenBundleContentChanged) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_id(session_id);
  ::boca::SessionConfig session_config_2;
  auto* active_bundle_2 =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle_2->set_locked(true);
  active_bundle_2->mutable_content_configs()->Add()->set_url("youtube.com");
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(2);
  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenBundleOrderChanged) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  active_bundle->mutable_content_configs()->Add()->set_url("youtube.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_id(session_id);
  ::boca::SessionConfig session_config_2;
  auto* active_bundle_2 =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle_2->set_locked(true);
  active_bundle_2->mutable_content_configs()->Add()->set_url("youtube.com");
  active_bundle_2->mutable_content_configs()->Add()->set_url("google.com");
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(2);
  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenBundledContentNoChange) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  ::boca::SessionConfig session_config;
  auto* active_bundle =
      session_config.mutable_on_task_config()->mutable_active_bundle();
  active_bundle->set_locked(true);
  active_bundle->mutable_content_configs()->Add()->set_url("google.com");
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_id(session_id);
  ::boca::SessionConfig session_config_2;
  auto* active_bundle_2 =
      session_config_2.mutable_on_task_config()->mutable_active_bundle();
  active_bundle_2->set_locked(true);
  active_bundle_2->mutable_content_configs()->Add()->set_url("google.com");
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);
  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  // On emit once when flip from initial empty state.
  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(1);

  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenCurrentBundleEmpty) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnBundleUpdated(_)).Times(0);

  // Have updated one session.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenSessionCaptionUpdated) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(true);
  caption_config_1->set_translations_enabled(true);
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_id(session_id);
  ::boca::SessionConfig session_config_2;
  auto* caption_config_2 = session_config.mutable_captions_config();

  caption_config_2->set_captions_enabled(false);
  caption_config_2->set_translations_enabled(false);
  (*session_2->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config_2);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  EXPECT_CALL(*observer(),
              OnSessionCaptionConfigUpdated(kMainStudentGroupName, _))
      .Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenSessionCaptionSame) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(false);
  caption_config_1->set_translations_enabled(false);
  (*session_1->mutable_student_group_configs())[kMainStudentGroupName] =
      std::move(session_config);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }));

  EXPECT_CALL(*observer(),
              OnSessionCaptionConfigUpdated(kMainStudentGroupName, _))
      .Times(0);

  // Have updated one session.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenSessionConfigNotMatch) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  ::boca::SessionConfig session_config;
  auto* caption_config_1 = session_config.mutable_captions_config();

  caption_config_1->set_captions_enabled(false);
  caption_config_1->set_translations_enabled(false);
  (*session_1->mutable_student_group_configs())["unknown"] =
      std::move(session_config);

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }));

  EXPECT_CALL(*observer(),
              OnSessionCaptionConfigUpdated(kMainStudentGroupName, _))
      .Times(0);

  // Have updated one session.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifySessionUpdateWhenSessionRosterUpdated) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  auto* student_groups_1 =
      session_1->mutable_roster()->mutable_student_groups()->Add();
  student_groups_1->set_title(kMainStudentGroupName);
  student_groups_1->mutable_students()->Add()->set_email("dog1@email.com");

  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_id(session_id);
  auto* student_groups_2 =
      session_2->mutable_roster()->mutable_student_groups()->Add();
  student_groups_2->set_title(kMainStudentGroupName);
  student_groups_2->mutable_students()->Add()->set_email("dog2@email.com");

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionRosterUpdated(_, _)).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest,
       NotifySessionUpdateWhenSessionRosterOrderUpdated) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();
  session_1->set_session_id(session_id);
  auto* student_groups_1 =
      session_1->mutable_roster()->mutable_student_groups()->Add();
  student_groups_1->set_title(kMainStudentGroupName);
  student_groups_1->mutable_students()->Add()->set_email("dog2@email.com");
  student_groups_1->mutable_students()->Add()->set_email("dog1@email.com");

  auto session_2 = std::make_unique<::boca::Session>();
  session_2->set_session_id(session_id);
  auto* student_groups_2 =
      session_2->mutable_roster()->mutable_student_groups()->Add();
  student_groups_2->set_title(kMainStudentGroupName);
  student_groups_2->mutable_students()->Add()->set_email("dog1@email.com");
  student_groups_2->mutable_students()->Add()->set_email("dog2@email.com");

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_2));
      }));

  EXPECT_CALL(*observer(), OnSessionRosterUpdated(_, _)).Times(2);

  // Have updated two sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 2 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNothingWhenSessionRosterSame) {
  const std::string session_id = "1";
  auto session_1 = std::make_unique<::boca::Session>();

  EXPECT_CALL(*session_client_impl(), GetSession(_))
      .WillOnce(testing::InvokeWithoutArgs([&]() {
        boca_session_manager()->ParseSessionResponse(std::move(session_1));
      }));

  EXPECT_CALL(*observer(), OnSessionRosterUpdated(_, _)).Times(0);

  // Have updated one sessions.
  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNotPollSessionWhenNoNetwork) {
  ToggleOffline();
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(0);

  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, DoNotPollSessionWhenUserNotActive) {
  EXPECT_CALL(*session_client_impl(), GetSession(_)).Times(0);

  // Sign in different user.
  auto account_id = AccountId::FromUserEmailGaiaId("another", "user");
  const std::string username_hash =
      user_manager::FakeUserManager::GetFakeUsernameHash(account_id);
  fake_user_manager().Reset(std::make_unique<user_manager::FakeUserManager>());
  fake_user_manager()->AddUser(account_id);
  fake_user_manager()->UserLoggedIn(account_id, username_hash,
                                    /*browser_restart=*/false,
                                    /*is_child=*/false);

  task_environment()->FastForwardBy(BocaSessionManager::kPollingInterval * 1 +
                                    base::Seconds(1));
}

TEST_F(BocaSessionManagerTest, NotifyLocalCaptionConfigWhenLocalChange) {
  EXPECT_CALL(*boca_app_client(), GetIdentityManager())
      .WillOnce(Return(identity_manager()));
  EXPECT_CALL(*observer(), OnLocalCaptionConfigUpdated(_)).Times(1);

  ::boca::CaptionsConfig config;
  BocaAppClient::Get()->GetSessionManager()->NotifyLocalCaptionEvents(config);
}

}  // namespace ash::boca
