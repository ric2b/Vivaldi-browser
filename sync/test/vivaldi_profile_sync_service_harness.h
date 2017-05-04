// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/vivaldi_syncmanager_factory.h"

// Substitute these class and test names with Vivaldi ones
#define ProfileSyncServiceFactory vivaldi::VivaldiSyncManagerFactory
#define ProfileSyncServiceHarness VivaldiProfileSyncServiceHarness
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_FACTORY_H_

#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
