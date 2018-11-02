// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/updated_progress_marker_checker.h"
#include "components/sync/test/fake_server/entity_builder_factory.h"
#include "notes/notes_model.h"
#include "notes/notesnode.h"
#include "sync/test/fake_server/notes_entity_builder.h"
#include "sync/test/integration/notes_helper.h"
#include "sync/test/integration/notes_sync_test.h"
#include "ui/base/layout.h"

using notes_helper::AddFolder;
using notes_helper::AddNote;
using notes_helper::CountNotesWithTitlesMatching;
using notes_helper::GetNotesModel;
using notes_helper::GetNotesTopNode;
using notes_helper::ModelMatchesVerifier;
using notes_helper::Move;
using notes_helper::Remove;
using notes_helper::RemoveAll;
using notes_helper::SetTitle;

class SingleClientNotesSyncTest : public NotesSyncTest {
 public:
  SingleClientNotesSyncTest() : NotesSyncTest(SINGLE_CLIENT) {}
  ~SingleClientNotesSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientNotesSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientNotesSyncTest, Sanity) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";

  // Starting state:
  //    -> top
  //      -> tier1_a
  //        -> http://mail.google.com  "tier1_a_url0"
  //        -> http://www.pandora.com  "tier1_a_url1"
  //        -> http://www.facebook.com "tier1_a_url2"
  //      -> tier1_b
  //        -> http://www.nhl.com "tier1_b_url0"
  //        -> http://www.vg.no "tier1_b_url1"
  //    -> trash
  //      -> http://www.microsoft.com "trash_1_url0"
  const Notes_Node* top = AddFolder(0, GetNotesTopNode(0), 0, "top");
  const Notes_Node* tier1_a = AddFolder(0, top, 0, "tier1_a");
  const Notes_Node* tier1_b = AddFolder(0, top, 1, "tier1_b");
  const Notes_Node* tier1_a_url0 =
      AddNote(0, tier1_a, 0, "tier1_a_url0", GURL("http://mail.google.com"));
  const Notes_Node* tier1_a_url1 =
      AddNote(0, tier1_a, 1, "tier1_a_url1", GURL("http://www.pandora.com"));
  const Notes_Node* tier1_a_url2 =
      AddNote(0, tier1_a, 2, "tier1_a_url2", GURL("http://www.facebook.com"));
  const Notes_Node* tier1_b_url0 =
      AddNote(0, tier1_b, 0, "tier1_b_url0", GURL("http://www.nhl.com"));
  const Notes_Node* tier1_b_url1 =
      AddNote(0, tier1_b, 0, "tier1_b_url1", GURL("http://www.vg.no"));

  Notes_Node* trash_node = GetNotesModel(0)->trash_node();
  const Notes_Node* trash_1_url0 = AddNote(0, trash_node, 0, "trash_1_url0",
                                           GURL("http://www.microsoft.com"));

  // Setup sync, wait for its completion, and make sure changes were synced.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  //  Ultimately we want to end up with the following model; but this test is
  //  more about the journey than the destination.
  //
  //  -> top
  //    -> CNN (www.cnn.com)
  //    -> tier1_a
  //      -> tier1_a_url2 (www.facebook.com)
  //      -> tier1_a_url1 (www.pandora.com)
  //    -> Porsche (www.porsche.com)
  //    -> Bank of America (www.bankofamerica.com)
  //    -> tier1_b
  //      -> Wired News (www.wired.com)
  //      -> tier2_b
  //        -> tier1_b_url0
  //        -> tier3_b
  //          -> Toronto Maple Leafs (mapleleafs.nhl.com)
  //          -> Wynn (www.wynnlasvegas.com)
  //    -> Seattle Bubble
  //    -> tier1_a_url0
  //  -> trash
  //    -> http://www.microsoft.com "trash_1_url0"
  //    -> http://www.vg.no "tier1_b_url1"
  const Notes_Node* cnn = AddNote(0, top, 0, "CNN", GURL("http://www.cnn.com"));
  ASSERT_TRUE(cnn != NULL);
  Move(0, tier1_a, top, 1);

  // Wait for the Notes position change to sync.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  const Notes_Node* porsche =
      AddNote(0, top, 2, "Porsche", GURL("http://www.porsche.com"));
  // Rearrange stuff in tier1_a.
  ASSERT_EQ(tier1_a, tier1_a_url2->parent());
  ASSERT_EQ(tier1_a, tier1_a_url1->parent());
  Move(0, tier1_a_url2, tier1_a, 0);
  Move(0, tier1_a_url1, tier1_a, 2);

  // Wait for the rearranged hierarchy to sync.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  ASSERT_EQ(1, tier1_a_url0->parent()->GetIndexOf(tier1_a_url0));
  Move(0, tier1_a_url0, top, top->child_count());
  const Notes_Node* boa = AddNote(0, top, top->child_count(), "Bank of America",
                                  GURL("https://www.bankofamerica.com"));
  ASSERT_TRUE(boa != NULL);
  const Notes_Node* bubble =
      AddNote(0, top, top->child_count(), "Seattle Bubble",
              GURL("http://seattlebubble.com"));
  Move(0, tier1_a_url0, top, top->child_count());
  ASSERT_TRUE(bubble != NULL);
  const Notes_Node* wired =
      AddNote(0, top, 2, "Wired News", GURL("http://www.wired.com"));
  const Notes_Node* tier2_b = AddFolder(0, tier1_b, 0, "tier2_b");
  Move(0, tier1_b_url0, tier2_b, 0);
  Move(0, porsche, top, 0);
  SetTitle(0, wired, "News Wired");
  SetTitle(0, porsche, "ICanHazPorsche?");

  // Wait for the title change to sync.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  ASSERT_EQ(tier1_a_url0->id(), top->GetChild(top->child_count() - 1)->id());
  Remove(0, top, top->child_count() - 1);
  Move(0, wired, tier1_b, 0);
  Move(0, porsche, top, 3);
  const Notes_Node* tier3_b = AddFolder(0, tier2_b, 1, "tier3_b");
  const Notes_Node* leafs = AddNote(0, tier1_a, 0, "Toronto Maple Leafs",
                                    GURL("http://mapleleafs.nhl.com"));
  const Notes_Node* wynn =
      AddNote(0, top, 1, "Wynn", GURL("http://www.wynnlasvegas.com"));

  Move(0, wynn, tier3_b, 0);
  Move(0, leafs, tier3_b, 0);

  // Wait for newly added notes to sync.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  Move(0, tier1_b_url1, trash_node, 1);

  // Wait for newly deleted notes to sync.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  ASSERT_EQ(trash_node->GetChild(0)->id(), trash_1_url0->id());
  Remove(0, trash_node, 0);
  ASSERT_EQ(trash_node->GetChild(0)->id(), tier1_b_url1->id());

  // Wait for newly deleted notes to sync.
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Only verify FakeServer data if FakeServer is being used.
  // TODO(pvalenzuela): Use this style of verification in more tests once it is
  // proven stable.
  ASSERT_TRUE(GetFakeServer());
  if (GetFakeServer())
    VerifyNotesModelMatchesFakeServer(0);
}

IN_PROC_BROWSER_TEST_F(SingleClientNotesSyncTest, InjectedNote) {
  std::string title = "Montreal Canadiens";
  fake_server::EntityBuilderFactory entity_builder_factory;
  fake_server::NotesEntityBuilder notes_builder =
      entity_builder_factory
          .NewNotesEntityBuilder(title, GURL("http://canadiens.nhl.com"),
                                 std::string());
  fake_server_->InjectEntity(notes_builder.Build());

  DisableVerifier();
  ASSERT_TRUE(SetupClients());
  ASSERT_TRUE(SetupSync());

  ASSERT_EQ(1, CountNotesWithTitlesMatching(0, title));
}

IN_PROC_BROWSER_TEST_F(SingleClientNotesSyncTest, NotesAllNodesRemovedEvent) {
  ASSERT_TRUE(SetupClients()) << "SetupClients() failed.";
  // Starting state:
  //    -> folder0
  //      -> tier1_a
  //        -> http://mail.google.com
  //        -> http://www.google.com
  //      -> http://news.google.com
  //      -> http://yahoo.com
  //    -> http://www.cnn.com

  const Notes_Node* folder0 = AddFolder(0, GetNotesTopNode(0), 0, "folder0");
  const Notes_Node* tier1_a = AddFolder(0, folder0, 0, "tier1_a");
  ASSERT_TRUE(AddNote(0, folder0, 1, "News", GURL("http://news.google.com")));
  ASSERT_TRUE(AddNote(0, folder0, 2, "Yahoo", GURL("http://www.yahoo.com")));
  ASSERT_TRUE(AddNote(0, tier1_a, 0, "Gmai", GURL("http://mail.google.com")));
  ASSERT_TRUE(AddNote(0, tier1_a, 1, "Google", GURL("http://www.google.com")));
  ASSERT_TRUE(
      AddNote(0, GetNotesTopNode(0), 1, "CNN", GURL("http://www.cnn.com")));

  // Set up sync, wait for its completion and verify that changes propagated.
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  ASSERT_TRUE(ModelMatchesVerifier(0));

  // Remove all notes and wait for sync completion.
  RemoveAll(0);
  ASSERT_TRUE(UpdatedProgressMarkerChecker(GetSyncService(0)).Wait());
  // Verify other node has no children now.
  EXPECT_EQ(0, GetNotesTopNode(0)->child_count());
  // Verify model matches verifier.
  ASSERT_TRUE(ModelMatchesVerifier(0));
}
