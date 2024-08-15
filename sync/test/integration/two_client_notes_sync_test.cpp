// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/rand_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/passwords_helper.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "content/public/test/browser_test.h"
#include "sync/test/integration/notes_helper.h"
#include "sync/test/integration/notes_sync_test.h"
#include "ui/base/layout.h"

using notes_helper::AddFolder;
using notes_helper::AddNote;
using notes_helper::AllModelsMatch;
using notes_helper::ContainsDuplicateNotes;
using notes_helper::CountNotesWithContentMatching;
using notes_helper::CreateAutoIndexedContent;
using notes_helper::GetNotesModel;
using notes_helper::GetNotesTopNode;
using notes_helper::GetUniqueNodeByURL;
using notes_helper::HasNodeWithURL;
using notes_helper::IndexedFolderName;
using notes_helper::IndexedSubfolderName;
using notes_helper::IndexedSubsubfolderName;
using notes_helper::IndexedURL;
using notes_helper::IndexedURLTitle;
using notes_helper::Move;
using notes_helper::Remove;
using notes_helper::RemoveAll;
using notes_helper::ReverseChildOrder;
using notes_helper::SetContent;
using notes_helper::SetTitle;
using notes_helper::SetURL;
using notes_helper::SortChildren;

static const char kGenericURL[] = "http://www.host.ext:1234/path/filename";
static const char kGenericURLContent[] = "URL\ncontent";
static const char kGenericURLTitle[] = "URL Title";
static const char kGenericFolderName[] = "Folder Name";
static const char kGenericSubfolderName[] = "Subfolder Name";

class TwoClientNotesSyncTest : public NotesSyncTest {
 public:
  TwoClientNotesSyncTest() : NotesSyncTest(TWO_CLIENT) {}
  ~TwoClientNotesSyncTest() override {}
  TwoClientNotesSyncTest(const TwoClientNotesSyncTest&) = delete;
  TwoClientNotesSyncTest& operator=(const TwoClientNotesSyncTest&) = delete;

  bool UseVerifier() override { return false; }
};

class LegacyTwoClientNotesSyncTest : public NotesSyncTest {
 public:
  LegacyTwoClientNotesSyncTest() : NotesSyncTest(TWO_CLIENT) {}
  ~LegacyTwoClientNotesSyncTest() override {}
  LegacyTwoClientNotesSyncTest(const LegacyTwoClientNotesSyncTest&) = delete;
  LegacyTwoClientNotesSyncTest& operator=(const LegacyTwoClientNotesSyncTest&) =
      delete;
};

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GURL vivaldi_url = GURL("https://en.wikipedia.org/wiki/Antonio_Vivaldi");
  ASSERT_TRUE(
      AddNote(0,
              "Antonio Lucio Vivaldi was an Italian Baroque musical composer",
              "Vivaldi", vivaldi_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(
      AddNote(
          1,
          "The Four Seasons is a group of four violin concerti by Italian "
          "composer Antonio Vivaldi",
          "The Four Seasons",
          GURL("https://en.wikipedia.org/wiki/The_Four_Seasons_(Vivaldi)")) !=
      NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* trash_node_0 = GetNotesModel(0)->trash_node();
  const NoteNode* trash_node_1 = GetNotesModel(1)->trash_node();
  ASSERT_TRUE(AddNote(0, trash_node_0, 0,
                      "Venice is a city in northeastern Italy and the capital "
                      "of the Veneto region.",
                      "Venice", GURL("https://en.wikipedia.org/wiki/Venice")));
  ASSERT_EQ(1u, trash_node_0->children().size());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_EQ(1u, trash_node_1->children().size());
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* new_folder = AddFolder(0, 2, "New Folder");
  Move(0, GetUniqueNodeByURL(0, vivaldi_url), new_folder, 0);
  SetContent(0, GetNotesTopNode(0)->children()[0].get(),
             "The Four Seasons is the best known of Vivaldi's works.");
  SetTitle(0, GetNotesTopNode(0)->children()[0].get(),
           "The Four Seasons (Vivaldi)");
  ASSERT_TRUE(AddNote(0, GetNotesTopNode(0), 1,
                      "Baroque music is a period or style of Western art music "
                      "composed from approximately 1600 to 1750."
                      "Baroque Music",
                      GURL("https://en.wikipedia.org/wiki/Baroque_music")) !=
              NULL);
  ASSERT_TRUE(AddNote(0, GetNotesTopNode(0), 1,
                      "Eggs,\nmilk,\nflour,\n,butter"
                      "Shopping list",
                      GURL()) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(
      AddNote(1,
              "A concerto is a musical composition generally composed of three "
              "movements, in which, usually, one solo instrument is "
              "accompanied by an orchestra or concert band.",
              "Concerto",
              GURL("https://en.wikipedia.org/wiki/Concerto")) != NULL);
  ASSERT_TRUE(
      AddNote(1,
              "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Proin "
              "tincidunt feugiat erat sit amet hendrerit. Vestibulum porttitor "
              "magna et risus cursus facilisis. Morbi sit amet erat ac ex "
              "pulvinar euismod eget a massa.",
              GURL()) != NULL);
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(1, trash_node_1, 0);
  ASSERT_EQ(0u, trash_node_1->children().size());
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_EQ(0u, trash_node_0->children().size());
  ASSERT_TRUE(AllModelsMatch());

  SortChildren(1, GetNotesTopNode(1));
  ASSERT_TRUE(GetClient(1)->AwaitMutualSyncCycleCompletion(GetClient(0)));
  ASSERT_TRUE(AllModelsMatch());

  SetTitle(0, GetUniqueNodeByURL(0, vivaldi_url), "Vivaldi++");
  SetTitle(1, GetUniqueNodeByURL(1, vivaldi_url), "Vivaldi--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SimultaneousURLChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GURL initial_url("http://www.elg.no");
  GURL second_url("http://www.elg.no/ulv");
  GURL third_url("http://www.elg.no/sau");
  std::string content =
      "Elger er gromme dyr.\nElgkalvene er mat for bl.a. ulv.";

  ASSERT_TRUE(AddNote(0, content, initial_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));

  ASSERT_TRUE(SetURL(0, GetUniqueNodeByURL(0, initial_url), second_url) !=
              NULL);
  ASSERT_TRUE(SetURL(1, GetUniqueNodeByURL(1, initial_url), third_url) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  SetTitle(0, GetNotesTopNode(0)->children()[0].get(), "Elg");
  SetContent(0, GetNotesTopNode(0)->children()[0].get(),
             "Elg er et spesielt stort hjortedyr som trives i temperert klima "
             "i det store boreale barskogbeltet som finnes i Nordeuropa.");
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370558.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_SC_AddFirstFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370559.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_SC_AddFirstNote) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddNote(0, kGenericURLContent, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370560.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_SC_AddNonHTTPNote) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddNote(0, "Content from FTP",
                      GURL("ftp://user:password@host:1234/path")) != NULL);
  ASSERT_TRUE(AddNote(0, "Content from a file", GURL("file://host/path")) !=
              NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370561.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_AddFirstNoteUnderFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  ASSERT_TRUE(AddNote(0, folder, 0, kGenericURLContent, kGenericURLTitle,
                      GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370562.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_SC_AddSeveralNotes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 20; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370563.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_AddSeveralNotesAndFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      const NoteNode* folder = AddFolder(0, i, title);
      ASSERT_TRUE(folder != NULL);
      if (base::RandDouble() > 0.4) {
        for (int j = 0; j < 20; ++j) {
          std::string content = CreateAutoIndexedContent(i);
          GURL url = GURL(IndexedURL(j));
          ASSERT_TRUE(AddNote(0, folder, j, content, url) != NULL);
        }
      }
    }
  }
  for (int i = 0; i < 10; i++) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, GetNotesTopNode(0), i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370641.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_DuplicateNotesWithDifferentURLSameContent) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GURL url0 = GURL(IndexedURL(0));
  GURL url1 = GURL(IndexedURL(1));
  ASSERT_TRUE(AddNote(0, kGenericURLContent, url0) != NULL);
  ASSERT_TRUE(AddNote(0, kGenericURLContent, url1) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 370639 - Add notes with different name and same URL.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_DuplicateNotesWithSameURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  std::string content0 = CreateAutoIndexedContent(0);
  std::string content1 = CreateAutoIndexedContent(1);
  ASSERT_TRUE(AddNote(0, content0, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(0, content1, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371817.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_SC_RenameNoteName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  std::string content = CreateAutoIndexedContent(1);
  const NoteNode* note = AddNote(0, content, GURL(kGenericURL));
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  std::string new_title = IndexedURLTitle(2);
  SetTitle(0, note, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_SC_ChangeNoteContent) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  std::string content = CreateAutoIndexedContent(1);
  const NoteNode* note = AddNote(0, content, GURL(kGenericURL));
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  std::string new_content = CreateAutoIndexedContent(2);
  SetContent(0, note, new_content);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371822.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_SC_ChangeNoteURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GURL url = GURL(IndexedURL(1));
  const NoteNode* note = AddNote(0, kGenericURLContent, url);
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  GURL new_url = GURL(IndexedURL(2));
  ASSERT_TRUE(SetURL(0, note, new_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371818 - Renaming the same note name twice.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_TwiceRenamingNoteName) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  std::string title = IndexedURLTitle(1);
  const NoteNode* note =
      AddNote(0, kGenericURLContent, title, GURL(kGenericURL));
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  std::string new_title = IndexedURLTitle(2);
  SetTitle(0, note, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  SetTitle(0, note, title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_TwiceChangingNoteContent) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  std::string content = CreateAutoIndexedContent(1);
  const NoteNode* note = AddNote(0, content, GURL(kGenericURL));
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  std::string new_content = CreateAutoIndexedContent(2);
  SetContent(0, note, new_content);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  SetContent(0, note, content);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371823 - Renaming the same note URL twice.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_TwiceRenamingNoteURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GURL url = GURL(IndexedURL(1));
  const NoteNode* note = AddNote(0, kGenericURLContent, url);
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  GURL new_url = GURL(IndexedURL(2));
  ASSERT_TRUE(SetURL(0, note, new_url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(SetURL(0, note, url) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371824.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_SC_RenameNotesFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  std::string title = IndexedFolderName(1);
  const NoteNode* folder = AddFolder(0, title);
  ASSERT_TRUE(AddNote(0, folder, 0, kGenericURLContent, GURL(kGenericURL)) !=
              NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371825.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_RenameEmptyNotesFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  std::string title = IndexedFolderName(1);
  const NoteNode* folder = AddFolder(0, title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371826.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_RenameNotesFolderWithLongHierarchy) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  std::string title = IndexedFolderName(1);
  const NoteNode* folder = AddFolder(0, title);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
    } else {
      std::string title2 = IndexedSubfolderName(i);
      ASSERT_TRUE(AddFolder(0, folder, i, title2) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  std::string new_title = IndexedFolderName(2);
  SetTitle(0, folder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371827.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_RenameNotesFolderThatHasParentAndChildren) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 1; i < 15; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
  }
  std::string title = IndexedSubfolderName(1);
  const NoteNode* subfolder = AddFolder(0, folder, 0, title);
  for (int i = 0; i < 120; ++i) {
    if (base::RandDouble() > 0.15) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, subfolder, i, content, url) != NULL);
    } else {
      std::string title2 = IndexedSubsubfolderName(i);
      ASSERT_TRUE(AddFolder(0, subfolder, i, title2) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  std::string new_title = IndexedSubfolderName(2);
  SetTitle(0, subfolder, new_title);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371828.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_RenameNoteNameAndContentAndURL) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GURL url = GURL(IndexedURL(1));
  std::string content = CreateAutoIndexedContent(1);
  std::string title = IndexedURLTitle(1);
  const NoteNode* note = AddNote(0, content, title, url);
  ASSERT_TRUE(note != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  GURL new_url = GURL(IndexedURL(2));
  std::string new_content = CreateAutoIndexedContent(2);
  std::string new_title = IndexedURLTitle(2);
  note = SetURL(0, note, new_url);
  ASSERT_TRUE(note != NULL);
  SetTitle(0, note, new_title);
  SetContent(0, note, new_content);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371832.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_DeleteNoteEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddNote(0, kGenericURLContent, GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371833.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_DelNoteNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 20; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371835.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_SC_DelFirstNoteUnderNotesFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, folder, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371836.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_SC_DelLastNoteUnderNotesFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, folder, folder->children().size() - 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371856.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_SC_DelMiddleNoteUnderNotesFoldNonEmptyFoldAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, folder, 4);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371857.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_SC_DelNotessUnderNotesFoldEmptyFolderAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  size_t child_count = folder->children().size();
  for (size_t i = 0; i < child_count; ++i) {
    Remove(0, folder, 0);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371858.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_DelEmptyNotesFoldEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371869.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_DelEmptyNotesFoldNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddFolder(0, kGenericFolderName) != NULL);
  for (int i = 1; i < 15; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, GetNotesTopNode(0), 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371879.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_SC_DelNotesFoldWithNotesNonEmptyAccountAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const NoteNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
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
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, GetNotesTopNode(0), 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371880.
IN_PROC_BROWSER_TEST_F(
    TwoClientNotesSyncTest,
    DISABLED_SC_DelNotesFoldWithNotesAndNotesFoldsNonEmptyACAfterwards) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const NoteNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
    } else {
      std::string title = IndexedFolderName(i);
      ASSERT_TRUE(AddFolder(0, i, title) != NULL);
    }
  }
  for (int i = 0; i < 10; ++i) {
    if (base::RandDouble() > 0.6) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
    } else {
      std::string title = IndexedSubfolderName(i);
      const NoteNode* subfolder = AddFolder(0, folder, i, title);
      ASSERT_TRUE(subfolder != NULL);
      if (base::RandDouble() > 0.3) {
        for (int j = 0; j < 10; ++j) {
          if (base::RandDouble() > 0.6) {
            std::string content = CreateAutoIndexedContent(j);
            GURL url = GURL(IndexedURL(j));
            ASSERT_TRUE(AddNote(0, subfolder, j, content, url) != NULL);
          } else {
            std::string title2 = IndexedSubsubfolderName(j);
            ASSERT_TRUE(AddFolder(0, subfolder, j, title2) != NULL);
          }
        }
      }
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, GetNotesTopNode(0), 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371882.
IN_PROC_BROWSER_TEST_F(
    TwoClientNotesSyncTest,
    DISABLED_SC_DelBNotesFoldWithParentAndChildrenNotesAndNotesFolds) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 1; i < 11; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, title, url) != NULL);
  }
  const NoteNode* subfolder = AddFolder(0, folder, 0, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 30; ++i) {
    if (base::RandDouble() > 0.2) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, subfolder, i, content, url) != NULL);
    } else {
      std::string title = IndexedSubsubfolderName(i);
      ASSERT_TRUE(AddFolder(0, subfolder, i, title) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Remove(0, folder, 0);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371931.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_ReverseTheOrderOfTwoNotes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  GURL url0 = GURL(IndexedURL(0));
  GURL url1 = GURL(IndexedURL(1));
  std::string content0 = CreateAutoIndexedContent(0);
  std::string content1 = CreateAutoIndexedContent(1);
  const NoteNode* note0 = AddNote(0, 0, content0, url0);
  const NoteNode* note1 = AddNote(0, 1, content1, url1);
  ASSERT_TRUE(note0 != NULL);
  ASSERT_TRUE(note1 != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Move(0, note0, GetNotesTopNode(0), 2);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371933.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_ReverseTheOrderOf10Notes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  ReverseChildOrder(0, GetNotesTopNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371954.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_MovingNotessFromRootToNotesFolder) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddNote(0, kGenericURLTitle, GURL(kGenericURL)) != NULL);
  const NoteNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 2; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(GetNotesTopNode(0)->children().size() >= 2);
  size_t num_notes_to_move = GetNotesTopNode(0)->children().size() - 2;
  for (size_t i = 0; i < num_notes_to_move; ++i) {
    Move(0, GetNotesTopNode(0)->children()[2].get(), folder, i);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(AllModelsMatch());
  }
}

// Test Scribe ID - 371957.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_MovingNotesFromFoldToRoot) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(AddNote(0, kGenericURLContent, kGenericURLTitle,
                      GURL(kGenericURL)) != NULL);
  const NoteNode* folder = AddFolder(0, 1, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  int num_notes_to_move = folder->children().size() - 2;
  for (int i = 0; i < num_notes_to_move; ++i) {
    Move(0, folder->children()[0].get(), GetNotesTopNode(0), i);
    ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
    ASSERT_TRUE(AllModelsMatch());
  }
}

// Test Scribe ID - 371961.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_SC_MovingNotesFromParentNotesFoldToChildNotesFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
  }
  const NoteNode* subfolder = AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i + 3);
    GURL url = GURL(IndexedURL(i + 3));
    ASSERT_TRUE(AddNote(0, subfolder, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 3; ++i) {
    GURL url = GURL(IndexedURL(i));
    Move(0, GetUniqueNodeByURL(0, url), subfolder, i + 10);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371964.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_SC_MovingNotesFromChildNotesFoldToParentNotesFold) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
  }
  const NoteNode* subfolder = AddFolder(0, folder, 3, kGenericSubfolderName);
  ASSERT_TRUE(subfolder != NULL);
  for (int i = 0; i < 5; ++i) {
    std::string content = CreateAutoIndexedContent(i + 3);
    GURL url = GURL(IndexedURL(i + 3));
    ASSERT_TRUE(AddNote(0, subfolder, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 3; ++i) {
    GURL url = GURL(IndexedURL(i + 3));
    Move(0, GetUniqueNodeByURL(0, url), folder, i + 4);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371967.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_HoistNotes10LevelUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = GetNotesTopNode(0);
  const NoteNode* folder_L0 = NULL;
  const NoteNode* folder_L10 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
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
  ASSERT_TRUE(AllModelsMatch());

  GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(0, url10), folder_L0,
       folder_L0->children().size());
  GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L0, 0);
  GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L0, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371968.
// Flaky. http://crbug.com/107744.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_SinkNotes10LevelDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = GetNotesTopNode(0);
  const NoteNode* folder_L0 = NULL;
  const NoteNode* folder_L10 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
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
  ASSERT_TRUE(AllModelsMatch());

  GURL url10 = GURL(IndexedURL(10));
  Move(0, GetUniqueNodeByURL(0, url10), folder_L10,
       folder_L10->children().size());
  GURL url11 = GURL(IndexedURL(11));
  Move(0, GetUniqueNodeByURL(0, url11), folder_L10, 0);
  GURL url12 = GURL(IndexedURL(12));
  Move(0, GetUniqueNodeByURL(0, url12), folder_L10, 1);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371980.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_SinkEmptyNotesFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = GetNotesTopNode(0);
  const NoteNode* folder_L5 = NULL;
  for (int level = 0; level < 15; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5)
      folder_L5 = folder;
  }
  folder =
      AddFolder(0, GetNotesTopNode(0)->children().size(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Move(0, folder, folder_L5, folder_L5->children().size());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 371997.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_SinkNonEmptyNotesFold5LevelsDown) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = GetNotesTopNode(0);
  const NoteNode* folder_L5 = NULL;
  for (int level = 0; level < 6; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5)
      folder_L5 = folder;
  }
  folder =
      AddFolder(0, GetNotesTopNode(0)->children().size(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(CreateAutoIndexedContent(i));
    ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Move(0, folder, folder_L5, folder_L5->children().size());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 372006.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_HoistFolder5LevelsUp) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  const NoteNode* folder = GetNotesTopNode(0);
  const NoteNode* folder_L5 = NULL;
  for (int level = 0; level < 6; ++level) {
    int num_notes = base::RandInt(0, 9);
    for (int i = 0; i < num_notes; ++i) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, folder, i, content, url) != NULL);
    }
    std::string title = IndexedFolderName(level);
    folder = AddFolder(0, folder, folder->children().size(), title);
    ASSERT_TRUE(folder != NULL);
    if (level == 5)
      folder_L5 = folder;
  }
  folder =
      AddFolder(0, folder_L5, folder_L5->children().size(), kGenericFolderName);
  ASSERT_TRUE(folder != NULL);
  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder, i, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  Move(0, folder, GetNotesTopNode(0), GetNotesTopNode(0)->children().size());
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 372026.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_ReverseTheOrderOfTwoNotesFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 2; ++i) {
    std::string title = IndexedFolderName(i);
    const NoteNode* folder = AddFolder(0, i, title);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 10; ++j) {
      std::string content = CreateAutoIndexedContent(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddNote(0, folder, j, content, url) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  ReverseChildOrder(0, GetNotesTopNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 372028.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_SC_ReverseTheOrderOfTenNotesFolders) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 10; ++i) {
    std::string title = IndexedFolderName(i);
    const NoteNode* folder = AddFolder(0, i, title);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 10; ++j) {
      std::string content = CreateAutoIndexedContent(1000 * i + j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddNote(0, folder, j, content, url) != NULL);
    }
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());

  ReverseChildOrder(0, GetNotesTopNode(0));
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 373379.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_BiDirectionalPushAddingNotes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  for (int i = 0; i < 2; ++i) {
    std::string content0 = CreateAutoIndexedContent(2 * i);
    GURL url0 = GURL(IndexedURL(2 * i));
    ASSERT_TRUE(AddNote(0, content0, url0) != NULL);
    std::string content1 = CreateAutoIndexedContent(2 * i + 1);
    GURL url1 = GURL(IndexedURL(2 * i + 1));
    ASSERT_TRUE(AddNote(1, content1, url1) != NULL);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 373503.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_BiDirectionalPush_AddingSameNotes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  // Note: When a racy commit is done with identical notes, it is possible
  // for duplicates to exist after sync completes. See http://crbug.com/19769.
  for (int i = 0; i < 2; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, content, url) != NULL);
    ASSERT_TRUE(AddNote(1, content, url) != NULL);
  }
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 373506.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_BootStrapEmptyStateEverywhere) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// Test Scribe ID - 373505.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_Merge_CaseInsensitivity_InNames) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const NoteNode* folder0 = AddFolder(0, "Folder");
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 0, "Note 0", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 1, "Note 1", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 2, "Note 2", GURL(kGenericURL)) != NULL);

  const NoteNode* folder1 = AddFolder(1, "fOlDeR");
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 0, "nOtE 0", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 1, "NoTe 1", GURL(kGenericURL)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 2, "nOTE 2", GURL(kGenericURL)) != NULL);

  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 373508.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_SimpleMergeOfDifferentNotesModels) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
    ASSERT_TRUE(AddNote(1, i, content, url) != NULL);
  }

  for (int i = 3; i < 10; ++i) {
    std::string content0 = CreateAutoIndexedContent(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content0, url0) != NULL);
    std::string content1 = CreateAutoIndexedContent(i + 7);
    GURL url1 = GURL(IndexedURL(i + 7));
    ASSERT_TRUE(AddNote(1, i, content1, url1) != NULL);
  }

  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 386586.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_MergeSimpleNotesHierarchy) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
    ASSERT_TRUE(AddNote(1, i, content, url) != NULL);
  }

  for (int i = 3; i < 10; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(1, i, content, url) != NULL);
  }

  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 386589.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_MergeSimpleNotesHierarchyEqualSets) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
    ASSERT_TRUE(AddNote(1, i, content, url) != NULL);
  }

  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 373504 - Merge note folders with different notes.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_MergeNotesFoldersWithDifferentNotes) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const NoteNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  const NoteNode* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  for (int i = 0; i < 2; ++i) {
    std::string content0 = CreateAutoIndexedContent(2 * i);
    GURL url0 = GURL(IndexedURL(2 * i));
    ASSERT_TRUE(AddNote(0, folder0, i, content0, url0) != NULL);
    std::string content1 = CreateAutoIndexedContent(2 * i + 1);
    GURL url1 = GURL(IndexedURL(2 * i + 1));
    ASSERT_TRUE(AddNote(1, folder1, i, content1, url1) != NULL);
  }
  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Test Scribe ID - 373509 - Merge moderately complex note models.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_MergeDifferentNotesModelsModeratelyComplex) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (int i = 0; i < 25; ++i) {
    std::string contents0 = CreateAutoIndexedContent(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, contents0, url0) != NULL);
    std::string contents1 = CreateAutoIndexedContent(i + 50);
    GURL url1 = GURL(IndexedURL(i + 50));
    ASSERT_TRUE(AddNote(1, i, contents1, url1) != NULL);
  }
  for (int i = 25; i < 30; ++i) {
    std::string title0 = IndexedFolderName(i);
    const NoteNode* folder0 = AddFolder(0, i, title0);
    ASSERT_TRUE(folder0 != NULL);
    std::string title1 = IndexedFolderName(i + 50);
    const NoteNode* folder1 = AddFolder(1, i, title1);
    ASSERT_TRUE(folder1 != NULL);
    for (int j = 0; j < 5; ++j) {
      std::string content0 = CreateAutoIndexedContent(i + 5 * j);
      GURL url0 = GURL(IndexedURL(i + 5 * j));
      ASSERT_TRUE(AddNote(0, folder0, j, content0, url0) != NULL);
      std::string content1 = CreateAutoIndexedContent(i + 5 * j + 50);
      GURL url1 = GURL(IndexedURL(i + 5 * j + 50));
      ASSERT_TRUE(AddNote(1, folder1, j, content1, url1) != NULL);
    }
  }
  for (int i = 100; i < 125; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, content, url) != NULL);
    ASSERT_TRUE(AddNote(1, content, url) != NULL);
  }

  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3675271 - Merge simple note subset under note folder.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_MC_MergeSimpleNotesHierarchySubsetUnderNotesFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (int i = 0; i < 2; ++i) {
    const NoteNode* folder = AddFolder(i, kGenericFolderName);
    ASSERT_TRUE(folder != NULL);
    for (int j = 0; j < 4; ++j) {
      if (base::RandDouble() < 0.5) {
        std::string content = CreateAutoIndexedContent(j);
        GURL url = GURL(IndexedURL(j));
        ASSERT_TRUE(AddNote(i, folder, j, content, url) != NULL);
      } else {
        std::string title = IndexedFolderName(j);
        ASSERT_TRUE(AddFolder(i, folder, j, title) != NULL);
      }
    }
  }
  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3727284 - Merge subsets of note under note bar.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_MergeSimpleNotesHierarchySubset) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (int i = 0; i < 4; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
  }

  for (int j = 0; j < 2; ++j) {
    std::string content = CreateAutoIndexedContent(j);
    GURL url = GURL(IndexedURL(j));
    ASSERT_TRUE(AddNote(1, j, content, url) != NULL);
  }

  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
  ASSERT_FALSE(ContainsDuplicateNotes(1));
}

// TCM ID - 3659294 - Merge simple note hierarchy under note folder.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
    DISABLED_MC_Merge_SimpleNotesHierarchy_Under_NotesFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const NoteNode* folder0 = AddFolder(0, 0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 0, CreateAutoIndexedContent(1),
                      GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 2, CreateAutoIndexedContent(3),
                      GURL(IndexedURL(3))) != NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 3, IndexedSubfolderName(4)) != NULL);

  const NoteNode* folder1 = AddFolder(1, 0, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 0, IndexedSubfolderName(0)) != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 2, CreateAutoIndexedContent(3),
                      GURL(IndexedURL(3))) != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 3, IndexedSubfolderName(5)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 4, CreateAutoIndexedContent(1),
                      GURL(IndexedURL(1))) != NULL);

  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3711273 - Merge disjoint sets of note hierarchy under note
// folder.
IN_PROC_BROWSER_TEST_F(
    TwoClientNotesSyncTest,
    DISABLED_MC_Merge_SimpleNotesHierarchy_DisjointSets_Under_NotesFolder) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const NoteNode* folder0 = AddFolder(0, 0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 0, CreateAutoIndexedContent(1),
                      GURL(IndexedURL(1))) != NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 1, IndexedSubfolderName(2)) != NULL);
  ASSERT_TRUE(AddNote(0, folder0, 2, CreateAutoIndexedContent(3),
                      GURL(IndexedURL(3))) != NULL);
  ASSERT_TRUE(AddFolder(0, folder0, 3, IndexedSubfolderName(4)) != NULL);

  const NoteNode* folder1 = AddFolder(1, 0, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 0, IndexedSubfolderName(5)) != NULL);
  ASSERT_TRUE(AddFolder(1, folder1, 1, IndexedSubfolderName(6)) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 2, CreateAutoIndexedContent(7),
                      GURL(IndexedURL(7))) != NULL);
  ASSERT_TRUE(AddNote(1, folder1, 3, CreateAutoIndexedContent(8),
                      GURL(IndexedURL(8))) != NULL);

  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3639296 - Merge disjoint sets of note hierarchy
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_Merge_SimpleNotesHierarchy_DisjointSets) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i + 1);
    GURL url = GURL(IndexedURL(i + 1));
    ASSERT_TRUE(AddNote(0, i, content, url) != NULL);
  }

  for (int j = 0; j < 3; ++j) {
    std::string content = CreateAutoIndexedContent(j + 4);
    GURL url = GURL(IndexedURL(j + 4));
    ASSERT_TRUE(AddNote(0, j, content, url) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3616282 - Merge sets of duplicate notes.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_Merge_SimpleNotesHierarchy_DuplicateNotes) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Let's add duplicate set of note {1,2,2,3,3,3,4,4,4,4} to client0.
  int node_index = 0;
  for (int i = 1; i < 5; ++i) {
    for (int j = 0; j < i; ++j) {
      std::string content = CreateAutoIndexedContent(i);
      GURL url = GURL(IndexedURL(i));
      ASSERT_TRUE(AddNote(0, node_index, content, url) != NULL);
      ++node_index;
    }
  }
  // Let's add a set of notes {1,2,3,4} to client1.
  for (int i = 0; i < 4; ++i) {
    std::string content = CreateAutoIndexedContent(i + 1);
    GURL url = GURL(IndexedURL(i + 1));
    ASSERT_TRUE(AddNote(1, i, content, url) != NULL);
  }

  // Commit sequentially to make sure there is no race condition.
  ASSERT_TRUE(SetupSync());
  ASSERT_TRUE(AwaitQuiescence());

  for (int i = 1; i < 5; ++i) {
    ASSERT_TRUE(CountNotesWithContentMatching(1, CreateAutoIndexedContent(i)) ==
                i);
  }
}

// TCM ID - 6593872.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_DisableNotes) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(
      GetClient(1)->DisableSyncForType(syncer::UserSelectableType::kNotes));
  ASSERT_TRUE(AddFolder(1, kGenericFolderName) != NULL);
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(
      GetClient(1)->EnableSyncForType(syncer::UserSelectableType::kNotes));
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// TCM ID - 7343544.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_DisableSync) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->DisableSyncForAllDatatypes());
  ASSERT_TRUE(AddFolder(0, IndexedFolderName(0)) != NULL);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(AddFolder(1, IndexedFolderName(1)) != NULL);
  ASSERT_FALSE(AllModelsMatch());

  ASSERT_TRUE(GetClient(1)->EnableSyncForRegisteredDatatypes());
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

// TCM ID - 3662298 - Test adding duplicate folder - Both with different BMs
// underneath.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_MC_DuplicateFolders) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const NoteNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  const NoteNode* folder1 = AddFolder(1, kGenericFolderName);
  ASSERT_TRUE(folder1 != NULL);
  for (int i = 0; i < 5; ++i) {
    std::string content0 = CreateAutoIndexedContent(i);
    GURL url0 = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder0, i, content0, url0) != NULL);
    std::string content1 = CreateAutoIndexedContent(i + 5);
    GURL url1 = GURL(IndexedURL(i + 5));
    ASSERT_TRUE(AddNote(1, folder1, i, content1, url1) != NULL);
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// This test fails when run with FakeServer and FakeServerInvalidationService.
IN_PROC_BROWSER_TEST_F(LegacyTwoClientNotesSyncTest, DISABLED_MC_DeleteNote) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(
      GetClient(1)->DisableSyncForType(syncer::UserSelectableType::kNotes));

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

  ASSERT_TRUE(
      GetClient(1)->EnableSyncForType(syncer::UserSelectableType::kNotes));
  ASSERT_TRUE(AwaitQuiescence());

  ASSERT_FALSE(HasNodeWithURL(0, bar_url));
  ASSERT_TRUE(HasNodeWithURL(0, other_url));
  ASSERT_FALSE(HasNodeWithURL(1, bar_url));
  ASSERT_TRUE(HasNodeWithURL(1, other_url));
}

// TCM ID - 3719307 - Test a scenario of updating the name of the same note
// from two clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_NoteNameChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const NoteNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder0, i, content, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  GURL url(IndexedURL(0));
  SetTitle(0, GetUniqueNodeByURL(0, url), "Title++");
  SetTitle(1, GetUniqueNodeByURL(1, url), "Title--");

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_NoteContentChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const NoteNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    std::string title = IndexedURLTitle(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder0, i, content, title, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  GURL url(IndexedURL(0));
  SetContent(0, GetUniqueNodeByURL(0, url), "Content++");
  SetContent(1, GetUniqueNodeByURL(1, url), "Content--");

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// TCM ID - 3672299 - Test a scenario of updating the URL of the same note
// from two clients at the same time.
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_NoteURLChangeConflict) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";

  const NoteNode* folder0 = AddFolder(0, kGenericFolderName);
  ASSERT_TRUE(folder0 != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folder0, i, content, url) != NULL);
  }
  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

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
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_MC_FolderNameChangeConflict) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  const NoteNode* folderA[2];
  const NoteNode* folderB[2];
  const NoteNode* folderC[2];

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
    std::string content = CreateAutoIndexedContent(i);
    GURL url = GURL(IndexedURL(i));
    ASSERT_TRUE(AddNote(0, folderB[0], i, content, url) != NULL);
  }

  // Create folder C with notes and subfolders on both clients.
  folderC[0] = AddFolder(0, IndexedFolderName(2));
  ASSERT_TRUE(folderC[0] != NULL);
  folderC[1] = AddFolder(1, IndexedFolderName(2));
  ASSERT_TRUE(folderC[1] != NULL);
  for (int i = 0; i < 3; ++i) {
    std::string folder_name = IndexedSubfolderName(i);
    const NoteNode* subfolder = AddFolder(0, folderC[0], i, folder_name);
    ASSERT_TRUE(subfolder != NULL);
    for (int j = 0; j < 3; ++j) {
      std::string content = CreateAutoIndexedContent(j);
      GURL url = GURL(IndexedURL(j));
      ASSERT_TRUE(AddNote(0, subfolder, j, content, url) != NULL);
    }
  }

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  // Simultaneously rename folder A on both clients. We must retrieve the nodes
  // directly from the model as one of them will have been replaced during merge
  // for GUID reassignment.
  SetTitle(0, GetNotesTopNode(0)->children()[2].get(), "Folder A++");
  SetTitle(1, GetNotesTopNode(1)->children()[2].get(), "Folder A--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  // Simultaneously rename folder B on both clients. We must retrieve the nodes
  // directly from the model as one of them will have been replaced during merge
  // for GUID reassignment.
  SetTitle(0, GetNotesTopNode(0)->children()[1].get(), "Folder B++");
  SetTitle(1, GetNotesTopNode(1)->children()[1].get(), "Folder B--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));

  // Simultaneously rename folder C on both clients. We must retrieve the nodes
  // directly from the model as one of them will have been replaced during merge
  // for GUID reassignment.
  SetTitle(0, GetNotesTopNode(0)->children()[0].get(), "Folder C++");
  SetTitle(1, GetNotesTopNode(1)->children()[1].get(), "Folder C--");
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
  ASSERT_FALSE(ContainsDuplicateNotes(0));
}

// Deliberately racy rearranging of notes to test that our conflict resolver
// code results in a consistent view across machines (no matter what the final
// order is).
IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest, DISABLED_RacyPositionChanges) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  // Add initial notes.
  size_t num_notes = 5;
  for (size_t i = 0; i < num_notes; ++i) {
    ASSERT_TRUE(AddNote(0, i, CreateAutoIndexedContent(i),
                        GURL(IndexedURL(i))) != NULL);
  }

  // Once we make diverging changes the verifer is helpless.
  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  // Make changes on client 0.
  for (size_t i = 0; i < num_notes; ++i) {
    const NoteNode* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_notes - 1);
    DVLOG(1) << "Moving client 0's note " << i << " to position " << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  // Make changes on client 1.
  for (size_t i = 0; i < num_notes; ++i) {
    const NoteNode* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_notes - 1);
    DVLOG(1) << "Moving client 1's note " << i << " to position " << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  // Now make changes to client 1 first.
  for (size_t i = 0; i < num_notes; ++i) {
    const NoteNode* node = GetUniqueNodeByURL(1, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_notes - 1);
    DVLOG(1) << "Moving client 1's note " << i << " to position " << rand_pos;
    Move(1, node, node->parent(), rand_pos);
  }

  // Make changes on client 0.
  for (size_t i = 0; i < num_notes; ++i) {
    const NoteNode* node = GetUniqueNodeByURL(0, GURL(IndexedURL(i)));
    int rand_pos = base::RandInt(0, num_notes - 1);
    DVLOG(1) << "Moving client 0's note " << i << " to position " << rand_pos;
    Move(0, node, node->parent(), rand_pos);
  }

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());
}

IN_PROC_BROWSER_TEST_F(TwoClientNotesSyncTest,
                       DISABLED_NoteAllNodesRemovedEvent) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(AllModelsMatch());

  // Starting state:
  // root
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

  const NoteNode* folder0 = AddFolder(0, GetNotesTopNode(0), 0, "folder0");
  const NoteNode* tier1_a = AddFolder(0, folder0, 0, "tier1_a");
  ASSERT_TRUE(AddNote(0, folder0, 1, "News", GURL("http://news.google.com")));
  ASSERT_TRUE(AddNote(0, folder0, 2, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AddNote(0, tier1_a, 0, "Gmai", GURL("http://mail.google.com")));
  ASSERT_TRUE(AddNote(0, tier1_a, 1, "Google", GURL("http://www.google.com")));
  ASSERT_TRUE(
      AddNote(0, GetNotesTopNode(0), 1, "CNN", GURL("http://www.cnn.com")));

  ASSERT_TRUE(AddFolder(0, GetNotesTopNode(0), 0, "empty_folder"));
  const NoteNode* folder1 = AddFolder(0, GetNotesTopNode(0), 1, "folder1");
  ASSERT_TRUE(AddNote(0, folder1, 0, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(
      AddNote(0, GetNotesTopNode(0), 2, "Gmail", GURL("http://gmail.com")));

  ASSERT_TRUE(AwaitQuiescence());
  ASSERT_TRUE(AllModelsMatch());

  // Remove all
  RemoveAll(0);

  ASSERT_TRUE(GetClient(0)->AwaitMutualSyncCycleCompletion(GetClient(1)));
  // Verify other node has no children now.
  EXPECT_EQ(0u, GetNotesTopNode(0)->children().size());
  EXPECT_EQ(0u, GetNotesTopNode(0)->children().size());
  ASSERT_TRUE(AllModelsMatch());
}
