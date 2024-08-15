// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notes/note_load_details.h"
#include "base/uuid.h"

namespace vivaldi {

NoteLoadDetails::NoteLoadDetails() {
  root_node_ = std::make_unique<NoteNode>(
      0, base::Uuid::ParseLowercase(NoteNode::kRootNodeUuid), NoteNode::FOLDER);

  // WARNING: order is important here, various places assume the order is
  // constant (but can vary between embedders with the initial visibility
  // of permanent nodes).

  main_notes_node_ = static_cast<PermanentNoteNode*>(
      root_node_->Add(PermanentNoteNode::CreateMainNotes(max_id_++)));
  other_notes_node_ = static_cast<PermanentNoteNode*>(
      root_node_->Add(PermanentNoteNode::CreateOtherNotes(max_id_++)));
  trash_notes_node_ = static_cast<PermanentNoteNode*>(
      root_node_->Add(PermanentNoteNode::CreateNoteTrash(max_id_++)));
}

NoteLoadDetails::~NoteLoadDetails() {}

}  // namespace vivaldi
