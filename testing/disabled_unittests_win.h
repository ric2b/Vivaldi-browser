// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved
#include "testing/disable_unittests_macros.h"

// List of unittest disabled for Windows by Vivaldi
// On the form
//    DISABLE(foo,bar)
//    DISABLE(foo,baz)

  // Seems broken on Win64
  DISABLE(ChildThreadImplBrowserTest, LockDiscardableMemory)

  //DISABLE(ExtensionFetchTest, ExtensionCannotFetchHostedResourceWithoutHostPermissions)

  // Flaky on Windows, fails on tester and works on local machine
  //DISABLE(ExtensionSettingsApiTest, ExtensionsSchemas)

  // Flaky on Windows, fails on tester and works on local machine
  DISABLE(LocalFileSyncServiceTest, LocalChangeObserver)

  //DISABLE(PluginPowerSaverBrowserTest, PluginMarkedEssentialAfterPosterClicked)

  //DISABLE(SyncFileSystemTest, AuthorizationTest)

  // Flaky in v50
  DISABLE_ALL(TabDesktopMediaListTest)

  //DISABLE(UnloadTest, BrowserCloseInfiniteUnload)
