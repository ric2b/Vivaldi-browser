// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_util.h"

#include <stddef.h>

#include <algorithm>
#include <numeric>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/dense_set.h"

namespace autofill {

using mojom::FocusedFieldType;
using mojom::SubmissionIndicatorEvent;
using mojom::SubmissionSource;

namespace {

constexpr std::u16string_view kSplitCharacters = u" .,-_@";

template <typename Char>
struct Compare : base::CaseInsensitiveCompareASCII<Char> {
  explicit Compare(bool case_sensitive) : case_sensitive_(case_sensitive) {}
  bool operator()(Char x, Char y) const {
    return case_sensitive_
               ? (x == y)
               : base::CaseInsensitiveCompareASCII<Char>::operator()(x, y);
  }

 private:
  bool case_sensitive_;
};

}  // namespace

bool IsShowAutofillSignaturesEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kShowAutofillSignatures);
}

bool IsPrefixOfEmailEndingWithAtSign(const std::u16string& full_string,
                                     const std::u16string& prefix) {
  return full_string.starts_with(prefix + u"@");
}

size_t GetTextSelectionStart(const std::u16string& suggestion,
                             const std::u16string& field_contents,
                             bool case_sensitive) {
  // Loop until we find either the |field_contents| is a prefix of |suggestion|
  // or character right before the match is one of the splitting characters.
  for (std::u16string::const_iterator it = suggestion.begin();
       (it = std::search(
            it, suggestion.end(), field_contents.begin(), field_contents.end(),
            Compare<std::u16string::value_type>(case_sensitive))) !=
       suggestion.end();
       ++it) {
    if (it == suggestion.begin() ||
        kSplitCharacters.find(it[-1]) != std::string::npos) {
      // Returns the character position right after the |field_contents| within
      // |suggestion| text as a caret position for text selection.
      return it - suggestion.begin() + field_contents.size();
    }
  }

  // Unable to find the |field_contents| in |suggestion| text.
  return std::u16string::npos;
}

bool IsCheckable(const FormFieldData::CheckStatus& check_status) {
  return check_status != FormFieldData::CheckStatus::kNotCheckable;
}

bool IsChecked(const FormFieldData::CheckStatus& check_status) {
  return check_status == FormFieldData::CheckStatus::kChecked;
}

void SetCheckStatus(FormFieldData* form_field_data,
                    bool isCheckable,
                    bool isChecked) {
  if (isChecked) {
    form_field_data->set_check_status(FormFieldData::CheckStatus::kChecked);
  } else {
    if (isCheckable) {
      form_field_data->set_check_status(
          FormFieldData::CheckStatus::kCheckableButUnchecked);
    } else {
      form_field_data->set_check_status(
          FormFieldData::CheckStatus::kNotCheckable);
    }
  }
}

std::vector<std::string> LowercaseAndTokenizeAttributeString(
    std::string_view attribute) {
  return base::SplitString(base::ToLowerASCII(attribute),
                           base::kWhitespaceASCII, base::TRIM_WHITESPACE,
                           base::SPLIT_WANT_NONEMPTY);
}

std::u16string RemoveWhitespace(const std::u16string& value) {
  std::u16string stripped_value;
  base::RemoveChars(value, base::kWhitespaceUTF16, &stripped_value);
  return stripped_value;
}

bool SanitizedFieldIsEmpty(const std::u16string& value) {
  // Some sites enter values such as ____-____-____-____ or (___)-___-____ in
  // their fields. Check if the field value is empty after the removal of the
  // formatting characters.
  static const base::NoDestructor<std::u16string> formatting(
      base::StrCat({u"-_()/",
                    {&base::i18n::kRightToLeftMark, 1},
                    {&base::i18n::kLeftToRightMark, 1},
                    base::kWhitespaceUTF16}));

  return base::ContainsOnlyChars(value, *formatting);
}

bool IsFillable(FocusedFieldType focused_field_type) {
  switch (focused_field_type) {
    case FocusedFieldType::kFillableTextArea:
    case FocusedFieldType::kFillableSearchField:
    case FocusedFieldType::kFillableNonSearchField:
    case FocusedFieldType::kFillableUsernameField:
    case FocusedFieldType::kFillablePasswordField:
    case FocusedFieldType::kFillableWebauthnTaggedField:
      return true;
    case FocusedFieldType::kUnfillableElement:
    case FocusedFieldType::kUnknown:
      return false;
  }
  NOTREACHED_NORETURN();
}

SubmissionIndicatorEvent ToSubmissionIndicatorEvent(SubmissionSource source) {
  switch (source) {
    case SubmissionSource::NONE:
      return SubmissionIndicatorEvent::NONE;
    case SubmissionSource::SAME_DOCUMENT_NAVIGATION:
      return SubmissionIndicatorEvent::SAME_DOCUMENT_NAVIGATION;
    case SubmissionSource::XHR_SUCCEEDED:
      return SubmissionIndicatorEvent::XHR_SUCCEEDED;
    case SubmissionSource::FRAME_DETACHED:
      return SubmissionIndicatorEvent::FRAME_DETACHED;
    case SubmissionSource::PROBABLY_FORM_SUBMITTED:
      return SubmissionIndicatorEvent::PROBABLE_FORM_SUBMISSION;
    case SubmissionSource::FORM_SUBMISSION:
      return SubmissionIndicatorEvent::HTML_FORM_SUBMISSION;
    case SubmissionSource::DOM_MUTATION_AFTER_AUTOFILL:
      return SubmissionIndicatorEvent::DOM_MUTATION_AFTER_AUTOFILL;
  }

  NOTREACHED_IN_MIGRATION();
  return SubmissionIndicatorEvent::NONE;
}

GURL StripAuthAndParams(const GURL& gurl) {
  GURL::Replacements rep;
  rep.ClearUsername();
  rep.ClearPassword();
  rep.ClearQuery();
  rep.ClearRef();
  return gurl.ReplaceComponents(rep);
}

bool IsAutofillManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return IsAddressAutofillManuallyTriggered(trigger_source) ||
         IsPaymentsAutofillManuallyTriggered(trigger_source) ||
         IsPasswordsAutofillManuallyTriggered(trigger_source);
}

bool IsAddressAutofillManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackAddress;
}

bool IsPaymentsAutofillManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackPayments;
}

bool IsPasswordsAutofillManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackPasswords;
}

bool IsPlusAddressesManuallyTriggered(
    AutofillSuggestionTriggerSource trigger_source) {
  return trigger_source ==
         AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses;
}

}  // namespace autofill
