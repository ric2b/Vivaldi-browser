// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Mac and Linux by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

// Does not work for vivaldi
DISABLE(StartupBrowserCreatorFirstRunTest, FirstRunTabsWithRestoreSession)
DISABLE(StartupBrowserCreatorFirstRunTest, RestoreOnStartupURLsPolicySpecified)
DISABLE(ProfileSigninConfirmationHelperBrowserTest, HasNotBeenShutdown)

// Disabled in Google Chrome official builds
DISABLE(FirstRunMasterPrefsImportNothing, ImportNothingAndShowNewTabPage)
