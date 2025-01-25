// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_IMPL_H_
#define CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_IMPL_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/boca/on_task/on_task_blocklist.h"
#include "chromeos/ash/components/boca/on_task/on_task_system_web_app_manager.h"
#include "url/gurl.h"

// Forward declaration of the browser profile and `SessionID`.
class Profile;
class SessionID;

namespace ash::boca {

// `OnTaskSystemWebAppManager` implementation that is essentially a thin wrapper
// around SWA window management APIs, specifically launch, close, and window
// pinning.
class OnTaskSystemWebAppManagerImpl : public OnTaskSystemWebAppManager {
 public:
  explicit OnTaskSystemWebAppManagerImpl(Profile* profile);
  ~OnTaskSystemWebAppManagerImpl() override;

  // OnTaskSystemWebAppManager:
  void LaunchSystemWebAppAsync(
      base::OnceCallback<void(bool)> callback) override;
  void CloseSystemWebAppWindow(SessionID window_id) override;
  SessionID GetActiveSystemWebAppWindowID() override;
  void SetPinStateForSystemWebAppWindow(bool pinned,
                                        SessionID window_id) override;
  void SetWindowTrackerForSystemWebAppWindow(SessionID window_id) override;
  void CreateBackgroundTabWithUrl(
      SessionID window_id,
      GURL url,
      OnTaskBlocklist::RestrictionLevel restriction_level) override;

 private:
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<OnTaskSystemWebAppManagerImpl> weak_ptr_factory_{this};
};

}  // namespace ash::boca

#endif  // CHROME_BROWSER_ASH_BOCA_ON_TASK_ON_TASK_SYSTEM_WEB_APP_MANAGER_IMPL_H_
