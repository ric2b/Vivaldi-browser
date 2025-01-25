// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_OBSERVER_H_
#define COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_OBSERVER_H_

namespace vivaldi_omnibox {

class OmniboxServiceObserver {
 public:
  virtual ~OmniboxServiceObserver() = default;

  virtual void OnResultChanged(AutocompleteController* controller,
                               bool default_match_changed) = 0;
};

}  // namespace vivaldi_omnibox

#endif  // COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_OBSERVER_H_
