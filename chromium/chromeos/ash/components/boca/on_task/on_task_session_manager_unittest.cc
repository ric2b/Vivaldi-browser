// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/on_task/on_task_session_manager.h"

#include <memory>

#include "base/functional/callback.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "components/sessions/core/session_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::Return;
using ::testing::Sequence;

namespace ash::boca {
namespace {

constexpr char kTestUrl1[] = "https://www.test1.com";
constexpr char kTestUrl2[] = "https://www.test2.com";
constexpr char kTestUrl3[] = "https://www.test3.com";
constexpr char kTestUrl4[] = "https://www.test4.com";
constexpr char kTestUrl5[] = "https://www.test5.com";

// Mock implementation of the `OnTaskSystemWebAppManager`.
class OnTaskSystemWebAppManagerMock : public OnTaskSystemWebAppManager {
 public:
  OnTaskSystemWebAppManagerMock() = default;
  ~OnTaskSystemWebAppManagerMock() override = default;

  MOCK_METHOD(void,
              LaunchSystemWebAppAsync,
              (base::OnceCallback<void(bool)>),
              (override));
  MOCK_METHOD(void, CloseSystemWebAppWindow, (SessionID window_id), (override));
  MOCK_METHOD(SessionID, GetActiveSystemWebAppWindowID, (), (override));
  MOCK_METHOD(void,
              SetPinStateForSystemWebAppWindow,
              (bool pinned, SessionID window_id),
              (override));
  MOCK_METHOD(void,
              SetWindowTrackerForSystemWebAppWindow,
              (SessionID window_id),
              (override));
  MOCK_METHOD(void,
              CreateBackgroundTabWithUrl,
              (SessionID window_id,
               GURL url,
               OnTaskBlocklist::RestrictionLevel restriction_level),
              (override));
};

class OnTaskSessionManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    auto system_web_app_manager =
        std::make_unique<OnTaskSystemWebAppManagerMock>();
    system_web_app_manager_ptr_ = system_web_app_manager.get();
    session_manager_ = std::make_unique<OnTaskSessionManager>(
        std::move(system_web_app_manager));
  }

  std::unique_ptr<OnTaskSessionManager> session_manager_;
  raw_ptr<OnTaskSystemWebAppManagerMock> system_web_app_manager_ptr_;
};

TEST_F(OnTaskSessionManagerTest, ShouldLaunchBocaSWAOnSessionStart) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .Times(2)
      .WillRepeatedly(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldPrepareBocaSWAOnLaunch) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_,
              GetActiveSystemWebAppWindowID())
      .WillOnce(
          Return(SessionID::InvalidValue()))  // Initial check before launch.
      .WillOnce(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetWindowTrackerForSystemWebAppWindow(kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(true, kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(false, kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldClosePreExistingBocaSWAOnSessionStart) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(kWindowId))
      .WillRepeatedly(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
}

TEST_F(OnTaskSessionManagerTest, ShouldCloseBocaSWAOnSessionEnd) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(kWindowId))
      .Times(1);
  session_manager_->OnSessionEnded("test_session_id");
}

TEST_F(OnTaskSessionManagerTest, ShouldIgnoreWhenNoBocaSWAOpenOnSessionEnd) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CloseSystemWebAppWindow(_))
      .Times(0);
  session_manager_->OnSessionEnded("test_session_id");
}

TEST_F(OnTaskSessionManagerTest, ShouldOpenTabsOnBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .Times(3)
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl2), _))
      .Times(1);

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest, ShouldIgnoreWhenNoBocaSWAOpenOnBundleUpdated) {
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .Times(3)
      .WillRepeatedly(Return(SessionID::InvalidValue()));
  EXPECT_CALL(*system_web_app_manager_ptr_, CreateBackgroundTabWithUrl(_, _, _))
      .Times(0);

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest,
       TabsCreatedAfterSWALaunchedWhenSessionStartsAndBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  Sequence s;
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillOnce(Return(
          SessionID::InvalidValue()))  // Initial check before spawning SWA
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_, LaunchSystemWebAppAsync(_))
      .InSequence(s)
      .WillOnce([](base::OnceCallback<void(bool)> callback) {
        std::move(callback).Run(true);
      });
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetWindowTrackerForSystemWebAppWindow(kWindowId))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(true, kWindowId))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(false, kWindowId))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl2), _))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetWindowTrackerForSystemWebAppWindow(kWindowId))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(false, kWindowId))
      .Times(1)
      .InSequence(s);

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.add_content_configs()->set_url(kTestUrl2);
  session_manager_->OnSessionStarted("test_session_id", ::boca::UserIdentity());
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest, ShouldApplyRestrictionsToTabsOnBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl1),
                  OnTaskBlocklist::RestrictionLevel::kNoRestrictions))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl2),
                  OnTaskBlocklist::RestrictionLevel::kLimitedNavigation))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl3),
                  OnTaskBlocklist::RestrictionLevel::kSameDomainNavigation))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl4),
                  OnTaskBlocklist::RestrictionLevel::kOneLevelDeepNavigation))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(
                  kWindowId, GURL(kTestUrl5),
                  OnTaskBlocklist::RestrictionLevel::kNoRestrictions))
      .Times(1);

  ::boca::Bundle bundle;
  ::boca::ContentConfig* const content_config_1 =
      bundle.mutable_content_configs()->Add();
  content_config_1->set_url(kTestUrl1);
  content_config_1->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::OPEN_NAVIGATION);
  ::boca::ContentConfig* const content_config_2 =
      bundle.mutable_content_configs()->Add();
  content_config_2->set_url(kTestUrl2);
  content_config_2->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  ::boca::ContentConfig* const content_config_3 =
      bundle.mutable_content_configs()->Add();
  content_config_3->set_url(kTestUrl3);
  content_config_3->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::DOMAIN_NAVIGATION);
  ::boca::ContentConfig* const content_config_4 =
      bundle.mutable_content_configs()->Add();
  content_config_4->set_url(kTestUrl4);
  content_config_4->mutable_locked_navigation_options()->set_navigation_type(
      ::boca::LockedNavigationOptions::LIMITED_NAVIGATION);
  ::boca::ContentConfig* const content_config_5 =
      bundle.mutable_content_configs()->Add();
  content_config_5->set_url(kTestUrl5);
  session_manager_->OnBundleUpdated(bundle);
}

TEST_F(OnTaskSessionManagerTest, ShouldPinBocaSWAWhenLockedOnBundleUpdated) {
  const SessionID kWindowId = SessionID::NewUnique();
  EXPECT_CALL(*system_web_app_manager_ptr_, GetActiveSystemWebAppWindowID())
      .Times(2)
      .WillRepeatedly(Return(kWindowId));
  EXPECT_CALL(*system_web_app_manager_ptr_,
              CreateBackgroundTabWithUrl(kWindowId, GURL(kTestUrl1), _))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetWindowTrackerForSystemWebAppWindow(kWindowId))
      .Times(1);
  EXPECT_CALL(*system_web_app_manager_ptr_,
              SetPinStateForSystemWebAppWindow(true, kWindowId))
      .Times(1);

  ::boca::Bundle bundle;
  bundle.add_content_configs()->set_url(kTestUrl1);
  bundle.set_locked(true);
  session_manager_->OnBundleUpdated(bundle);
}

}  // namespace
}  // namespace ash::boca
