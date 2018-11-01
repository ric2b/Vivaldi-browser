// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NOTES_SCOPED_GROUP_NOTES_ACTIONS_H_
#define NOTES_SCOPED_GROUP_NOTES_ACTIONS_H_

#include "base/macros.h"

namespace vivaldi {

class Notes_Model;

// Scopes the grouping of a set of changes into one undoable action.
class ScopedGroupNotesActions {
 public:
  explicit ScopedGroupNotesActions(Notes_Model* model);
  ~ScopedGroupNotesActions();

 private:
  Notes_Model* model_;

  DISALLOW_COPY_AND_ASSIGN(ScopedGroupNotesActions);
};
}  // namespace vivaldi

#endif  // NOTES_SCOPED_GROUP_NOTES_ACTIONS_H_
