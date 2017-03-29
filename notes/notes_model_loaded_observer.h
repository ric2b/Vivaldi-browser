// Copyright (c) 2013 Vivaldi Technologies AS. All rights reserved
// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIVALDI_NOTES_NOTES_MODEL_LOADED_OBSERVER_H_
#define VIVALDI_NOTES_NOTES_MODEL_LOADED_OBSERVER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "notes/notes_base_model_observer.h"

class Profile;

class NotesModelLoadedObserver : public Vivaldi::NotesBaseModelObserver {
 public:
  explicit NotesModelLoadedObserver(Profile* profile);

 private:
  void NotesModelChanged() override;
  void Loaded(Vivaldi::Notes_Model* model, bool ids_reassigned) override;
  void NotesModelBeingDeleted(Vivaldi::Notes_Model* model) override;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(NotesModelLoadedObserver);
};

#endif  // VIVALDI_NOTES_NOTES_MODEL_LOADED_OBSERVER_H_
