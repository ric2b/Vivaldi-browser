// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NOTES_NOTES_MODEL_LOADED_OBSERVER_H_
#define NOTES_NOTES_MODEL_LOADED_OBSERVER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "notes/notes_model_observer.h"

class Profile;

namespace vivaldi {

class NotesModelLoadedObserver : public NotesModelObserver {
 public:
  explicit NotesModelLoadedObserver(Profile* profile);

 private:
  void NotesModelLoaded(Notes_Model* model, bool ids_reassigned) override;
  void NotesModelBeingDeleted(Notes_Model* model) override;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(NotesModelLoadedObserver);
};

}  // namespace vivaldi

#endif  // NOTES_NOTES_MODEL_LOADED_OBSERVER_H_
