// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_OMNIBOX_INPUT_H_
#define COMPONENTS_OMNIBOX_OMNIBOX_INPUT_H_

namespace vivaldi_omnibox {

class OmniboxPrivateInput {
 public:
  bool clear_state_before_searching;
  bool prevent_inline_autocomplete;
  bool from_search_field;
  std::string search_engine_guid;
  metrics::OmniboxFocusType focus_type;
};

}  // namespace vivaldi_omnibox
#endif  // COMPONENTS_OMNIBOX_OMNIBOX_INPUT_H_"
