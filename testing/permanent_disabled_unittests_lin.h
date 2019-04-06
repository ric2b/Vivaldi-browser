// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Linux by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// Disabled in Google Chrome official builds
//DISABLE(FirstRunMasterPrefsImportBookmarksFile, ImportBookmarksFile)

// Disabled in Google Chrome official builds
//DISABLE(FirstRunMasterPrefsImportDefault, ImportDefault)

// Disabled in Google Chrome official builds
//DISABLE_MULTI(FirstRunMasterPrefsWithTrackedPreferences,
//              TrackedPreferencesSurviveFirstRun)

// Disabled because the Widevine lib is not included in the package build,
// as it crashes vivaldi
DISABLE(PepperContentSettingsSpecialCasesPluginsBlockedTest, WidevineCdm)

// Does not work for vivaldi
//DISABLE(ProfileSigninConfirmationHelperBrowserTest, HasNoSyncedExtensions)

// Does not work for vivaldi
//DISABLE(StartupBrowserCreatorFirstRunTest,
//        FirstRunTabsContainNTPSyncPromoAllowed)
//DISABLE(StartupBrowserCreatorFirstRunTest,
//        FirstRunTabsContainNTPSyncPromoForbidden)
//DISABLE(StartupBrowserCreatorFirstRunTest, FirstRunTabsContainSyncPromo)
//DISABLE(StartupBrowserCreatorFirstRunTest, FirstRunTabsPromoAllowed)
//DISABLE(StartupBrowserCreatorFirstRunTest, FirstRunTabsSyncPromoForbidden)
//DISABLE(StartupBrowserCreatorFirstRunTest, SyncPromoAllowed)
//DISABLE(StartupBrowserCreatorFirstRunTest, SyncPromoForbidden)

