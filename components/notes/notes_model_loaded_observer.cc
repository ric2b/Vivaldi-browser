// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notes/notes_model_loaded_observer.h"

#include "app/vivaldi_apptools.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "components/notes/notes_model.h"

namespace vivaldi {

NotesModelLoadedObserver::NotesModelLoadedObserver(Profile* profile,
                                                   NotesModel* model)
    : profile_(profile) {
  CHECK(model);
  observation_.Observe(model);
}

NotesModelLoadedObserver::~NotesModelLoadedObserver() = default;

void NotesModelLoadedObserver::NotesModelLoaded(bool ids_reassigned) {
  if (vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning()) {
    // Causes lazy-load if sync is enabled.
    SyncServiceFactory::GetInstance()->GetForProfile(profile_);
  }
  delete this;
}

void NotesModelLoadedObserver::NotesModelBeingDeleted() {
  delete this;
}

}  // namespace vivaldi
