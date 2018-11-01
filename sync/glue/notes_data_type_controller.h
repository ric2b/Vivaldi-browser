// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_GLUE_NOTES_DATA_TYPE_CONTROLLER_H_
#define SYNC_GLUE_NOTES_DATA_TYPE_CONTROLLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/scoped_observer.h"
#include "components/sync/driver/frontend_data_type_controller.h"
#include "notes/notes_model_observer.h"

namespace vivaldi {

// A class that manages the startup and shutdown of notes sync.
class NotesDataTypeController : public syncer::FrontendDataTypeController,
                                public vivaldi::NotesModelObserver {
 public:
  NotesDataTypeController(const base::Closure& dump_stack,
                          syncer::SyncClient* sync_client);
  ~NotesDataTypeController() override;

 private:
  // FrontendDataTypeController interface.
  bool StartModels() override;
  void CleanUpState() override;
  void CreateSyncComponents() override;

  // NotesModelObserver interface.
  void NotesModelLoaded(Notes_Model* model, bool ids_reassigned) override;
  void NotesModelBeingDeleted(Notes_Model* model) override;

  // Helper that returns true iff the notes model have finished loading.
  bool DependentsLoaded();

  ScopedObserver<Notes_Model, NotesModelObserver> notes_model_observer_;

  DISALLOW_COPY_AND_ASSIGN(NotesDataTypeController);
};

}  // namespace vivaldi

#endif  // SYNC_GLUE_NOTES_DATA_TYPE_CONTROLLER_H_
