// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes/notes_model_loaded_observer.h"

#include "app/vivaldi_apptools.h"
#include "notes/notes_model.h"

#include "chrome/browser/sync/profile_sync_service_factory.h"

namespace vivaldi {

NotesModelLoadedObserver::NotesModelLoadedObserver(Profile* profile)
    : profile_(profile) {}

void NotesModelLoadedObserver::NotesModelLoaded(Notes_Model* model,
                                                bool ids_reassigned) {
  if (vivaldi::IsVivaldiRunning()) {
    // Causes lazy-load if sync is enabled.
    ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  }
  model->RemoveObserver(this);
  delete this;
}

void NotesModelLoadedObserver::NotesModelBeingDeleted(Notes_Model* model) {
  model->RemoveObserver(this);
  delete this;
}

}  // namespace vivaldi
