// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/glue/notes_data_type_controller.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/sync/driver/sync_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "notes/notes_factory.h"
#include "notes/notes_model.h"
#include "notes/notesnode.h"

using content::BrowserThread;

namespace vivaldi {

NotesDataTypeController::NotesDataTypeController(
    const base::Closure& dump_stack,
    syncer::SyncClient* sync_client)
    : FrontendDataTypeController(syncer::NOTES, dump_stack, sync_client),
      notes_model_observer_(this) {}

NotesDataTypeController::~NotesDataTypeController() {}

bool NotesDataTypeController::StartModels() {
  if (!DependentsLoaded()) {
    Notes_Model* notes_model = sync_client_->GetNotesModel();
    notes_model_observer_.Add(notes_model);
    return false;
  }
  return true;
}

void NotesDataTypeController::CleanUpState() {
  notes_model_observer_.RemoveAll();
}

void NotesDataTypeController::CreateSyncComponents() {
  syncer::SyncApiComponentFactory::SyncComponents sync_components =
      sync_client_->GetSyncApiComponentFactory()->CreateNotesSyncComponents(
          sync_client_->GetSyncService(), CreateErrorHandler());
  set_model_associator(sync_components.model_associator);
  set_change_processor(sync_components.change_processor);
}

void NotesDataTypeController::NotesModelLoaded(Notes_Model* model,
                                               bool ids_reassigned) {
  DCHECK(model->loaded());
  notes_model_observer_.RemoveAll();

  if (!DependentsLoaded())
    return;

  OnModelLoaded();
}

void NotesDataTypeController::NotesModelBeingDeleted(Notes_Model* model) {
  CleanUpState();
}

// Check that both the notes model is loaded.
bool NotesDataTypeController::DependentsLoaded() {
  Notes_Model* notes_model = sync_client_->GetNotesModel();
  if (!notes_model || !notes_model->loaded())
    return false;

  // All necessary services are loaded.
  return true;
}

}  // namespace vivaldi
