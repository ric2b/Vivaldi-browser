// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_MIGRATION_GUIDE_NOTIFICATION_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_MIGRATION_GUIDE_NOTIFICATION_H_

class Profile;

namespace arc {

// TODO(kinaba): Will show a one-time notification for successful migration.
// On M58 it just updates the pref value regarding this behavior, so that
// the update to M59 is not confused by the unset pref.
void ShowArcMigrationSuccessNotificationIfNeeded(Profile* profile);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_MIGRATION_GUIDE_NOTIFICATION_H_
