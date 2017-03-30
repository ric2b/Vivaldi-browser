// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/chrome_password_manager_client.h"

#include <stdint.h>

#include <string>
#include <tuple>

#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/channel_info.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/common/autofill_messages.h"
#include "components/password_manager/content/browser/password_manager_internals_service_factory.h"
#include "components/password_manager/core/browser/credentials_filter.h"
#include "components/password_manager/core/browser/log_manager.h"
#include "components/password_manager/core/browser/log_receiver.h"
#include "components/password_manager/core/browser/log_router.h"
#include "components/password_manager/core/browser/password_manager_internals_service.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sessions/content/content_record_password_state.h"
#include "components/syncable_prefs/testing_pref_service_syncable.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::WebContents;
using sessions::GetPasswordStateFromNavigation;
using sessions::SerializedNavigationEntry;
using testing::Return;
using testing::_;

namespace {

const char kPasswordManagerSettingsBehaviourChangeFieldTrialName[] =
    "PasswordManagerSettingsBehaviourChange";
const char kPasswordManagerSettingsBehaviourChangeEnabledGroupName[] =
    "PasswordManagerSettingsBehaviourChange.Active";
const char kPasswordManagerSettingsBehaviourChangeDisabledGroupName[] =
    "PasswordManagerSettingsBehaviourChange.NotActive";

// TODO(vabr): Get rid of the mocked client in the client's own test, see
// http://crbug.com/474577.
class MockChromePasswordManagerClient : public ChromePasswordManagerClient {
 public:
  MOCK_CONST_METHOD0(DidLastPageLoadEncounterSSLErrors, bool());

  explicit MockChromePasswordManagerClient(content::WebContents* web_contents)
      : ChromePasswordManagerClient(web_contents, nullptr) {
    ON_CALL(*this, DidLastPageLoadEncounterSSLErrors())
        .WillByDefault(testing::Return(false));
  }
  ~MockChromePasswordManagerClient() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockChromePasswordManagerClient);
};

class DummyLogReceiver : public password_manager::LogReceiver {
 public:
  DummyLogReceiver() = default;

  void LogSavePasswordProgress(const std::string& text) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyLogReceiver);
};

}  // namespace

class ChromePasswordManagerClientTest : public ChromeRenderViewHostTestHarness {
 public:
  ChromePasswordManagerClientTest()
      : field_trial_list_(nullptr), metrics_enabled_(false) {}
  void SetUp() override;
  void TearDown() override;

  syncable_prefs::TestingPrefServiceSyncable* prefs() {
    return profile()->GetTestingPrefService();
  }

  void EnforcePasswordManagerSettingsBehaviourChangeExperimentGroup(
      const char* name) {
    ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
        kPasswordManagerSettingsBehaviourChangeFieldTrialName, name));
  }

  // Caller does not own the returned pointer.
  ProfileSyncServiceMock* SetupBasicMockSync() {
    ProfileSyncServiceMock* mock_sync_service =
        static_cast<ProfileSyncServiceMock*>(
            ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
                profile(), BuildMockProfileSyncService));

    EXPECT_CALL(*mock_sync_service, IsFirstSetupComplete())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_sync_service, IsSyncActive())
        .WillRepeatedly(Return(true));
    return mock_sync_service;
  }

  // Make a navigation entry that will accept an annotation.
  void SetupNavigationForAnnotation() {
    ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
    EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
        .WillRepeatedly(Return(false));
    metrics_enabled_ = true;
    NavigateAndCommit(GURL("about:blank"));
  }

 protected:
  ChromePasswordManagerClient* GetClient();

  // If the test IPC sink contains an AutofillMsg_SetLoggingState message, then
  // copies its argument into |activation_flag| and returns true. Otherwise
  // returns false.
  bool WasLoggingActivationMessageSent(bool* activation_flag);

  TestingPrefServiceSimple prefs_;
  base::FieldTrialList field_trial_list_;
  bool metrics_enabled_;
};

void ChromePasswordManagerClientTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  prefs_.registry()->RegisterBooleanPref(
      password_manager::prefs::kPasswordManagerSavingEnabled, true);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      web_contents(), nullptr);

  // Connect our bool for testing.
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
      &metrics_enabled_);
}

void ChromePasswordManagerClientTest::TearDown() {
  ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

ChromePasswordManagerClient* ChromePasswordManagerClientTest::GetClient() {
  return ChromePasswordManagerClient::FromWebContents(web_contents());
}

bool ChromePasswordManagerClientTest::WasLoggingActivationMessageSent(
    bool* activation_flag) {
  const uint32_t kMsgID = AutofillMsg_SetLoggingState::ID;
  const IPC::Message* message =
      process()->sink().GetFirstMessageMatching(kMsgID);
  if (!message)
    return false;
  std::tuple<bool> param;
  AutofillMsg_SetLoggingState::Read(message, &param);
  *activation_flag = std::get<0>(param);
  process()->sink().ClearMessages();
  return true;
}

TEST_F(ChromePasswordManagerClientTest, LogSavePasswordProgressNotifyRenderer) {
  bool logging_active = true;
  // Ensure the existence of a driver, which will send the IPCs we listen for
  // below.
  NavigateAndCommit(GURL("about:blank"));

  // Initially, the logging should be off, so no IPC messages.
  EXPECT_TRUE(!WasLoggingActivationMessageSent(&logging_active) ||
              !logging_active)
      << "logging_active=" << logging_active;

  DummyLogReceiver log_receiver;
  password_manager::LogRouter* log_router = password_manager::
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile());
  EXPECT_EQ(std::string(), log_router->RegisterReceiver(&log_receiver));
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_TRUE(logging_active);

  log_router->UnregisterReceiver(&log_receiver);
  EXPECT_TRUE(WasLoggingActivationMessageSent(&logging_active));
  EXPECT_FALSE(logging_active);
}

TEST_F(ChromePasswordManagerClientTest,
       IsAutomaticPasswordSavingEnabledDefaultBehaviourTest) {
  EXPECT_FALSE(GetClient()->IsAutomaticPasswordSavingEnabled());
}

TEST_F(ChromePasswordManagerClientTest,
       IsAutomaticPasswordSavingEnabledWhenFlagIsSetTest) {
  // Add the enable-automatic-password-saving feature.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kEnableAutomaticPasswordSaving);

  if (chrome::GetChannel() == version_info::Channel::UNKNOWN)
    EXPECT_TRUE(GetClient()->IsAutomaticPasswordSavingEnabled());
  else
    EXPECT_FALSE(GetClient()->IsAutomaticPasswordSavingEnabled());
}

TEST_F(ChromePasswordManagerClientTest, GetPasswordSyncState) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();

  syncer::ModelTypeSet active_types;
  active_types.Put(syncer::PASSWORDS);
  EXPECT_CALL(*mock_sync_service, GetActiveDataTypes())
      .WillRepeatedly(Return(active_types));
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));

  ChromePasswordManagerClient* client = GetClient();

  // Passwords are syncing and custom passphrase isn't used.
  EXPECT_EQ(password_manager::SYNCING_NORMAL_ENCRYPTION,
            client->GetPasswordSyncState());

  // Again, using a custom passphrase.
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));

  EXPECT_EQ(password_manager::SYNCING_WITH_CUSTOM_PASSPHRASE,
            client->GetPasswordSyncState());

  // Report correctly if we aren't syncing passwords.
  active_types.Remove(syncer::PASSWORDS);
  active_types.Put(syncer::BOOKMARKS);
  EXPECT_CALL(*mock_sync_service, GetActiveDataTypes())
      .WillRepeatedly(Return(active_types));

  EXPECT_EQ(password_manager::NOT_SYNCING_PASSWORDS,
            client->GetPasswordSyncState());

  // Again, without a custom passphrase.
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));

  EXPECT_EQ(password_manager::NOT_SYNCING_PASSWORDS,
            client->GetPasswordSyncState());
}

TEST_F(ChromePasswordManagerClientTest, IsOffTheRecordTest) {
  ChromePasswordManagerClient* client = GetClient();

  profile()->ForceIncognito(true);
  EXPECT_TRUE(client->IsOffTheRecord());

  profile()->ForceIncognito(false);
  EXPECT_FALSE(client->IsOffTheRecord());
}

TEST_F(ChromePasswordManagerClientTest,
       SavingDependsOnManagerEnabledPreference) {
  // Test that saving passwords depends on the password manager enabled
  // preference.
  ChromePasswordManagerClient* client = GetClient();
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(true));
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest,
       FillingDependsOnManagerEnabledPreferenceAndExperimentEnabled) {
  // Test that filing of passwords depends on the password manager enabled
  // preference and is the user participated in behavior change experiment.
  ChromePasswordManagerClient* client = GetClient();
  EnforcePasswordManagerSettingsBehaviourChangeExperimentGroup(
      kPasswordManagerSettingsBehaviourChangeEnabledGroupName);
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(true));
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest,
       FillingDependsOnManagerEnabledPreferenceAndExperimentDisabled) {
  // Test that filing of passwords depends on the password manager enabled
  // preference and is the user participated in behavior change experiment.
  ChromePasswordManagerClient* client = GetClient();
  EnforcePasswordManagerSettingsBehaviourChangeExperimentGroup(
      kPasswordManagerSettingsBehaviourChangeDisabledGroupName);
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(true));
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(false));
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
}

TEST_F(ChromePasswordManagerClientTest, SavingAndFillingEnabledConditionsTest) {
  std::unique_ptr<MockChromePasswordManagerClient> client(
      new MockChromePasswordManagerClient(web_contents()));
  // Functionality disabled if there is SSL errors.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(true));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingEnabledForCurrentPage());

  // Functionality disabled if there are SSL errors and the manager itself is
  // disabled.
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_FALSE(client->IsFillingEnabledForCurrentPage());

  // Functionality disabled if there are no SSL errors, but the manager itself
  // is disabled.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(false));
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(false));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());

  // Functionality enabled if there are no SSL errors and the manager is
  // enabled.
  EXPECT_CALL(*client, DidLastPageLoadEncounterSSLErrors())
      .WillRepeatedly(Return(false));
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(true));
  EXPECT_TRUE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());

  // Functionality disabled in Incognito mode.
  profile()->ForceIncognito(true);
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());

  // Functionality disabled in Incognito mode also when manager itself is
  // enabled.
  prefs()->SetUserPref(password_manager::prefs::kPasswordManagerSavingEnabled,
                       new base::FundamentalValue(true));
  EXPECT_FALSE(client->IsSavingAndFillingEnabledForCurrentPage());
  EXPECT_TRUE(client->IsFillingEnabledForCurrentPage());
  profile()->ForceIncognito(false);
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL_Empty) {
  EXPECT_EQ(GURL::EmptyGURL(), GetClient()->GetLastCommittedEntryURL());
}

TEST_F(ChromePasswordManagerClientTest, GetLastCommittedEntryURL) {
  GURL kUrl(
      "https://accounts.google.com/ServiceLogin?continue="
      "https://passwords.google.com/settings");
  NavigateAndCommit(kUrl);
  EXPECT_EQ(kUrl, GetClient()->GetLastCommittedEntryURL());
}

TEST_F(ChromePasswordManagerClientTest, WebUINoLogging) {
  // Make sure that logging is active.
  password_manager::LogRouter* log_router = password_manager::
      PasswordManagerInternalsServiceFactory::GetForBrowserContext(profile());
  DummyLogReceiver log_receiver;
  EXPECT_EQ(std::string(), log_router->RegisterReceiver(&log_receiver));

  // But then navigate to a WebUI, there the logging should not be active.
  NavigateAndCommit(GURL("about:password-manager-internals"));
  EXPECT_FALSE(GetClient()->GetLogManager()->IsLoggingActive());

  log_router->UnregisterReceiver(&log_receiver);
}

// Metrics enabled, syncing with non-custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryWithMetricsNoCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  metrics_enabled_ = true;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics disabled, syncing with non-custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryNoMetricsNoCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(false));
  metrics_enabled_ = false;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics enabled, syncing with custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryWithMetricsWithCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  metrics_enabled_ = true;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Metrics disabled, syncing with custom passphrase: Do not annotate.
TEST_F(ChromePasswordManagerClientTest,
       AnnotateNavigationEntryNoMetricsWithCustom) {
  ProfileSyncServiceMock* mock_sync_service = SetupBasicMockSync();
  EXPECT_CALL(*mock_sync_service, IsUsingSecondaryPassphrase())
      .WillRepeatedly(Return(true));
  metrics_enabled_ = false;

  NavigateAndCommit(GURL("about:blank"));
  GetClient()->AnnotateNavigationEntry(true);

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: Unannotated
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryUnannotated) {
  SetupNavigationForAnnotation();

  EXPECT_EQ(
      SerializedNavigationEntry::PASSWORD_STATE_UNKNOWN,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: unknown->false
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToFalse) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::NO_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: false->true
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryToTrue) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(false);
  GetClient()->AnnotateNavigationEntry(true);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// State transition: true->false (retains true)
TEST_F(ChromePasswordManagerClientTest, AnnotateNavigationEntryTrueToFalse) {
  SetupNavigationForAnnotation();

  GetClient()->AnnotateNavigationEntry(true);
  GetClient()->AnnotateNavigationEntry(false);
  EXPECT_EQ(
      SerializedNavigationEntry::HAS_PASSWORD_FIELD,
      GetPasswordStateFromNavigation(*controller().GetLastCommittedEntry()));
}

// Handle missing ChromePasswordManagerClient instance in BindCredentialManager
// gracefully.
TEST_F(ChromePasswordManagerClientTest, BindCredentialManager_MissingInstance) {
  // Create a WebContent without tab helpers.
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(
          content::WebContents::CreateParams(profile())));
  // In particular, this WebContent should not have the
  // ChromePasswordManagerClient.
  ASSERT_FALSE(
      ChromePasswordManagerClient::FromWebContents(web_contents.get()));

  // This call should not crash.
  ChromePasswordManagerClient::BindCredentialManager(
      web_contents->GetMainFrame(),
      password_manager::mojom::CredentialManagerRequest());
}
