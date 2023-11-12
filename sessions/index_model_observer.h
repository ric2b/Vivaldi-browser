// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SESSIONS_INDEX_MODEL_OBSERVER_H_
#define SESSIONS_INDEX_MODEL_OBSERVER_H_

#include "base/observer_list_types.h"

namespace sessions {

class Index_Node;
class Index_Model;

class IndexModelObserver : public base::CheckedObserver {
 public:
  // Invoked when the model has finished loading
  virtual void IndexModelLoaded(Index_Model* model) {}

  // Invoked when the model has been changed after initial loading.
  virtual void IndexModelNodeChanged(Index_Model* model, Index_Node* node) {}
  virtual void IndexModelNodeAdded(Index_Model* model, Index_Node* node,
                                   int64_t parent_id, size_t index,
                                   const std::string& owner) {}
  virtual void IndexModelNodeMoved(Index_Model* model, int64_t id,
                                   int64_t parent_id, size_t index) {}
  virtual void IndexModelNodeRemoved(Index_Model* model, int64_t id) {}

  // Invoked from the destructor of the MenuModel.
  virtual void IndexModelBeingDeleted(Index_Model* model) {}
};

}  // namespace sessions

#endif  // SESSIONS_INDEX_MODEL_OBSERVER_H_
