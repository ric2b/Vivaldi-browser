// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/suggestion.h"

#include <type_traits>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"

namespace autofill {

Suggestion::Text::Text() = default;

Suggestion::Text::Text(std::u16string value,
                       IsPrimary is_primary,
                       ShouldTruncate should_truncate)
    : value(value), is_primary(is_primary), should_truncate(should_truncate) {}

Suggestion::Text::Text(const Text& other) = default;
Suggestion::Text::Text(Text& other) = default;

Suggestion::Text& Suggestion::Text::operator=(const Text& other) = default;
Suggestion::Text& Suggestion::Text::operator=(Text&& other) = default;

Suggestion::Text::~Text() = default;

Suggestion::Suggestion() = default;

Suggestion::Suggestion(std::u16string main_text)
    : main_text(std::move(main_text), Text::IsPrimary(true)) {}

Suggestion::Suggestion(SuggestionType type) : type(type) {}

Suggestion::Suggestion(std::u16string main_text, SuggestionType type)
    : type(type), main_text(std::move(main_text), Text::IsPrimary(true)) {}

Suggestion::Suggestion(std::string_view main_text,
                       std::string_view label,
                       Icon icon,
                       SuggestionType type)
    : type(type),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      icon(icon) {
  if (!label.empty())
    this->labels = {{Text(base::UTF8ToUTF16(label))}};
}

Suggestion::Suggestion(std::string_view main_text,
                       std::vector<std::vector<Text>> labels,
                       Icon icon,
                       SuggestionType type)
    : type(type),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      labels(std::move(labels)),
      icon(icon) {}

Suggestion::Suggestion(std::string_view main_text,
                       std::string_view minor_text,
                       std::string_view label,
                       Icon icon,
                       SuggestionType type)
    : type(type),
      main_text(base::UTF8ToUTF16(main_text), Text::IsPrimary(true)),
      minor_text(base::UTF8ToUTF16(minor_text)),
      icon(icon) {
  if (!label.empty())
    this->labels = {{Text(base::UTF8ToUTF16(label))}};
}

Suggestion::Suggestion(const Suggestion& other) = default;
Suggestion::Suggestion(Suggestion&& other) = default;

Suggestion& Suggestion::operator=(const Suggestion& other) = default;
Suggestion& Suggestion::operator=(Suggestion&& other) = default;

Suggestion::~Suggestion() = default;

std::string_view ConvertIconToPrintableString(Suggestion::Icon icon) {
  switch (icon) {
    case Suggestion::Icon::kAccount:
      return "kAccount";
    case Suggestion::Icon::kClear:
      return "kClear";
    case Suggestion::Icon::kCreate:
      return "kCreate";
    case Suggestion::Icon::kCode:
      return "kCode";
    case Suggestion::Icon::kDelete:
      return "kDelete";
    case Suggestion::Icon::kDevice:
      return "kDevice";
    case Suggestion::Icon::kEdit:
      return "kEdit";
    case Suggestion::Icon::kEmail:
      return "kEmail";
    case Suggestion::Icon::kEmpty:
      return "kEmpty";
    case Suggestion::Icon::kGlobe:
      return "kGlobe";
    case Suggestion::Icon::kGoogle:
      return "kGoogle";
    case Suggestion::Icon::kGoogleMonochrome:
      return "kGoogleMonochrome";
    case Suggestion::Icon::kGooglePasswordManager:
      return "kGooglePasswordManager";
    case Suggestion::Icon::kGooglePay:
      return "kGooglePay";
    case Suggestion::Icon::kGooglePayDark:
      return "kGooglePayDark";
    case Suggestion::Icon::kHttpWarning:
      return "kHttpWarning";
    case Suggestion::Icon::kHttpsInvalid:
      return "kHttpsInvalid";
    case Suggestion::Icon::kKey:
      return "kKey";
    case Suggestion::Icon::kLocation:
      return "kLocation";
    case Suggestion::Icon::kMagic:
      return "kMagic";
    case Suggestion::Icon::kOfferTag:
      return "kOfferTag";
    case Suggestion::Icon::kPenSpark:
      return "kPenSpark";
    case Suggestion::Icon::kScanCreditCard:
      return "kScanCreditCard";
    case Suggestion::Icon::kSettings:
      return "kSettings";
    case Suggestion::Icon::kSettingsAndroid:
      return "kSettingsAndroid";
    case Suggestion::Icon::kUndo:
      return "kUndo";
    case Suggestion::Icon::kCardGeneric:
      return "kCardGeneric";
    case Suggestion::Icon::kCardAmericanExpress:
      return "kCardAmericanExpress";
    case Suggestion::Icon::kCardDiners:
      return "kCardDiners";
    case Suggestion::Icon::kCardDiscover:
      return "kCardDiscover";
    case Suggestion::Icon::kCardElo:
      return "kCardElo";
    case Suggestion::Icon::kCardJCB:
      return "kCardJCB";
    case Suggestion::Icon::kCardMasterCard:
      return "kCardMasterCard";
    case Suggestion::Icon::kCardMir:
      return "kCardMir";
    case Suggestion::Icon::kCardTroy:
      return "kCardTroy";
    case Suggestion::Icon::kCardUnionPay:
      return "kCardUnionPay";
    case Suggestion::Icon::kCardVerve:
      return "kCardVerve";
    case Suggestion::Icon::kCardVisa:
      return "kCardVisa";
    case Suggestion::Icon::kIban:
      return "kIban";
    case Suggestion::Icon::kPlusAddress:
      return "kPlusAddress";
    case Suggestion::Icon::kNoIcon:
      return "kNoIcon";
  }
  NOTREACHED_NORETURN();
}

void PrintTo(const Suggestion& suggestion, std::ostream* os) {
  *os << std::endl
      << "Suggestion (type:" << suggestion.type << ", main_text:\""
      << suggestion.main_text.value << "\""
      << (suggestion.main_text.is_primary ? "(Primary)" : "(Not Primary)")
      << ", minor_text:\"" << suggestion.minor_text.value << "\""
      << (suggestion.minor_text.is_primary ? "(Primary)" : "(Not Primary)")
      << ", additional_label: \"" << suggestion.additional_label << "\""
      << ", icon:" << ConvertIconToPrintableString(suggestion.icon)
      << ", trailing_icon:"
      << ConvertIconToPrintableString(suggestion.trailing_icon) << ")";
}

}  // namespace autofill
