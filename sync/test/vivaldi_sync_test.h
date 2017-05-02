// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#include "chrome/browser/sync/test/integration/sync_test.h"

class VivaldiProfileSyncServiceHarness;

class VivaldiSyncTest : public SyncTest {
 public:
  VivaldiSyncTest(TestType test_type);
  ~VivaldiSyncTest() override;
  VivaldiProfileSyncServiceHarness* GetClient(int index) WARN_UNUSED_RESULT;

  void SetUp() override;
  void TearDown() override;
};
