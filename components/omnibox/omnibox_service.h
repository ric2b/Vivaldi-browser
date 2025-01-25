// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_H_
#define COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_H_

#include "components/omnibox/browser/autocomplete_controller.h"

namespace vivaldi_omnibox {

class OmniboxServiceObserver;

class OmniboxService : public KeyedService,
                       public AutocompleteController::Observer {
 public:
  OmniboxService(Profile* profile);

  Profile* profile_;
  base::ObserverList<OmniboxServiceObserver>::Unchecked observers_;

  //  Called from shutdown service before shutting down the browser
  void Shutdown() override;

  void StartSearch(std::u16string input);

  void OnResultChanged(AutocompleteController* controller,
                       bool default_match_changed) override;

  // Adds/Removes an Observer.
  void AddObserver(OmniboxServiceObserver* observer);
  void RemoveObserver(OmniboxServiceObserver* observer);

 private:
  std::unique_ptr<AutocompleteController> controller_;
};

}  // namespace vivaldi_omnibox

#endif  //  COMPONENTS_OMNIBOX_OMNIBOX_SERVICE_H_
