// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "sync/test/vivaldi_sync_test.h"

#undef SyncTest

#include "app/vivaldi_apptools.h"
#include "sync/test/vivaldi_profile_sync_service_harness.h"

VivaldiSyncTest::VivaldiSyncTest(TestType test_type) : SyncTest(test_type) {}

VivaldiSyncTest::~VivaldiSyncTest() {}

void VivaldiSyncTest::SetUp() {
  vivaldi::ForceVivaldiRunning(true);
  SyncTest::SetUp();
}

void VivaldiSyncTest::TearDown() {
  SyncTest::TearDown();
  vivaldi::ForceVivaldiRunning(false);
}

VivaldiProfileSyncServiceHarness* VivaldiSyncTest::GetClient(int index) {
  return reinterpret_cast<VivaldiProfileSyncServiceHarness*>(
      SyncTest::GetClient(index));
}
