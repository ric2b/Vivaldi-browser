// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_field.h"

#include <stdint.h>
#include <iterator>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/html_field_types.h"
#include "components/autofill/core/common/signatures.h"

namespace autofill {

using FieldPrediction =
    AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction;

template <>
struct DenseSetTraits<FieldPrediction::Source> {
  static constexpr FieldPrediction::Source kMinValue =
      FieldPrediction::Source(0);
  static constexpr FieldPrediction::Source kMaxValue =
      FieldPrediction::Source_MAX;
  static constexpr bool kPacked = false;
};

namespace {

// This list includes pairs (heuristic_type, html_type) that express which
// heuristic predictions should be prioritized over HTML. The list is used for
// new field types that do not have a clear corresponding HTML type. In these
// cases, the local heuristics predictions will be used to determine the field
// overall type.
static constexpr auto kAutofillHeuristicsVsHtmlOverrides =
    base::MakeFixedFlatSet<std::pair<FieldType, HtmlFieldType>>(
        {{ADDRESS_HOME_ADMIN_LEVEL2, HtmlFieldType::kAddressLevel1},
         {ADDRESS_HOME_ADMIN_LEVEL2, HtmlFieldType::kAddressLevel2},
         {ADDRESS_HOME_APT_NUM, HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_APT_NUM, HtmlFieldType::kAddressLine3},
         {ADDRESS_HOME_BETWEEN_STREETS, HtmlFieldType::kAddressLevel2},
         {ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
          HtmlFieldType::kAddressLevel2},
         {ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
          HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK,
          HtmlFieldType::kOrganization},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLevel1},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLevel2},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLevel3},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLine1},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, HtmlFieldType::kAddressLine3},
         {ADDRESS_HOME_OVERFLOW_AND_LANDMARK, HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_OVERFLOW, HtmlFieldType::kAddressLine2},
         {ADDRESS_HOME_OVERFLOW, HtmlFieldType::kAddressLine3},
         {ADDRESS_HOME_HOUSE_NUMBER, HtmlFieldType::kStreetAddress},
         {ADDRESS_HOME_STREET_NAME, HtmlFieldType::kStreetAddress}});

// This list includes pairs (heuristic_type, server_type) that express which
// heuristics predictions should be prioritized over server predictions. The
// list is used for new field types that the server may have learned
// incorrectly. In these cases, the local heuristics predictions will be used to
// determine the field type.
static constexpr auto kAutofillHeuristicsVsServerOverrides =
    base::MakeFixedFlatSet<std::pair<FieldType, FieldType>>(
        {{ADDRESS_HOME_ADMIN_LEVEL2, ADDRESS_HOME_CITY},
         {ADDRESS_HOME_HOUSE_NUMBER_AND_APT, ADDRESS_HOME_HOUSE_NUMBER},
         {ADDRESS_HOME_HOUSE_NUMBER_AND_APT, ADDRESS_HOME_APT_NUM},
         {ADDRESS_HOME_APT_NUM, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_APT_NUM, ADDRESS_HOME_LINE3},
         {ADDRESS_HOME_APT_NUM, ADDRESS_HOME_HOUSE_NUMBER},
         {ADDRESS_HOME_BETWEEN_STREETS, ADDRESS_HOME_LINE1},
         {ADDRESS_HOME_BETWEEN_STREETS, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_BETWEEN_STREETS, ADDRESS_HOME_STREET_ADDRESS},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_CITY},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_STATE},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_LINE1},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_DEPENDENT_LOCALITY, ADDRESS_HOME_LINE3},
         {ADDRESS_HOME_LANDMARK, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_BETWEEN_STREETS_OR_LANDMARK, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_OVERFLOW_AND_LANDMARK, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_OVERFLOW, ADDRESS_HOME_LINE2},
         {ADDRESS_HOME_OVERFLOW, ADDRESS_HOME_LINE3}});

// Returns true, if the prediction is non-experimental and should be used by
// autofill or password manager.
// Note: A `NO_SERVER_DATA` prediction with `SOURCE_UNSPECIFIED` may also be a
// default prediction. We don't need to store it, because its meaning is that
// there is no default prediction.
bool IsDefaultPrediction(const FieldPrediction& prediction) {
  constexpr DenseSet<FieldPrediction::Source> default_sources = {
      FieldPrediction::SOURCE_AUTOFILL_DEFAULT,
      FieldPrediction::SOURCE_PASSWORDS_DEFAULT,
      FieldPrediction::SOURCE_OVERRIDE,
      FieldPrediction::SOURCE_MANUAL_OVERRIDE};
  return default_sources.contains(prediction.source());
}

// Returns true if for two consecutive events, the second event may be ignored.
// In that case, if `event1` is at the back of AutofillField::field_log_events_,
// `event2` is not supposed to be added.
bool AreCollapsibleLogEvents(const AutofillField::FieldLogEventType& event1,
                             const AutofillField::FieldLogEventType& event2) {
  return absl::visit(
      [](const auto& e1, const auto& e2) {
        if constexpr (std::is_same_v<decltype(e1), decltype(e2)>) {
          return AreCollapsible(e1, e2);
        }
        return false;
      },
      event1, event2);
}

// Returns whether the `heuristic_type` should be preferred over the
// `html_type`. For certain field types that have been recently introduced, we
// want to prioritize local heuristics over the autocomplete type.
bool PreferHeuristicOverHtml(FieldType heuristic_type,
                             HtmlFieldType html_type) {
  return base::FeatureList::IsEnabled(
             features::kAutofillLocalHeuristicsOverrides) &&
         base::Contains(kAutofillHeuristicsVsHtmlOverrides,
                        std::make_pair(heuristic_type, html_type));
}

// Returns whether the `heuristic_type` should be preferred over the
// `server_type`. For certain field types that have been recently introduced, we
// want to prioritize the local heuristics predictions because they are more
// likely to be accurate. By prioritizing the local heuristics predictions, we
// can help the server to "learn" the correct classification for these fields.
bool PreferHeuristicOverServer(FieldType heuristic_type,
                               FieldType server_type) {
  return base::FeatureList::IsEnabled(
             features::kAutofillLocalHeuristicsOverrides) &&
         base::Contains(kAutofillHeuristicsVsServerOverrides,
                        std::make_pair(heuristic_type, server_type));
}

// Util function for `ComputedType`. Returns the values of HtmlFieldType that
// won't be overridden by heuristics or server predictions, up to a few
// exceptions. Check function `ComputedType` for more details.
DenseSet<HtmlFieldType> BelievedHtmlTypes(FieldType heuristic_prediction,
                                          FieldType server_prediction) {
  DenseSet<HtmlFieldType> believed_html_types = {};
  constexpr auto kMin = base::to_underlying(HtmlFieldType::kMinValue);
  constexpr auto kMax = base::to_underlying(HtmlFieldType::kMaxValue);
  for (auto i = kMin; i <= kMax; ++i) {
    believed_html_types.insert(static_cast<HtmlFieldType>(i));
  }
  // We always override unspecified autocomplete attribute.
  believed_html_types.erase(HtmlFieldType::kUnspecified);
  auto is_street_name_or_house_number_type = [](FieldType field_type) {
    return field_type == ADDRESS_HOME_STREET_NAME ||
           field_type == ADDRESS_HOME_HOUSE_NUMBER;
  };
  // When the heuristics or server predict that an address is a street name or a
  // house number, we prioritize this over "address-line[1|2]" autocomplete
  // since those signals are usually stronger for this combination.
  if (is_street_name_or_house_number_type(heuristic_prediction) ||
      is_street_name_or_house_number_type(server_prediction)) {
    believed_html_types.erase_all(
        {HtmlFieldType::kAddressLine1, HtmlFieldType::kAddressLine2});
  }
  // Always override unrecognized autocomplete attributes.
  believed_html_types.erase(HtmlFieldType::kUnrecognized);
  return believed_html_types;
}

}  // namespace

AutofillField::AutofillField() {
  local_type_predictions_.fill(NO_SERVER_DATA);
}

AutofillField::AutofillField(FieldSignature field_signature) : AutofillField() {
  field_signature_ = field_signature;
}

AutofillField::AutofillField(const FormFieldData& field)
    : FormFieldData(field),
      field_signature_(
          CalculateFieldSignatureByNameAndType(name(), form_control_type())),
      parseable_name_(name()),
      parseable_label_(label()) {
  local_type_predictions_.fill(NO_SERVER_DATA);
}

AutofillField::AutofillField(AutofillField&&) = default;

AutofillField& AutofillField::operator=(AutofillField&&) = default;

AutofillField::~AutofillField() = default;

std::unique_ptr<AutofillField> AutofillField::CreateForPasswordManagerUpload(
    FieldSignature field_signature) {
  std::unique_ptr<AutofillField> field;
  field.reset(new AutofillField(field_signature));
  return field;
}

FieldType AutofillField::heuristic_type() const {
  return heuristic_type(GetActiveHeuristicSource());
}

FieldType AutofillField::heuristic_type(HeuristicSource s) const {
  FieldType type = local_type_predictions_[static_cast<size_t>(s)];
  // `NO_SERVER_DATA` would mean that there is no heuristic type. Client code
  // presumes there is a prediction, therefore we coalesce to `UNKNOWN_TYPE`.
  // Shadow predictions however are not used and we care whether the type is
  // `UNKNOWN_TYPE` or whether we never ran the heuristics.
  return (type > 0 || s != GetActiveHeuristicSource()) ? type : UNKNOWN_TYPE;
}

FieldType AutofillField::server_type() const {
  return server_predictions_.empty()
             ? NO_SERVER_DATA
             : ToSafeFieldType(server_predictions_[0].type(), NO_SERVER_DATA);
}

bool AutofillField::server_type_prediction_is_override() const {
  return server_predictions_.empty() ? false
                                     : server_predictions_[0].override();
}

void AutofillField::set_heuristic_type(HeuristicSource s, FieldType type) {
  if (type < 0 || type > MAX_VALID_FIELD_TYPE ||
      type == FIELD_WITH_DEFAULT_VALUE) {
    NOTREACHED_IN_MIGRATION();
    // This case should not be reachable; but since this has potential
    // implications on data uploaded to the server, better safe than sorry.
    type = UNKNOWN_TYPE;
  }
  local_type_predictions_[static_cast<size_t>(s)] = type;
  if (s == GetActiveHeuristicSource()) {
    overall_type_ = AutofillType(NO_SERVER_DATA);
  }
}

void AutofillField::set_server_predictions(
    std::vector<FieldPrediction> predictions) {
  overall_type_ = AutofillType(NO_SERVER_DATA);
  // Ensures that AutofillField::server_type() is a valid enum value.
  for (auto& prediction : predictions) {
    prediction.set_type(ToSafeFieldType(prediction.type(), NO_SERVER_DATA));
  }

  server_predictions_.clear();
  experimental_server_predictions_.clear();

  for (auto& prediction : predictions) {
    if (prediction.has_source()) {
      if (prediction.source() == FieldPrediction::SOURCE_UNSPECIFIED)
        // A prediction with `SOURCE_UNSPECIFIED` is one of two things:
        //   1. No prediction for default, a.k.a. `NO_SERVER_DATA`. The absence
        //      of a prediction may not be creditable to a particular prediction
        //      source.
        //   2. An experiment that is missing from the `PredictionSource` enum.
        //      Protobuf corrects unknown values to 0 when parsing.
        // Neither case is actionable.
        continue;
      if (IsDefaultPrediction(prediction)) {
        server_predictions_.push_back(std::move(prediction));
      } else {
        experimental_server_predictions_.push_back(std::move(prediction));
      }
    } else {
      // TODO(crbug.com/40243028): captured tests store old autofill api
      // response recordings without `source` field. We need to maintain the old
      // behavior until these recordings will be migrated.
      server_predictions_.push_back(std::move(prediction));
    }
  }

  if (server_predictions_.empty())
    // Equivalent to a `NO_SERVER_DATA` prediction from `SOURCE_UNSPECIFIED`.
    server_predictions_.emplace_back();

  LOG_IF(ERROR, server_predictions_.size() > 2)
      << "Expected up to 2 default predictions from the Autofill server. "
         "Actual: "
      << server_predictions_.size();
}

void AutofillField::SetHtmlType(HtmlFieldType type, HtmlFieldMode mode) {
  html_type_ = type;
  html_mode_ = mode;
  overall_type_ = AutofillType(NO_SERVER_DATA);
}

void AutofillField::SetTypeTo(const AutofillType& type) {
  DCHECK(type.GetStorableType() != NO_SERVER_DATA);
  overall_type_ = type;
}

AutofillType AutofillField::ComputedType() const {
  // If autocomplete=tel/tel-* and server confirms it really is a phone field,
  // we always use the server prediction as html types are not very reliable.
  if (GroupTypeOfHtmlFieldType(html_type_) == FieldTypeGroup::kPhone &&
      GroupTypeOfFieldType(server_type()) == FieldTypeGroup::kPhone) {
    return AutofillType(server_type());
  }

  // TODO(crbug.com/40266396) Delete this if-statement when
  // features::kAutofillEnableExpirationDateImprovements has launched. This
  // should be covered by
  // FormStructureRationalizer::RationalizeAutocompleteAttributes.
  //
  // If the explicit type is cc-exp and either the server or heuristics agree on
  // a 2 vs 4 digit specialization of cc-exp, use that specialization.
  if (html_type_ == HtmlFieldType::kCreditCardExp &&
      !base::FeatureList::IsEnabled(
          features::kAutofillEnableExpirationDateImprovements)) {
    if (server_type() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
        server_type() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {
      return AutofillType(server_type());
    }
    if (heuristic_type() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
        heuristic_type() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR) {
      return AutofillType(heuristic_type());
    }
  }

  // In general, the autocomplete attribute has precedence over the other types
  // of field detection. Except for specific cases in PreferHeuristicOverHtml
  // and also those detailed in `BelievedHtmlTypes()`.
  if (PreferHeuristicOverHtml(heuristic_type(), html_type())) {
    return AutofillType(heuristic_type());
  }

  if (BelievedHtmlTypes(heuristic_type(), server_type())
          .contains(html_type())) {
    return AutofillType(html_type_);
  }

  if (server_type() != NO_SERVER_DATA &&
      !PreferHeuristicOverServer(heuristic_type(), server_type())) {
    // Sometimes the server and heuristics disagree on whether a name field
    // should be associated with an address or a credit card. There was a
    // decision to prefer the heuristics in these cases, but it looks like
    // it might be better to fix this server-side.
    // See http://crbug.com/429236 for background.
    bool believe_server = !(server_type() == NAME_FULL &&
                            heuristic_type() == CREDIT_CARD_NAME_FULL) &&
                          !(server_type() == CREDIT_CARD_NAME_FULL &&
                            heuristic_type() == NAME_FULL) &&
                          !(server_type() == NAME_FIRST &&
                            heuristic_type() == CREDIT_CARD_NAME_FIRST) &&
                          !(server_type() == NAME_LAST &&
                            heuristic_type() == CREDIT_CARD_NAME_LAST);

    // Either way, retain a preference for the CVC heuristic over the
    // server's password predictions (http://crbug.com/469007)
    believe_server =
        believe_server && !(GroupTypeOfFieldType(server_type()) ==
                                FieldTypeGroup::kPasswordField &&
                            heuristic_type() == CREDIT_CARD_VERIFICATION_CODE);

    // For structured last name tokens the heuristic predictions get precedence
    // over the server predictions.
    believe_server = believe_server && heuristic_type() != NAME_LAST_SECOND &&
                     heuristic_type() != NAME_LAST_FIRST;

    // For structured address tokens the heuristic predictions get precedence
    // over the server predictions.
    believe_server = believe_server &&
                     heuristic_type() != ADDRESS_HOME_STREET_NAME &&
                     heuristic_type() != ADDRESS_HOME_HOUSE_NUMBER;

    // For merchant promo code fields the heuristic predictions get precedence
    // over the server predictions.
    believe_server =
        believe_server && (heuristic_type() != MERCHANT_PROMO_CODE);

    // For international bank account number (IBAN) fields the heuristic
    // predictions get precedence over the server predictions.
    believe_server = believe_server && (heuristic_type() != IBAN_VALUE);

    // The numeric quantity heuristic should get granted precedence over the
    // server prediction since it tries to catch false-positive server
    // predictions.
    believe_server =
        believe_server &&
        !(heuristic_type() == NUMERIC_QUANTITY &&
          server_type() != UNKNOWN_TYPE &&
          base::FeatureList::IsEnabled(
              features::kAutofillGivePrecedenceToNumericQuantities));

    if (believe_server)
      return AutofillType(server_type());
  }

  return AutofillType(heuristic_type());
}

AutofillType AutofillField::Type() const {
  // Server Overrides are granted precedence unconditionally.
  if (server_type_prediction_is_override() && server_type() != NO_SERVER_DATA)
    return AutofillType(server_type());

  if (overall_type_.GetStorableType() != NO_SERVER_DATA)
    return overall_type_;
  return ComputedType();
}

bool AutofillField::IsEmpty() const {
  return value().empty();
}

FieldSignature AutofillField::GetFieldSignature() const {
  return field_signature_ ? *field_signature_
                          : CalculateFieldSignatureByNameAndType(
                                name(), form_control_type());
}

std::string AutofillField::FieldSignatureAsStr() const {
  return base::NumberToString(GetFieldSignature().value());
}

bool AutofillField::IsFieldFillable() const {
  FieldType field_type = Type().GetStorableType();
  return IsFillableFieldType(field_type);
}

bool AutofillField::HasExpirationDateType() const {
  static constexpr std::array kExpirationDateTypes = {
      CREDIT_CARD_EXP_MONTH, CREDIT_CARD_EXP_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_4_DIGIT_YEAR, CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR,
      CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR};
  return base::Contains(kExpirationDateTypes, Type().GetStorableType());
}

bool AutofillField::ShouldSuppressSuggestionsAndFillingByDefault() const {
  return html_type_ == HtmlFieldType::kUnrecognized &&
         !server_type_prediction_is_override() && !IsCreditCardPrediction();
}

void AutofillField::SetPasswordRequirements(PasswordRequirementsSpec spec) {
  password_requirements_ = std::move(spec);
}

bool AutofillField::IsCreditCardPrediction() const {
  return GroupTypeOfFieldType(server_type()) == FieldTypeGroup::kCreditCard ||
         GroupTypeOfFieldType(heuristic_type()) == FieldTypeGroup::kCreditCard;
}

void AutofillField::AppendLogEventIfNotRepeated(
    const FieldLogEventType& log_event) {
  // TODO(crbug.com/40225658): Consider to use an Overflow event to stop
  // recording log events into |field_log_events_| to save memory when
  // |field_log_events_| reaches certain threshold, e.g. 1000.

  if (field_log_events_.empty() ||
      field_log_events_.back().index() != log_event.index() ||
      !AreCollapsibleLogEvents(field_log_events_.back(), log_event)) {
    field_log_events_.push_back(log_event);
  }
}

bool AutofillField::WasAutofilledWithFallback() const {
  return autofilled_type_ &&
         autofilled_type_ != overall_type_.GetStorableType();
}

}  // namespace autofill
