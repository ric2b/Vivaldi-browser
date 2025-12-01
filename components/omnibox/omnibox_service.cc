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
#include "components/omnibox/browser/keyword_provider.h"
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

void OmniboxService::StartSearch(
        std::u16string input_text,
        OmniboxPrivateInput input,
        metrics::OmniboxEventProto::PageClassification page_classification) {
  AutocompleteInput autocomplete_input(
      input_text, page_classification,
      ChromeAutocompleteSchemeClassifier(profile_));
  autocomplete_input.set_prevent_inline_autocomplete(
      input.prevent_inline_autocomplete);
  autocomplete_input.set_focus_type(input.focus_type);
  autocomplete_input.from_search_field = input.from_search_field;
  autocomplete_input.search_engine_guid = input.search_engine_guid;
  //Check if the input starts with a valid search engine Keyword and
  // if it is enable the keyword mode.
  TemplateURLService* template_URL_service =
      std::make_unique<ChromeAutocompleteProviderClient>
      (profile_)->GetTemplateURLService();
  if(0 < static_cast<int>(input_text.find_first_of(base::kWhitespaceUTF16)) &&
      !controller_->keyword_provider()->
        GetKeywordForText(input_text,template_URL_service).empty())
    autocomplete_input.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto::SPACE_IN_MIDDLE);
  else
    autocomplete_input.set_keyword_mode_entry_method(
      metrics::OmniboxEventProto::INVALID);

  if (input.clear_state_before_searching) {
    controller_->RemoveObserver(this);
    controller_->Stop(AutocompleteStopReason::kClobbered);
    controller_->AddObserver(this);
  }

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
