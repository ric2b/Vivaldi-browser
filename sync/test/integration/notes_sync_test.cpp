// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved

#include "sync/test/integration/notes_sync_test.h"

#include "app/vivaldi_apptools.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "components/notes/notes_factory.h"
#include "components/notes/notes_model.h"
#include "components/notes/notes_model_observer.h"
#include "components/sync/test/fake_server_verifier.h"
#include "sync/test/integration/notes_helper.h"

using notes_helper::GetNotesModel;
using vivaldi::NotesModel;
using vivaldi::NotesModelFactory;
using vivaldi::NotesModelObserver;

// NotesLoadObserver is used when blocking until the NotesModel finishes
// loading. As soon as the NotesModel finishes loading the message loop is
// quit.
class NotesLoadObserver : public NotesModelObserver {
 public:
  explicit NotesLoadObserver(base::OnceClosure quit_task);
  ~NotesLoadObserver() override;
  NotesLoadObserver(const NotesLoadObserver&) = delete;
  NotesLoadObserver& operator=(const NotesLoadObserver&) = delete;

 private:
  // NotesBaseModelObserver:
  void NotesModelLoaded(bool ids_reassigned) override;

  base::OnceClosure quit_task_;
};

NotesLoadObserver::NotesLoadObserver(base::OnceClosure quit_task)
    : quit_task_(std::move(quit_task)) {}

NotesLoadObserver::~NotesLoadObserver() {}

void NotesLoadObserver::NotesModelLoaded(bool ids_reassigned) {
  std::move(quit_task_).Run();
}

void WaitForNotesModelToLoad(NotesModel* model) {
  if (model->loaded())
    return;
  base::RunLoop run_loop;
  base::CurrentThread::ScopedAllowApplicationTasksInNativeNestedLoop allow;

  NotesLoadObserver observer(run_loop.QuitClosure());
  model->AddObserver(&observer);
  run_loop.Run();
  model->RemoveObserver(&observer);
  DCHECK(model->loaded());
}

NotesSyncTest::NotesSyncTest(TestType test_type) : SyncTest(test_type) {}

NotesSyncTest::~NotesSyncTest() {}

bool NotesSyncTest::UseVerifier() {
  return true;
}

void NotesSyncTest::SetUp() {
  vivaldi::ForceVivaldiRunning(true);
  SyncTest::SetUp();
}

void NotesSyncTest::TearDown() {
  SyncTest::TearDown();
  vivaldi::ForceVivaldiRunning(false);
}

bool NotesSyncTest::SetupClients() {
  if (!SyncTest::SetupClients())
    return false;

  if (UseVerifier())
    WaitForNotesModelToLoad(
        NotesModelFactory::GetForBrowserContext(verifier()));

  return true;
}

void NotesSyncTest::WaitForDataModels(Profile* profile) {
  SyncTest::WaitForDataModels(profile);
  WaitForNotesModelToLoad(NotesModelFactory::GetForBrowserContext(profile));
}

void NotesSyncTest::VerifyNotesModelMatchesFakeServer(int index) {
  fake_server::FakeServerVerifier fake_server_verifier(GetFakeServer());
  std::vector<NotesModel::URLAndTitle> local_notes;
  GetNotesModel(index)->GetNotes(&local_notes);

  // Verify that all local notes titles exist once on the server.
  std::vector<NotesModel::URLAndTitle>::const_iterator it;
  for (it = local_notes.begin(); it != local_notes.end(); ++it) {
    if (it->title.empty())
      continue;
    ASSERT_TRUE(fake_server_verifier.VerifyEntityCountByTypeAndName(
        1, syncer::NOTES, base::UTF16ToUTF8(it->title)));
  }
}
