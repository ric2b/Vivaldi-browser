// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/supervised_user/api_mock_setup_mixin.h"

#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/supervised_user/child_account_test_utils.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/supervised_user/core/common/pref_names.h"
#include "components/supervised_user/core/common/supervised_user_constants.h"
#include "components/supervised_user/test_support/kids_management_api_server_mock.h"
#include "net/dns/mock_host_resolver.h"

namespace supervised_user {

namespace {
constexpr std::string_view kKidsManagementServiceEndpoint{
    "kidsmanagement.googleapis.com"};

// Self-consistent conditional RAII lock on list family members load.
//
// Registers to observe a preference and blocks until it is loaded for
// *supervised users* (see ~FamilyFetchedLock() and IsSupervisedProfile()).
// Effectivelly, halts the main testing thread until the first fetch of list
// family members has finished, which is typically invoked by the browser after
// startup of the SupervisedUserService.
//
// For non-supervised users, this is no-op (it just registers and unregisters a
// preference observer).
class FamilyFetchedLock {
 public:
  FamilyFetchedLock() = delete;
  FamilyFetchedLock(raw_ptr<InProcessBrowserTest> test_base,
                    std::string_view custodian_pref)
      : test_base_(test_base) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    Profile* profile = ProfileManager::GetActiveUserProfile();
#else
    Profile* profile = test_base_->browser()->profile();
#endif
    CHECK(profile) << "Must be acquitted and initialized after the profile was "
                      "initialized too.";
    pref_change_registrar_.Init(GetPrefService());
    pref_change_registrar_.Add(
        std::string(custodian_pref),
        base::BindRepeating(&FamilyFetchedLock::OnPreferenceRegistered,
                            base::Unretained(this)));
  }
  ~FamilyFetchedLock() {
    if (IsSupervisedProfile()) {
      base::RunLoop run_loop;
      done_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    pref_change_registrar_.RemoveAll();
  }

  FamilyFetchedLock(const FamilyFetchedLock&) = delete;
  FamilyFetchedLock& operator=(const FamilyFetchedLock&) = delete;

 private:
  void OnPreferenceRegistered() { std::move(done_).Run(); }

  // Profile::AsTestingProfile won't return TestingProfile at this stage of
  // setup, so TestingProfile::IsChild is not available yet.
  bool IsSupervisedProfile() const {
    return GetPrefService()->GetString(prefs::kSupervisedUserId) ==
           supervised_user::kChildAccountSUID;
  }

  PrefService* GetPrefService() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    return ProfileManager::GetActiveUserProfile()->GetPrefs();
#else
    return test_base_->browser()->profile()->GetPrefs();
#endif
  }

  raw_ptr<InProcessBrowserTest> test_base_;
  base::OnceClosure done_;
  PrefChangeRegistrar pref_change_registrar_;
};
}  // namespace

KidsManagementApiMockSetupMixin::KidsManagementApiMockSetupMixin(
    InProcessBrowserTestMixinHost& host,
    InProcessBrowserTest* test_base)
    : InProcessBrowserTestMixin(&host), test_base_(test_base) {
  SetHttpEndpointsForKidsManagementApis(feature_list_,
                                        kKidsManagementServiceEndpoint);
}
KidsManagementApiMockSetupMixin::~KidsManagementApiMockSetupMixin() = default;

void KidsManagementApiMockSetupMixin::SetUp() {
  api_mock_.InstallOn(embedded_test_server_);
  CHECK(embedded_test_server_.InitializeAndListen());
}

void KidsManagementApiMockSetupMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  AddHostResolverRule(command_line, kKidsManagementServiceEndpoint,
                      embedded_test_server_);
}

void KidsManagementApiMockSetupMixin::SetUpOnMainThread() {
  // If expected, halts test until initial fetch is completed.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = ProfileManager::GetActiveUserProfile();
#else
  Profile* profile = test_base_->browser()->profile();
#endif
  CHECK(profile);
  // Waits for families to load until the requested preference
  // (kSupervisedUserCustodianName) has been fetched, if it's
  // not already present.
  // Guarantees that when `SetUpOnMainThread` terminates all
  // all preconditions have been fetched.
  std::unique_ptr<FamilyFetchedLock> optional_conditional_lock;

  if (test_base_->browser()
          ->profile()
          ->GetPrefs()
          ->GetString(prefs::kSupervisedUserCustodianName)
          .empty()) {
    optional_conditional_lock = std::make_unique<FamilyFetchedLock>(
        test_base_, prefs::kSupervisedUserCustodianName);
  }
  embedded_test_server_.StartAcceptingConnections();
}

void KidsManagementApiMockSetupMixin::TearDownOnMainThread() {
  CHECK(embedded_test_server_.ShutdownAndWaitUntilComplete());
}

}  // namespace supervised_user
