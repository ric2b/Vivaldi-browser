// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/arc_migration_guide_notification.h"

#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/account_id/account_id.h"
#include "components/user_manager/known_user.h"

namespace arc {

void ShowArcMigrationSuccessNotificationIfNeeded(Profile* profile) {
  const AccountId account_id =
      multi_user_util::GetAccountIdFromProfile(profile);

  int pref_value = kFileSystemIncompatible;
  user_manager::known_user::GetIntegerPref(
      account_id, prefs::kArcCompatibleFilesystemChosen, &pref_value);

  // Show notification only when the pref value indicates the file system is
  // compatible, but not yet notified.
  if (pref_value != kFileSystemCompatible)
    return;

  // TODO(kinaba): The acutual notificaiton is added here in M59.
  // For M58, this function is deployed just for maintaining the pref value.

  // Mark as notified.
  user_manager::known_user::SetIntegerPref(
      account_id, prefs::kArcCompatibleFilesystemChosen,
      arc::kFileSystemCompatibleAndNotified);
}

}  // namespace arc
