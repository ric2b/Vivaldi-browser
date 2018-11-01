// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "sync/test/integration/notes_helper.h"
#include "sync/test/integration/notes_sync_test.h"
#include "ui/base/layout.h"

using notes_helper::AddFolder;
using notes_helper::AddNote;
using notes_helper::CountNotesWithTitlesMatching;
using notes_helper::GetNotesModel;
using notes_helper::GetNotesTopNode;
using notes_helper::HasNodeWithURL;
using notes_helper::ModelMatchesVerifier;
using notes_helper::AllModelsMatchVerifier;
using notes_helper::AllModelsMatch;
using notes_helper::Move;
using notes_helper::Remove;
using notes_helper::RemoveAll;
using notes_helper::SetTitle;
using notes_helper::SetURL;
using notes_helper::GetUniqueNodeByURL;
using notes_helper::SortChildren;
using notes_helper::ReverseChildOrder;
using notes_helper::IndexedURL;
using notes_helper::IndexedURLTitle;
using notes_helper::IndexedFolderName;
using notes_helper::IndexedSubfolderName;
using notes_helper::IndexedSubsubfolderName;
using notes_helper::ContainsDuplicateNotes;

static const char kGenericURL[] = "http://www.host.ext:1234/path/filename";
static const char kGenericURLTitle[] = "URL Title";
static const char kGenericFolderName[] = "Folder Name";
static const char kGenericSubfolderName[] = "Subfolder Name";

class TwoClientNotesSyncTest : public NotesSyncTest {
 public:
  TwoClientNotesSyncTest() : NotesSyncTest(TWO_CLIENT) {}
  ~TwoClientNotesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientNotesSyncTest);
};

class LegacyTwoClientNotesSyncTest : public NotesSyncTest {
 public:
  LegacyTwoClientNotesSyncTest() : NotesSyncTest(TWO_CLIENT_LEGACY) {}
  ~LegacyTwoClientNotesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(LegacyTwoClientNotesSyncTest);
};

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL google_url("http://www.google.com");
  ASSERT_TRUE(AddNote(0, "Google", google_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AddNote(1, "Yahoo", GURL("http://www.yahoo.com")) != NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Notes_Node* trash_node_0 = GetNotesModel(0)->trash_node();
  Notes_Node* trash_node_1 = GetNotesModel(1)->trash_node();
  ASSERT_TRUE(AddNote(0, trash_node_0, 0, "trash_1_url0",
                      GURL("http://www.microsoft.com")));
  ASSERT_EQ(1, trash_node_0->child_count());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_EQ(1, trash_node_1->child_count());
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* new_folder = AddFolder(0, 2, "New Folder");
  Move(0, GetUniqueNodeByURL(0, google_url), new_folder, 0);
  SetTitle(0, GetNotesTopNode(0)->GetChild(0), "Yahoo!!");
  ASSERT_TRUE(AddNote(0, GetNotesTopNode(0), 1, "CNN",
                      GURL("http://www.cnn.com")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(1, "Facebook", GURL("http://www.facebook.com")) != NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(1, trash_node_1, 0);
  ASSERT_EQ(0, trash_node_1->child_count());
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_EQ(0, trash_node_0->child_count());
  ASSERT_TRUE(AllModelsMatchVerifier());

  SortChildren(1, GetNotesTopNode(1));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  DisableVerifier();
  SetTitle(0, GetUniqueNodeByURL(0, google_url), "Google++");
  SetTitle(1, GetUniqueNodeByURL(1, google_url), "Google--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SimultaneousURLChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL initial_url("http://www.google.com");
  GURL second_url("http://www.google.com/abc");
  GURL third_url("http://www.google.com/def");
  std::string title = "Google";

  ASSERT_TRUE(AddNote(0, title, initial_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  DisableVerifier();
  ASSERT_TRUE(SetURL(0, GetUniqueNodeByURL(0, initial_url), second_url) !=
              NULL);
  ASSERT_TRUE(SetURL(1, GetUniqueNodeByURL(1, initial_url), third_url) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  SetTitle(0, GetNotesTopNode(0)->GetChild(0), "Google1");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370558.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_AddFirstFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370559.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_AddFirstBMWithoutFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370489.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_AddFirstBMWithFavicon) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const GURL page_url(kGenericURL);

  const Notes_Node* note = AddNote(0, kGenericURLTitle, page_url);

  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370560.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_AddNonHTTPBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, "FTP URL",
                      GURL("ftp://user:password@host:1234/path")) != NULL);
  ASSERT_TRUE(AddNote(0, "File URL", GURL("file://host/path")) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370561.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_AddFirstBMUnderFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  ASSERT_TRUE(AddNote(0, folder, 0, kGenericURLTitle, GURL(kGenericURL)) !=
              NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370562.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_AddSeveralBMsUnderBMBarAndOtherBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 20; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
    ASSERT_TRUE(AddNote(0, GetNotesTopNode(0), i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370563.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_AddSeveralBMsAndFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      const Notes_Node* folder = AddFolder(0, i, title);
      ASSERT_TRUE(folder != NULL);
      if (base::RandDouble() > 0.4) {
        for (int i = 0; i < 20; ++i) {
          std::string title = IndexedURLTitle(i);
          GURL url = GURL(IndexedURL(i));
          ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
        }
      }
    }
  }
  for (int i = 0; i < 10; i++) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, GetNotesTopNode(0), i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370641.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DuplicateBMWithDifferentURLSameName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url0 = GURL(IndexedURL(0));
  GURL url1 = GURL(IndexedURL(1));
  ASSERT_TRUE(AddNote(0, kGenericURLTitle, url0) != NULL);
  ASSERT_TRUE(AddNote(0, kGenericURLTitle, url1) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 370639 - Add notes with different name and same URL.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_DuplicateNotesWithSameURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title0 = IndexedURLTitle(0);
  std::string title1 = IndexedURLTitle(1);
  ASSERT_TRUE(AddNote(0, title0, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(0, title1, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371817.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_RenameBMName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedURLTitle(1);
  const Notes_Node* note = AddNote(0, title, GURL(kGenericURL));
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedURLTitle(2);
  SetTitle(0, note, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371822.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_RenameBMURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  const Notes_Node* note = AddNote(0, kGenericURLTitle, url);
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL new_url = GURL(IndexedURL(2));
  ASSERT_TRUE(SetURL(0, note, new_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371818 - Renaming the same note name twice.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_TwiceRenamingNoteName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedURLTitle(1);
  const Notes_Node* note = AddNote(0, title, GURL(kGenericURL));
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedURLTitle(2);
  SetTitle(0, note, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  SetTitle(0, note, title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371823 - Renaming the same note URL twice.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_TwiceRenamingNoteURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  const Notes_Node* note = AddNote(0, kGenericURLTitle, url);
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL new_url = GURL(IndexedURL(2));
  ASSERT_TRUE(SetURL(0, note, new_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(SetURL(0, note, url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371824.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_RenameBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedFolderName(1);
  const Notes_Node* folder = AddFolder(0, title);
  ASSERT_TRUE(AddNote(0, folder, 0, kGenericURLTitle, GURL(kGenericURL)) !=
              NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371825.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_RenameEmptyBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedFolderName(1);
  const Notes_Node* folder = AddFolder(0, title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371826.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_RenameBMFolderWithLongHierarchy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string title = IndexedFolderName(1);
  const Notes_Node* folder = AddFolder(0, title);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
    } else {
      std::string title = IndexedSubfolderName(i);
      ASSERT_TRUE(AddFolder(0, folder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371827.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_RenameBMFolderThatHasParentAndChildren) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 1; i < 15; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
  }
  std::string title = IndexedSubfolderName(1);
  const Notes_Node* subfolder = AddFolder(0, folder, 0, title);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, subfolder, i, title, url) != NULL);
    } else {
      std::string title = IndexedSubsubfolderName(i);
      ASSERT_TRUE(AddFolder(0, subfolder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  std::string new_title = IndexedSubfolderName(2);
  SetTitle(0, subfolder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371828.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_RenameBMNameAndURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url = GURL(IndexedURL(1));
  std::string title = IndexedURLTitle(1);
  const Notes_Node* note = AddNote(0, title, url);
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL new_url = GURL(IndexedURL(2));
  std::string new_title = IndexedURLTitle(2);
  note = SetURL(0, note, new_url);
  ASSERT_TRUE(note != NULL);
  SetTitle(0, note, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371832.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DeleteBMEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371833.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelBMNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 20; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371835.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelFirstBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, folder, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371836.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelLastBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, folder, folder->child_count() - 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371856.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelMiddleBMUnderBMFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, folder, 4);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371857.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelBMsUnderBMFoldEmptyFolderAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  int child_count = folder->child_count();
  for (int i = 0; i < child_count; ++i) {
    Remove(0, folder, 0);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371858.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelEmptyBMFoldEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371869.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelEmptyBMFoldNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  for (int i = 1; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371879.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelBMFoldWithBMsNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const Notes_Node* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  for (int i = 0; i < 15; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetNotesTopNode(0), 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371880.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelBMFoldWithBMsAndBMFoldsNonEmptyACAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const Notes_Node* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  for (int i = 0; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
    } else {
      std::string title = IndexedSubfolderName(i);
      const Notes_Node* subfolder = AddFolder(0, folder, i, title);
      ASSERT_TRUE(subfolder != NULL);
      if (base::RandDouble() > 0.3) {
        for (int j = 0; j < 10; ++j) {
          if (base::RandDouble() > 0.6) {
            std::string title = IndexedURLTitle(j);
            GURL url = GURL(IndexedURL(j));
            ASSERT_TRUE(AddNote(0, subfolder, j, title, url) != NULL);
          } else {
            std::string title = IndexedSubsubfolderName(j);
            ASSERT_TRUE(AddFolder(0, subfolder, j, title) != NULL);
          }
        }
      }
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, GetNotesTopNode(0), 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371882.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_DelBMFoldWithParentAndChildrenBMsAndBMFolds) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 1; i < 11; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
  }
  const Notes_Node* subfolder = AddFolder(0, folder, 0, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 30; ++i) {
    if (base::RandDouble() > 0.2) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, subfolder, i, title, url) != NULL);
    } else {
      std::string title = IndexedSubsubfolderName(i);
      ASSERT_TRUE(AddFolder(0, subfolder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Remove(0, folder, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371931.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_ReverseTheOrderOfTwoBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url0 = GURL(IndexedURL(0));
  GURL url1 = GURL(IndexedURL(1));
  std::string title0 = IndexedURLTitle(0);
  std::string title1 = IndexedURLTitle(1);
  const Notes_Node* note0 = AddNote(0, 0, title0, url0);
  const Notes_Node* note1 = AddNote(0, 1, title1, url1);
  ASSERT_TRUE(note0 != NULL);
  ASSERT_TRUE(note1 != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Move(0, note0, GetNotesTopNode(0), 2);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371933.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_ReverseTheOrderOf10BMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ReverseChildOrder(0, GetNotesTopNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371954.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_MovingBMsFromBMBarToBMFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const Notes_Node* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  int num_notes_to_move = GetNotesTopNode(0)->child_count() - 2;
  for (int i = 0; i < num_notes_to_move; ++i) {
    Move(0, GetNotesTopNode(0)->GetChild(2), folder, i);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(AllModelsMatchVerifier());
  }
}

// Test Scribe ID - 371957.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_MovingBMsFromBMFoldToBMBar) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const Notes_Node* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  int num_notes_to_move = folder->child_count() - 2;
  for (int i = 0; i < num_notes_to_move; ++i) {
    Move(0, folder->GetChild(0), GetNotesTopNode(0), i);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(AllModelsMatchVerifier());
  }
}

// Test Scribe ID - 371961.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_MovingBMsFromParentBMFoldToChildBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  const Notes_Node* subfolder = AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i + 3);
    GURL url = GURL(IndexedURL(i + 3));
    ASSERT_TRUE(AddNote(0, subfolder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 3; ++i) {
    GURL url = GURL(IndexedURL(i));
    Move(0, GetUniqueNodeByURL(0, url), subfolder, i + 10);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371964.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_MovingBMsFromChildBMFoldToParentBMFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  const Notes_Node* subfolder = AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 5; ++i) {
    std::string title = IndexedURLTitle(i + 3);
    GURL url = GURL(IndexedURL(i + 3));
    ASSERT_TRUE(AddNote(0, subfolder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 3; ++i) {
    GURL url = GURL(IndexedURL(i + 3));
    Move(0, GetUniqueNodeByURL(0, url), folder, i + 4);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371967.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_HoistBMs10LevelUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = GetNotesTopNode(0);
  const Notes_Node* folder_L0 = NULL;
  const Notes_Node* folder_L10 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 0)
      folder_L0 = folder;
    if (level == 10)
      folder_L10 = folder;
  }
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i + 10);
    GURL url = GURL(IndexedURL(i + 10));
    ASSERT_TRUE(AddNote(0, folder_L10, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(0, url10), folder_L0, folder_L0->child_count());
  GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L0, 0);
  GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L0, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371968.
// Flaky. http://crbug.com/107744.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_SinkBMs10LevelDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = GetNotesTopNode(0);
  const Notes_Node* folder_L0 = NULL;
  const Notes_Node* folder_L10 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 0)
      folder_L0 = folder;
    if (level == 10)
      folder_L10 = folder;
  }
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i + 10);
    GURL url = GURL(IndexedURL(i + 10));
    ASSERT_TRUE(AddNote(0, folder_L0, 0, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(0, url10), folder_L10, folder_L10->child_count());
  GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L10, 0);
  GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L10, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371980.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_SinkEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = GetNotesTopNode(0);
  const Notes_Node* folder_L5 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5)
      folder_L5 = folder;
  }
  folder = AddFolder(0, GetNotesTopNode(0)->child_count(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Move(0, folder, folder_L5, folder_L5->child_count());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 371997.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_SinkNonEmptyBMFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = GetNotesTopNode(0);
  const Notes_Node* folder_L5 = NULL;
  for (int level = 0; level < 6; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5)
      folder_L5 = folder;
  }
  folder = AddFolder(0, GetNotesTopNode(0)->child_count(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Move(0, folder, folder_L5, folder_L5->child_count());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 372006.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SC_HoistFolder5LevelsUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  const Notes_Node* folder = GetNotesTopNode(0);
  const Notes_Node* folder_L5 = NULL;
  for (int level = 0; level < 6; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->child_count(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5)
      folder_L5 = folder;
  }
  folder =
      AddFolder(0, folder_L5, folder_L5->child_count(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  Move(0, folder, GetNotesTopNode(0), GetNotesTopNode(0)->child_count());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 372026.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_ReverseTheOrderOfTwoBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 2; ++i) {
    std::string title = IndexedFolderName(i);
    const Notes_Node* folder = AddFolder(0, i, title);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 10; ++j) {
      std::string title = IndexedURLTitle(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddNote(0, folder, j, title, url) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ReverseChildOrder(0, GetNotesTopNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 372028.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SC_ReverseTheOrderOfTenBMFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedFolderName(i);
    const Notes_Node* folder = AddFolder(0, i, title);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 10; ++j) {
      std::string title = IndexedURLTitle(1000 * i + j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddNote(0, folder, j, title, url) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ReverseChildOrder(0, GetNotesTopNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 373379.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, MC_BiDirectionalPushAddingBM) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  DisableVerifier();
  for (int i = 0; i < 2; ++i) {
    std::string title0 = IndexedURLTitle(2 * i);
    GURL url0 = GURL(IndexedURL(2 * i));
    ASSERT_TRUE(AddNote(0, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(2 * i + 1);
    GURL url1 = GURL(IndexedURL(2 * i + 1));
    ASSERT_TRUE(AddNote(1, title1, url1) != NULL);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 373503.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_BiDirectionalPush_AddingSameBMs) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Note: When a racy commit is done with identical notes, it is possible
  // for duplicates to exist after sync completes. See http://crbug.com/19769.
  DisableVerifier();
  for (int i = 0; i < 2; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, title, url) != NULL);
    ASSERT_TRUE(AddNote(1, title, url) != NULL);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 373506.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_BootStrapEmptyStateEverywhere) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Test Scribe ID - 373505.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_Merge_CaseInsensitivity_InNames) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const Notes_Node* folder0 = AddFolder(0, "Folder");
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 0, "Note 0", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 1, "Note 1", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 2, "Note 2", GURL(kGenericURL)) != NULL);

  const Notes_Node* folder1 = AddFolder(1, "fOlDeR");
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 0, "nOtE 0", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 1, "NoTe 1", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 2, "nOTE 2", GURL(kGenericURL)) != NULL);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 373508.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_SimpleMergeOfDifferentBMModels) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
    ASSERT_TRUE(AddNote(1, i, title, url) != NULL);
  }

  for (int i = 3; i < 10; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(i + 7);
    GURL url1 = GURL(IndexedURL(i + 7));
    ASSERT_TRUE(AddNote(1, i, title1, url1) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 386586.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_MergeSimpleBMHierarchyUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
    ASSERT_TRUE(AddNote(1, i, title, url) != NULL);
  }

  for (int i = 3; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(1, i, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 386589.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_MergeSimpleBMHierarchyEqualSetsUnderBMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
    ASSERT_TRUE(AddNote(1, i, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 373504 - Merge note folders with different notes.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_MergeBMFoldersWithDifferentBMs) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const Notes_Node* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  const Notes_Node* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  for (int i = 0; i < 2; ++i) {
    std::string title0 = IndexedURLTitle(2 * i);
    GURL url0 = GURL(IndexedURL(2 * i));
    ASSERT_TRUE(AddNote(0, folder0, i, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(2 * i + 1);
    GURL url1 = GURL(IndexedURL(2 * i + 1));
    ASSERT_TRUE(AddNote(1, folder1, i, title1, url1) != NULL);
  }
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 373509 - Merge moderately complex note models.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_MergeDifferentBMModelsModeratelyComplex) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 25; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(i + 50);
    GURL url1 = GURL(IndexedURL(i + 50));
    ASSERT_TRUE(AddNote(1, i, title1, url1) != NULL);
  }
  for (int i = 25; i < 30; ++i) {
    std::string title0 = IndexedFolderName(i);
    const Notes_Node* folder0 = AddFolder(0, i, title0);
    ASSERT_TRUE(folder0 != NULL);
    std::string title1 = IndexedFolderName(i + 50);
    const Notes_Node* folder1 = AddFolder(1, i, title1);
    ASSERT_TRUE(folder1 != NULL);
    for (int j = 0; j < 5; ++j) {
      std::string title0 = IndexedURLTitle(i + 5 * j);
      GURL url0 = GURL(IndexedURL(i + 5 * j));
      ASSERT_TRUE(AddNote(0, folder0, j, title0, url0) != NULL);
      std::string title1 = IndexedURLTitle(i + 5 * j + 50);
      GURL url1 = GURL(IndexedURL(i + 5 * j + 50));
      ASSERT_TRUE(AddNote(1, folder1, j, title1, url1) != NULL);
    }
  }
  for (int i = 100; i < 125; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, title, url) != NULL);
    ASSERT_TRUE(AddNote(1, title, url) != NULL);
  }
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3675271 - Merge simple note subset under note folder.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_MergeSimpleBMHierarchySubsetUnderBMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 2; ++i) {
    const Notes_Node* folder = AddFolder(i, kGenericFolderName);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 4; ++j) {
      if (base::RandDouble() < 0.5) {
        std::string title = IndexedURLTitle(j);
        GURL url = GURL(IndexedURL(j));
        ASSERT_TRUE(AddNote(i, folder, j, title, url) != NULL);
      } else {
        std::string title = IndexedFolderName(j);
        ASSERT_TRUE(AddFolder(i, folder, j, title) != NULL);
      }
    }
  }
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3727284 - Merge subsets of note under note bar.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_MergeSimpleBMHierarchySubsetUnderNoteBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 4; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
  }

  for (int j = 0; j < 2; ++j) {
    std::string title = IndexedURLTitle(j);
    GURL url = GURL(IndexedURL(j));
    ASSERT_TRUE(AddNote(1, j, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
  ASSERT_FALSE(ContainsDuplicateNotes(1));
}

// TCM ID - 3659294 - Merge simple note hierarchy under note folder.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_Merge_SimpleBMHierarchy_Under_BMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const Notes_Node* folder0 = AddFolder(0, 0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 0, IndexedURLTitle(1), GURL(IndexedURL(1))) !=
              NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 2, IndexedURLTitle(3), GURL(IndexedURL(3))) !=
              NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 3, IndexedSubfolderName(4)) != NULL);

  const Notes_Node* folder1 = AddFolder(1, 0, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 0, IndexedSubfolderName(0)) != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 2, IndexedURLTitle(3), GURL(IndexedURL(3))) !=
              NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 3, IndexedSubfolderName(5)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 4, IndexedURLTitle(1), GURL(IndexedURL(1))) !=
              NULL);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3711273 - Merge disjoint sets of note hierarchy under note
// folder.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_Merge_SimpleBMHierarchy_DisjointSets_Under_BMFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const Notes_Node* folder0 = AddFolder(0, 0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 0, IndexedURLTitle(1), GURL(IndexedURL(1))) !=
              NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 2, IndexedURLTitle(3), GURL(IndexedURL(3))) !=
              NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 3, IndexedSubfolderName(4)) != NULL);

  const Notes_Node* folder1 = AddFolder(1, 0, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 0, IndexedSubfolderName(5)) != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 1, IndexedSubfolderName(6)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 2, IndexedURLTitle(7), GURL(IndexedURL(7))) !=
              NULL);
  ASSERT_TRUE(AddNote(1, folder1, 3, IndexedURLTitle(8), GURL(IndexedURL(8))) !=
              NULL);

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3639296 - Merge disjoint sets of note hierarchy under note
// bar.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_Merge_SimpleBMHierarchy_DisjointSets_Under_Note) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i + 1);
    GURL url = GURL(IndexedURL(i + 1));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
  }

  for (int j = 0; j < 3; ++j) {
    std::string title = IndexedURLTitle(j + 4);
    GURL url = GURL(IndexedURL(j + 4));
    ASSERT_TRUE(AddNote(0, j, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3616282 - Merge sets of duplicate notes under note bar.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       MC_Merge_SimpleBMHierarchy_DuplicateBMs_Under_BMBar) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  // Let's add duplicate set of note {1,2,2,3,3,3,4,4,4,4} to client0.
  int node_index = 0;
  for (int i = 1; i < 5; ++i) {
    for (int j = 0; j < i; ++j) {
      std::string title = IndexedURLTitle(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, node_index, title, url) != NULL);
      ++node_index;
    }
  }
  // Let's add a set of notes {1,2,3,4} to client1.
  for (int i = 0; i < 4; ++i) {
    std::string title = IndexedURLTitle(i + 1);
    GURL url = GURL(IndexedURL(i + 1));
    ASSERT_TRUE(AddNote(1, i, title, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 1; i < 5; ++i) {
    ASSERT_TRUE(CountNotesWithTitlesMatching(1, IndexedURLTitle(i)) == i);
  }
}

// TCM ID - 6593872.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DisableNotes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::NOTES));
  ASSERT_TRUE(AddFolder(1, kGenericFolderName) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::NOTES));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// TCM ID - 7343544.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  ASSERT_TRUE(AddFolder(0, IndexedFolderName(0)) != NULL);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(AddFolder(1, IndexedFolderName(1)) != NULL);
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForAllDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// TCM ID - 3662298 - Test adding duplicate folder - Both with different BMs
// underneath.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, MC_DuplicateFolders) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const Notes_Node* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  const Notes_Node* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  for (int i = 0; i < 5; ++i) {
    std::string title0 = IndexedURLTitle(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder0, i, title0, url0) != NULL);
    std::string title1 = IndexedURLTitle(i + 5);
    GURL url1 = GURL(IndexedURL(i + 5));
    ASSERT_TRUE(AddNote(1, folder1, i, title1, url1) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// This test fails when run with FakeServer and FakeServerInvalidationService.
IN_PROC_BROWSER_TEST_F(LegacyTwoClientNotesSyncTest, MC_DeleteNote) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(GetClient(1)->DisableSyncForDatatype(syncer::NOTES));

  const GURL bar_url("http://example.com/bar");
  const GURL other_url("http://example.com/other");

  ASSERT_TRUE(AddNote(0, GetNotesTopNode(0), 0, "bar", bar_url) != NULL);
  ASSERT_TRUE(AddNote(0, GetNotesTopNode(0), 1, "other", other_url) != NULL);

  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  ASSERT_TRUE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_FALSE(HasNodeWithURL(1, other_url));

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));

  ASSERT_TRUE(GetClient(1)->EnableSyncForDatatype(syncer::NOTES));
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_TRUE(HasNodeWithURL(1, other_url));
}

// TCM ID - 3719307 - Test a scenario of updating the name of the same note
// from two clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, MC_NoteNameChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const Notes_Node* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  DisableVerifier();
  GURL url(IndexedURL(0));
  SetTitle(0, GetUniqueNodeByURL(0, url), "Title++");
  SetTitle(1, GetUniqueNodeByURL(1, url), "Title--");

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3672299 - Test a scenario of updating the URL of the same note
// from two clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, MC_NoteURLChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const Notes_Node* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder0, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  DisableVerifier();
  GURL url(IndexedURL(0));
  ASSERT_TRUE(
      SetURL(0, GetUniqueNodeByURL(0, url), GURL("http://www.google.com/00")));
  ASSERT_TRUE(
      SetURL(1, GetUniqueNodeByURL(1, url), GURL("http://www.google.com/11")));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3699290 - Test a scenario of updating the BM Folder name from two
// clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, MC_FolderNameChangeConflict) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  DisableVerifier();

  const Notes_Node* folderA[2];
  const Notes_Node* folderB[2];
  const Notes_Node* folderC[2];

  // Create empty folder A on both clients.
  folderA[0] = AddFolder(0, IndexedFolderName(0));
  ASSERT_TRUE(folderA[0] != NULL);
  folderA[1] = AddFolder(1, IndexedFolderName(0));
  ASSERT_TRUE(folderA[1] != NULL);

  // Create folder B with notes on both clients.
  folderB[0] = AddFolder(0, IndexedFolderName(1));
  ASSERT_TRUE(folderB[0] != NULL);
  folderB[1] = AddFolder(1, IndexedFolderName(1));
  ASSERT_TRUE(folderB[1] != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folderB[0], i, title, url) != NULL);
  }

  // Create folder C with notes and subfolders on both clients.
  folderC[0] = AddFolder(0, IndexedFolderName(2));
  ASSERT_TRUE(folderC[0] != NULL);
  folderC[1] = AddFolder(1, IndexedFolderName(2));
  ASSERT_TRUE(folderC[1] != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string folder_name = IndexedSubfolderName(i);
    const Notes_Node* subfolder = AddFolder(0, folderC[0], i, folder_name);
    ASSERT_TRUE(subfolder != NULL);
    for (int j = 0; j < 3; ++j) {
      std::string title = IndexedURLTitle(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddNote(0, subfolder, j, title, url) != NULL);
    }
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  // Simultaneously rename folder A on both clients.
  SetTitle(0, folderA[0], "Folder A++");
  SetTitle(1, folderA[1], "Folder A--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  // Simultaneously rename folder B on both clients.
  SetTitle(0, folderB[0], "Folder B++");
  SetTitle(1, folderB[1], "Folder B--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  // Simultaneously rename folder C on both clients.
  SetTitle(0, folderC[0], "Folder C++");
  SetTitle(1, folderC[1], "Folder C--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, SingleClientEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SingleClientEnabledEncryptionAndChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(AddNote(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, BothClientsEnabledEncryption) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(EnableEncryption(1));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SingleClientEnabledEncryptionBothChanged) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AddNote(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(AddNote(0, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       SingleClientEnabledEncryptionAndChangedMultipleTimes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, IndexedURLTitle(0), GURL(IndexedURL(0))) != NULL);
  ASSERT_TRUE(EnableEncryption(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(IsEncryptionComplete(0));
  ASSERT_TRUE(IsEncryptionComplete(1));
  ASSERT_TRUE(AllModelsMatchVerifier());

  ASSERT_TRUE(AddNote(0, IndexedURLTitle(1), GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AddFolder(0, IndexedFolderName(0)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatchVerifier());
}

// Deliberately racy rearranging of notes to test that our conflict resolver
// code results in a consistent view across machines (no matter what the final
// order is).
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, RacyPositionChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Add initial notes.
  size_t num_notes = 5;
  for (size_t i = 0; i < num_notes; ++i) {
    ASSERT_TRUE(AddNote(0, i, IndexedURLTitle(i), GURL(IndexedURL(i))) != NULL);
  }

  // Once we make diverging changes the verifer is helpless.
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatchVerifier());
  DisableVerifier();

  // Make changes on client 0.
  for (size_t i = 0; i < num_notes; ++i) {
    const Notes_Node* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_notes - 1);
    DVLOG(1) << "Moving client 0's note " << i << " to position " << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  // Make changes on client 1.
  for (size_t i = 0; i < num_notes; ++i) {
    const Notes_Node* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_notes - 1);
    DVLOG(1) << "Moving client 1's note " << i << " to position " << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  // Now make changes to client 1 first.
  for (size_t i = 0; i < num_notes; ++i) {
    const Notes_Node* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_notes - 1);
    DVLOG(1) << "Moving client 1's note " << i << " to position " << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  // Make changes on client 0.
  for (size_t i = 0; i < num_notes; ++i) {
    const Notes_Node* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_notes - 1);
    DVLOG(1) << "Moving client 0's note " << i << " to position " << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, NoteAllNodesRemovedEvent) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatchVerifier());

  // Starting state:
  // main_note
  //    -> folder0
  //      -> tier1_a
  //        -> http://mail.google.com
  //        -> http://www.google.com
  //      -> http://news.google.com
  //      -> http://yahoo.com
  //    -> http://www.cnn.com
  //   -> empty_folder
  //   -> folder1
  //     -> http://yahoo.com
  //   -> http://gmail.com

  const Notes_Node* folder0 = AddFolder(0, GetNotesTopNode(0), 0, "folder0");
  const Notes_Node* tier1_a = AddFolder(0, folder0, 0, "tier1_a");
  ASSERT_TRUE(AddNote(0, folder0, 1, "News", GURL("http://news.google.com")));
  ASSERT_TRUE(AddNote(0, folder0, 2, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AddNote(0, tier1_a, 0, "Gmai", GURL("http://mail.google.com")));
  ASSERT_TRUE(AddNote(0, tier1_a, 1, "Google", GURL("http://www.google.com")));
  ASSERT_TRUE(
      AddNote(0, GetNotesTopNode(0), 1, "CNN", GURL("http://www.cnn.com")));

  ASSERT_TRUE(AddFolder(0, GetNotesTopNode(0), 0, "empty_folder"));
  const Notes_Node* folder1 = AddFolder(0, GetNotesTopNode(0), 1, "folder1");
  ASSERT_TRUE(AddNote(0, folder1, 0, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(
      AddNote(0, GetNotesTopNode(0), 2, "Gmail", GURL("http://gmail.com")));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  // Remove all
  RemoveAll(0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  // Verify other node has no children now.
  EXPECT_EQ(0, GetNotesTopNode(0)->child_count());
  EXPECT_EQ(0, GetNotesTopNode(0)->child_count());
  ASSERT_TRUE(AllModelsMatch());
}
