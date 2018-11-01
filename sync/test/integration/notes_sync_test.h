// Copyright (c) 2015 Vivaldi Technologies AS. All rights reserved

#ifndef SYNC_TEST_INTEGRATION_NOTES_SYNC_TEST_H_
#define SYNC_TEST_INTEGRATION_NOTES_SYNC_TEST_H_

#include "chrome/browser/sync/test/integration/sync_test.h"

class NotesSyncTest : public SyncTest {
 public:
  explicit NotesSyncTest(TestType test_type);
  ~NotesSyncTest() override;

  bool SetupClients() override;

  // Verify that the local notes model (for the Profile corresponding to
  // |index|) matches the data on the FakeServer. It is assumed that FakeServer
  // is being used and each notes has a unique title. Folders are not
  // verified.
  void VerifyNotesModelMatchesFakeServer(int index);

 protected:
  void WaitForDataModels(Profile* profile) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotesSyncTest);
};

#endif  // SYNC_TEST_INTEGRATION_NOTES_SYNC_TEST_H_
