// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/test/vivaldi_profile_sync_service_harness.h"
#include "sync/test/vivaldi_sync_test.h"

// Needed to disable a group of tests because the define renaming does not work
// for those tests
#define VIVALDI_RENAMED_SYNC_CLIENT_TESTS

// Substitute these class and test names with Vivaldi ones
#define SingleClientBookmarksSyncTest VivaldiSingleClientBookmarksSyncTest
#define SingleClientBookmarksSyncTestIncludingUssTests \
  VivaldiSingleClientBookmarksSyncTestIncludingUssTests
#define SyncTest VivaldiSyncTest

#include "chromium/chrome/browser/sync/test/integration/single_client_bookmarks_sync_test.cc"  // NOLINT
