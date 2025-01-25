// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "omnibox_service.h"

#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/omnibox_service_observer.h"
#include "omnibox_service.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"

namespace vivaldi_omnibox {

OmniboxService::OmniboxService(Profile* profile) : profile_(profile) {
  controller_ = std::make_unique<AutocompleteController>(
      std::make_unique<ChromeAutocompleteProviderClient>(profile_),
      AutocompleteClassifier::DefaultOmniboxProviders(), false);

  controller_->AddObserver(this);
}

void OmniboxService::Shutdown() {
  controller_->RemoveObserver(this);
  controller_.reset();
}

void OmniboxService::StartSearch(std::u16string input_text) {
  auto page_classification = ::metrics::OmniboxEventProto::INVALID_SPEC;
  AutocompleteInput autocomplete_input(
      input_text, page_classification,
      ChromeAutocompleteSchemeClassifier(profile_));
  controller_->Start(autocomplete_input);
}

void OmniboxService::OnResultChanged(AutocompleteController* controller,
                                     bool default_match_changed) {
  DCHECK_EQ(controller, controller_.get());
  for (OmniboxServiceObserver& observer : observers_) {
    observer.OnResultChanged(controller, default_match_changed);
  }
}

void OmniboxService::AddObserver(OmniboxServiceObserver* observer) {
  observers_.AddObserver(observer);
}

void OmniboxService::RemoveObserver(OmniboxServiceObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace vivaldi_omnibox
