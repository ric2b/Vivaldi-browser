// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/test/vivaldi_profile_sync_service_harness.h"
#include "sync/test/vivaldi_sync_test.h"

// Substitute these class and test names with Vivaldi ones
#define TwoClientBookmarksSyncTest VivaldiTwoClientBookmarksSyncTest
#define LegacyTwoClientBookmarksSyncTest VivaldiLegacyTwoClientBookmarksSyncTest
#define SyncTest VivaldiSyncTest

#include "chromium/chrome/browser/sync/test/integration/two_client_bookmarks_sync_test.cc"  // NOLINT
