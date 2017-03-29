// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "notes/notes_model_loaded_observer.h"

#include "notes/notes_model.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"

using namespace Vivaldi;

NotesModelLoadedObserver::NotesModelLoadedObserver(Profile* profile)
    : profile_(profile) {
}

void NotesModelLoadedObserver::NotesModelChanged() {
}

void NotesModelLoadedObserver::Loaded(Notes_Model* model,
                                         bool ids_reassigned) {
  // Causes lazy-load if sync is enabled.
  ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  model->RemoveObserver(this);
  delete this;
}

void NotesModelLoadedObserver::NotesModelBeingDeleted(
    Notes_Model* model) {
  model->RemoveObserver(this);
  delete this;
}


