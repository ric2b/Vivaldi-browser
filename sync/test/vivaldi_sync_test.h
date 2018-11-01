// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_TEST_VIVALDI_SYNC_TEST_H_
#define SYNC_TEST_VIVALDI_SYNC_TEST_H_

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

#endif  // SYNC_TEST_VIVALDI_SYNC_TEST_H_
