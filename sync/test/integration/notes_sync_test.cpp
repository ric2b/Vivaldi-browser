// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "sync/test/integration/notes_sync_test.h"

#include "base/strings/utf_string_conversions.h"
#include "components/sync/test/fake_server/fake_server_verifier.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notes_model_observer.h"
#include "sync/test/integration/notes_helper.h"

using notes_helper::GetNotesModel;
using vivaldi::Notes_Model;
using vivaldi::NotesModelObserver;
using vivaldi::NotesModelFactory;

// NotesLoadObserver is used when blocking until the Notes_Model finishes
// loading. As soon as the Notes_Model finishes loading the message loop is
// quit.
class NotesLoadObserver : public NotesModelObserver {
 public:
  explicit NotesLoadObserver(const base::Closure& quit_task);
  ~NotesLoadObserver() override;

 private:
  // NotesBaseModelObserver:
  void NotesModelLoaded(Notes_Model* model, bool ids_reassigned) override;

  base::Closure quit_task_;

  DISALLOW_COPY_AND_ASSIGN(NotesLoadObserver);
};

NotesLoadObserver::NotesLoadObserver(const base::Closure& quit_task)
    : quit_task_(quit_task) {}

NotesLoadObserver::~NotesLoadObserver() {}

void NotesLoadObserver::NotesModelLoaded(Notes_Model* model,
                                         bool ids_reassigned) {
  quit_task_.Run();
}

void WaitForNotesModelToLoad(Notes_Model* model) {
  if (model->loaded())
    return;
  base::RunLoop run_loop;
  base::MessageLoop::ScopedNestableTaskAllower allow;

  NotesLoadObserver observer(run_loop.QuitClosure());
  model->AddObserver(&observer);
  run_loop.Run();
  model->RemoveObserver(&observer);
  DCHECK(model->loaded());
}

NotesSyncTest::NotesSyncTest(TestType test_type) : SyncTest(test_type) {}

NotesSyncTest::~NotesSyncTest() {}

bool NotesSyncTest::SetupClients() {
  if (!SyncTest::SetupClients())
    return false;

  if (use_verifier())
    WaitForNotesModelToLoad(NotesModelFactory::GetForBrowserContext(verifier()));

  return true;
}

void NotesSyncTest::WaitForDataModels(Profile* profile) {
  SyncTest::WaitForDataModels(profile);
  WaitForNotesModelToLoad(NotesModelFactory::GetForBrowserContext(profile));
}

void NotesSyncTest::VerifyNotesModelMatchesFakeServer(int index) {
  fake_server::FakeServerVerifier fake_server_verifier(GetFakeServer());
  std::vector<Notes_Model::URLAndTitle> local_notes;
  GetNotesModel(index)->GetNotes(&local_notes);

  // Verify that all local notes titles exist once on the server.
  std::vector<Notes_Model::URLAndTitle>::const_iterator it;
  for (it = local_notes.begin(); it != local_notes.end(); ++it) {
    ASSERT_TRUE(fake_server_verifier.VerifyEntityCountByTypeAndName(
        1, syncer::NOTES, base::UTF16ToUTF8(it->title)));
  }
}
