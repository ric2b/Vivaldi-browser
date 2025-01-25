// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_
#define CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/types/strong_alias.h"
#include "chrome/test/supervised_user/family_member.h"
#include "components/supervised_user/core/browser/fetcher_config.h"
#include "components/supervised_user/core/browser/proto/kidsmanagement_messages.pb.h"
#include "components/supervised_user/core/browser/proto_fetcher.h"
#include "components/supervised_user/core/browser/supervised_user_service.h"
#include "components/supervised_user/core/browser/supervised_user_service_observer.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "ui/base/interaction/polling_state_observer.h"

namespace supervised_user {

SupervisedUserService* GetSupervisedUserService(const FamilyMember& member);

// State of a Family Link toggle.
enum class FamilyLinkToggleState : bool {
  kEnabled = true,
  kDisabled = false,
};

// Toggles provided in the FL Advanced Settings parental controls.
enum class FamilyLinkToggleType : int {
  kPermissionsToggle = 0,
  kExtensionsToggle = 1,
  kCookiesToggle = 2
};

// Configured Family Link toggle.
struct FamilyLinkToggleConfiguration {
  const FamilyLinkToggleType type;
  const FamilyLinkToggleState state;
};

// Creates requests and conditions associated with given state.
class BrowserState {
 public:
  using Observer = ui::test::PollingStateObserver<bool>;

  // Represents intended state of the supervised user service to achieve.
  // It both knows what request to send to get to that state (::GetRequest()),
  // and how to check whether the service is in that state
  // (::Check()).
  class Intent {
   public:
    virtual ~Intent() = 0;

    // This intent represented as serialized proto request.
    virtual std::string GetRequest() const = 0;

    // Configuration for RPC call for this intent.
    virtual const FetcherConfig& GetConfig() const = 0;

    // Textual representation of this intent for debugging purposes.
    virtual std::string ToString() const = 0;

    // Function that is checking `browser_user`'s browser whether it is in the
    // intended state.
    virtual bool Check(const FamilyMember& browser_user) const = 0;
  };

  // Resets the state to defaults.
  class ResetIntent : public Intent {
   public:
    ~ResetIntent() override;

    // Intent
    std::string GetRequest() const override;
    const FetcherConfig& GetConfig() const override;
    std::string ToString() const override;
    bool Check(const FamilyMember& browser_user) const override;
  };

  // Defines safe sites configuration.
  class DefineManualSiteListIntent : public Intent {
   public:
    using AllowUrl = base::StrongAlias<class AllowUrlTag, GURL>;
    using BlockUrl = base::StrongAlias<class BlockUrlTag, GURL>;

    DefineManualSiteListIntent();
    explicit DefineManualSiteListIntent(AllowUrl url);
    explicit DefineManualSiteListIntent(BlockUrl url);
    ~DefineManualSiteListIntent() override;

    // Intent
    std::string GetRequest() const override;
    const FetcherConfig& GetConfig() const override;
    std::string ToString() const override;
    bool Check(const FamilyMember& browser_user) const override;

   private:
    std::optional<GURL> allowed_url_;
    std::optional<GURL> blocked_url_;
  };

  // Defines configuration for a list of given boolean toggles.
  class ToggleIntent : public Intent {
   public:
    explicit ToggleIntent(std::list<FamilyLinkToggleConfiguration> toggle_list);
    ~ToggleIntent() override;

    // Intent implementation:
    std::string GetRequest() const override;
    const FetcherConfig& GetConfig() const override;
    std::string ToString() const override;
    bool Check(const FamilyMember& browser_user) const override;

   private:
    std::list<FamilyLinkToggleConfiguration> toggle_list_;
  };

  // Use those static constructors to request state as indicated by name.
  // Clears url filter lists and filter settings to server-side defaults. After
  // issuing, url filter lists are empty. FilteringLevel is unset.
  static BrowserState Reset();
  // After issuing, FilteringLevel is set to SAFE_SITES
  static BrowserState EnableSafeSites();
  // After issuing, FilteringLevel is set to SAFE_SITES and gurl is added to
  // allow list of filtered urls.
  static BrowserState AllowSite(const GURL& gurl);
  // After issuing, FilteringLevel is set to SAFE_SITES and gurl is added to
  // block list of filtered urls.
  static BrowserState BlockSite(const GURL& gurl);
  // Sets the Advanced Setting toggles (Permissions, Extensions, Cookies) to
  // their default values.
  static BrowserState SetAdvancedSettingsDefault();
  // After issuing, Permissions, Extensions and Cookies toggles are set to the
  // given values, if such a value is provided on the input list.
  static BrowserState AdvancedSettingsToggles(
      std::list<FamilyLinkToggleConfiguration> toggle_list);

  ~BrowserState();

  // Tests whether the browser is in the intended state. The state is checked
  // for `member`'s browser, which typically should be the child.
  bool Check(const FamilyMember& browser_user) const;

  // Seeds the `target_state_` by issuing a RPC.
  void Seed(const FamilyMember& supervising_user,
            const FamilyMember& browser_user) const;

  // Textual representation of this instance (for logging).
  std::string ToString() const;

 private:
  explicit BrowserState(const Intent* intent);
  std::unique_ptr<const Intent> intent_;
};

}  // namespace supervised_user

#endif  // CHROME_TEST_SUPERVISED_USER_TEST_STATE_SEEDED_OBSERVER_H_
