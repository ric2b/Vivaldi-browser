// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTES_NOTES_MODEL_LOADED_OBSERVER_H_
#define COMPONENTS_NOTES_NOTES_MODEL_LOADED_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/notes/notes_model_observer.h"

class Profile;

namespace vivaldi {
class NotesModel;

class NotesModelLoadedObserver : public NotesModelObserver {
 public:
  NotesModelLoadedObserver(Profile* profile, NotesModel* model);
  NotesModelLoadedObserver(const NotesModelLoadedObserver&) = delete;
  ~NotesModelLoadedObserver() override;

  NotesModelLoadedObserver& operator=(const NotesModelLoadedObserver&) = delete;

 private:
  void NotesModelLoaded(bool ids_reassigned) override;
  void NotesModelBeingDeleted() override;

  const raw_ptr<Profile> profile_;
  base::ScopedObservation<NotesModel, NotesModelObserver> observation_{this};
};

}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_NOTES_MODEL_LOADED_OBSERVER_H_
