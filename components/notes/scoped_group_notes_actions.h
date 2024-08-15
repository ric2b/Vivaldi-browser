// Copyright (c) 2015-2017 Vivaldi Technologies AS. All rights reserved
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NOTES_SCOPED_GROUP_NOTES_ACTIONS_H_
#define COMPONENTS_NOTES_SCOPED_GROUP_NOTES_ACTIONS_H_

#include "base/memory/raw_ptr.h"

namespace vivaldi {

class NotesModel;

// Scopes the grouping of a set of changes into one undoable action.
class ScopedGroupNotesActions {
 public:
  explicit ScopedGroupNotesActions(NotesModel* model);
  ~ScopedGroupNotesActions();
  ScopedGroupNotesActions(const ScopedGroupNotesActions&) = delete;
  ScopedGroupNotesActions& operator=(const ScopedGroupNotesActions&) = delete;

 private:
  const raw_ptr<NotesModel> model_;
};
}  // namespace vivaldi

#endif  // COMPONENTS_NOTES_SCOPED_GROUP_NOTES_ACTIONS_H_
