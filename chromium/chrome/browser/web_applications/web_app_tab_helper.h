// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

namespace web_app {

class WebAppProvider;
class WebAppLaunchQueue;

// Per-tab web app helper. Allows to associate a tab (web page) with a web app.
class WebAppTabHelper : public content::WebContentsUserData<WebAppTabHelper>,
                        public content::WebContentsObserver,
                        public WebAppInstallManagerObserver {
 public:
  using content::WebContentsUserData<WebAppTabHelper>::CreateForWebContents;

  // Retrieves the WebAppTabHelper's app ID off |web_contents|, returns
  // nullptr if there is no tab helper or app ID.
  static const webapps::AppId* GetAppId(
      const content::WebContents* web_contents);

#if BUILDFLAG(IS_MAC)
  // Like the above method, but also checks if notification attribution should
  // apply to the app in the web contents. This checks the base::Feature as well
  // as makes sure the app is installed.
  static std::optional<webapps::AppId> GetAppIdForNotificationAttribution(
      content::WebContents* web_contents);
#endif

  WebAppTabHelper(const WebAppTabHelper&) = delete;
  WebAppTabHelper& operator=(const WebAppTabHelper&) = delete;
  ~WebAppTabHelper() override;

  // Sets the app id for this web contents. Ideally the app id would always be
  // equal to the id of whatever app the last committed primary main frame URL
  // is in scope for (and WebAppTabHelper resets it to that any time a
  // navigation commits), but for legacy reasons sometimes the app id is set
  // explicitly from elsewhere.
  void SetAppId(std::optional<webapps::AppId> app_id);

  // Called by `WebAppBrowserController::OnTabInserted` and `OnTabRemoved` to
  // indicate if this web contents is currently being displayed inside an app
  // window.
  void SetIsInAppWindow(bool is_in_app_window);

  // True when this web contents is currently being displayed inside an app
  // window instead of in a browser tab.
  bool is_in_app_window() const { return is_in_app_window_; }

  const base::UnguessableToken& GetAudioFocusGroupIdForTesting() const;

  const std::optional<webapps::AppId> app_id() const { return app_id_; }

  // Returns if this web contents was from an app-like launch from the OS, or if
  // it was ever in an app window. This is used to determine if app settings
  // should be shown in the page controls panel.
  bool acting_as_app() const { return acting_as_app_; }
  void set_acting_as_app(bool acting_as_app) { acting_as_app_ = acting_as_app; }

  bool is_pinned_home_tab() const { return is_pinned_home_tab_; }
  void set_is_pinned_home_tab(bool is_pinned_home_tab) {
    is_pinned_home_tab_ = is_pinned_home_tab;
  }

  WebAppLaunchQueue& EnsureLaunchQueue();

  // content::WebContentsObserver:
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryPageChanged(content::Page& page) override;
  void DidCloneToNewWebContents(
      content::WebContents* old_web_contents,
      content::WebContents* new_web_contents) override;

 private:
  friend class WebAppAudioFocusBrowserTest;
  friend class content::WebContentsUserData<WebAppTabHelper>;

  explicit WebAppTabHelper(content::WebContents* web_contents);

  // WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& installed_app_id) override;
  void OnWebAppWillBeUninstalled(
      const webapps::AppId& uninstalled_app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  void ResetAppId();

  // Sets the state of this tab helper. This will call
  // `WebAppUiManager::OnAssociatedAppChanged` if the id has changed, and
  // `UpdateAudioFocusGroupId()` if either has changed.
  void SetState(std::optional<webapps::AppId> app_id, bool is_in_app_window);

  // Runs any logic when the associated app is added, changed or removed.
  void OnAssociatedAppChanged(
      const std::optional<webapps::AppId>& previous_app_id,
      const std::optional<webapps::AppId>& new_app_id);

  // Updates the audio focus group id based on the current web app.
  void UpdateAudioFocusGroupId();

  // Triggers a reinstall of a placeholder app for |url|.
  void ReinstallPlaceholderAppIfNecessary(const GURL& url);

  std::optional<webapps::AppId> FindAppWithUrlInScope(const GURL& url) const;

  // WebApp associated with this tab.
  std::optional<webapps::AppId> app_id_;

  // True when the associated `WebContents` is acting as an app. Specifically,
  // this should only be true if `app_id_` is non empty, and the WebContents was
  // created in response to an app launch, or in some other corner cases such as
  // when an app is first installed and reparented from tab to window. It should
  // be false if a user types the app's URL into a normal browser window.
  bool acting_as_app_ = false;

  bool is_in_app_window_ = false;

  // True when this tab is the pinned home tab of a tabbed web app.
  bool is_pinned_home_tab_ = false;

  // The audio focus group id is used to group media sessions together for apps.
  // We store the applied group id locally on the helper for testing.
  base::UnguessableToken audio_focus_group_id_ = base::UnguessableToken::Null();

  // Use unique_ptr for lazy instantiation as most browser tabs have no need to
  // incur this memory overhead.
  std::unique_ptr<WebAppLaunchQueue> launch_queue_;

  base::ScopedObservation<WebAppInstallManager, WebAppInstallManagerObserver>
      observation_{this};
  raw_ptr<WebAppProvider> provider_ = nullptr;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_TAB_HELPER_H_
