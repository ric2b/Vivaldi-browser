// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_ablation_study.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/autofill_trigger_details.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling_product.h"
#include "components/autofill/core/browser/form_autofill_history.h"
#include "components/autofill/core/browser/form_filler.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/fallback_autocomplete_unrecognized_metrics.h"
#include "components/autofill/core/browser/metrics/form_events/address_form_event_logger.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/manual_fallback_metrics.h"
#include "components/autofill/core/browser/payments/autofill_offer_manager.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/single_field_form_fill_router.h"
#include "components/autofill/core/browser/suggestions_context.h"
#include "components/autofill/core/browser/ui/fast_checkout_delegate.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill/core/common/unique_ids.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace autofill {

class AutofillField;
class AutofillClient;
class AutofillProfile;
class CreditCard;
class CreditCardAccessManager;

class FormData;
class FormFieldData;
struct SuggestionsContext;

namespace autofill_metrics {

class CreditCardFormEventLogger;

}  // namespace autofill_metrics

// Enum for the value patterns metric. Don't renumerate existing value. They are
// used for metrics.
enum class ValuePatternsMetric {
  kNoPatternFound = 0,
  kUpiVpa = 1,  // UPI virtual payment address.
  kIban = 2,    // International Bank Account Number.
  kMaxValue = kIban,
};

// Manages saving and restoring the user's personal information entered into web
// forms. One per frame; owned by the AutofillDriver.
class BrowserAutofillManager : public AutofillManager {
 public:
  // Triggered when `GenerateSuggestionsAndMaybeShowUI` is complete.
  // `show_suggestions` indicates whether or not the list of `suggestions`
  // should be displayed (via the `external_delegate_`).
  using OnGenerateSuggestionsCallback =
      base::OnceCallback<void(bool show_suggestions,
                              std::vector<Suggestion> suggestions)>;

  BrowserAutofillManager(AutofillDriver* driver,
                         const std::string& app_locale);

  BrowserAutofillManager(const BrowserAutofillManager&) = delete;
  BrowserAutofillManager& operator=(const BrowserAutofillManager&) = delete;

  ~BrowserAutofillManager() override;

  // Whether the |field| should show an entry to scan a credit card.
  virtual bool ShouldShowScanCreditCard(const FormData& form,
                                        const FormFieldData& field);

  // Handlers for the "Show Cards From Account" row. This row should be shown to
  // users who have cards in their account and can use Sync Transport. Clicking
  // the row records the user's consent to see these cards on this device, and
  // refreshes the popup.
  virtual bool ShouldShowCardsFromAccountOption(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source) const;
  virtual void OnUserAcceptedCardsFromAccountOption();
  virtual void RefetchCardsAndUpdatePopup(const FormData& form,
                                          const FormFieldData& field_data);

  virtual void FillOrPreviewCreditCardForm(
      mojom::ActionPersistence action_persistence,
      const FormData& form,
      const FormFieldData& field,
      const CreditCard& credit_card,
      const std::u16string& cvc,
      const AutofillTriggerDetails& trigger_details);

  // Routes calls from external components to FormFiller::FillOrPreviewField.
  // Virtual for testing.
  // TODO(crbug.com/40227496): Replace FormFieldData parameter by FieldGlobalId.
  virtual void FillOrPreviewField(mojom::ActionPersistence action_persistence,
                                  mojom::FieldActionType action_type,
                                  const FormData& form,
                                  const FormFieldData& field,
                                  const std::u16string& value,
                                  SuggestionType type,
                                  std::optional<FieldType> field_type_used);

  // Logs metrics when the user accepts address form filling suggestion. This
  // happens only for already parsed forms (`FormStructure` and `AutofillField`
  // are defined).
  // TODO(crbug.com/40227071): Remove when field-filling and form-filling are
  // merged
  virtual void OnDidFillAddressFormFillingSuggestion(
      const AutofillProfile& profile,
      const FormData& form,
      const FormFieldData& field,
      AutofillTriggerSource trigger_source);

  // Calls UndoAutofillImpl and logs metrics. Virtual for testing.
  virtual void UndoAutofill(mojom::ActionPersistence action_persistence,
                            const FormData& form,
                            const FormFieldData& trigger_field);
  // Virtual for testing
  virtual void DidShowSuggestions(
      base::span<const SuggestionType> shown_suggestions_types,
      const FormData& form,
      const FormFieldData& field);

  // Fills or previews the profile form.
  // Assumes the form and field are valid.
  // TODO(crbug.com/40227071): Clean up the API.
  virtual void FillOrPreviewProfileForm(
      mojom::ActionPersistence action_persistence,
      const FormData& form,
      const FormFieldData& field,
      const AutofillProfile& profile,
      const AutofillTriggerDetails& trigger_details);

  // Asks for authentication via CVC before filling with server card data.
  // TODO(crbug.com/40227071): Clean up the API.
  virtual void AuthenticateThenFillCreditCardForm(
      const FormData& form,
      const FormFieldData& field,
      const CreditCard& credit_card,
      const AutofillTriggerDetails& trigger_details);

  // Remove the credit card or Autofill profile that matches |backend_id|
  // from the database. Returns true if deletion is allowed.
  bool RemoveAutofillProfileOrCreditCard(Suggestion::BackendId backend_id);

  // Remove the specified suggestion from single field filling.
  // `type` is the SuggestionType of the suggestion.
  void RemoveCurrentSingleFieldSuggestion(const std::u16string& name,
                                          const std::u16string& value,
                                          SuggestionType type);

  // Invoked when the user selected |value| in a suggestions list from single
  // field filling. `type` is the SuggestionType of the
  // suggestion.
  void OnSingleFieldSuggestionSelected(const std::u16string& value,
                                       SuggestionType type,
                                       const FormData& form,
                                       const FormFieldData& field);

  // Invoked when the user selects the "Hide Suggestions" item in the
  // Autocomplete drop-down.
  virtual void OnUserHideSuggestions(const FormData& form,
                                     const FormFieldData& field);

  const std::string& app_locale() const { return app_locale_; }

  // Will send an upload based on the |form_structure| data and the local
  // Autofill profile data. |observed_submission| is specified if the upload
  // follows an observed submission event. Returns false if the upload couldn't
  // start.
  virtual bool MaybeStartVoteUploadProcess(
      std::unique_ptr<FormStructure> form_structure,
      bool observed_submission);

  // Update the pending form with |form|, possibly processing the current
  // pending form for upload.
  void UpdatePendingForm(const FormData& form);

  // Upload the current pending form.
  void ProcessPendingFormForUpload();

  CreditCardAccessManager& GetCreditCardAccessManager();
  const CreditCardAccessManager& GetCreditCardAccessManager() const;

  // Handles post-filling logic of `form_structure`, like notifying observers
  // and logging form metrics.
  // `filled_fields` are the fields that were filled by the browser.
  // `safe_fields` are the fields that were deemed safe to fill by the router
  // according to the iframe security policy.
  // `safe_filled_fields` is the intersection of `filled_fields` and
  // `safe_fields`.
  void OnDidFillOrPreviewForm(
      mojom::ActionPersistence action_persistence,
      const FormStructure& form_structure,
      const AutofillField& trigger_autofill_field,
      base::span<const FormFieldData*> safe_filled_fields,
      base::span<const AutofillField*> safe_filled_autofill_fields,
      const base::flat_set<FieldGlobalId>& filled_fields,
      const base::flat_set<FieldGlobalId>& safe_fields,
      absl::variant<const AutofillProfile*, const CreditCard*>
          profile_or_credit_card,
      const AutofillTriggerDetails& trigger_details,
      bool is_refill);

  // AutofillManager:
  base::WeakPtr<AutofillManager> GetWeakPtr() override;
  bool ShouldClearPreviewedForm() override;
  void OnFocusOnNonFormFieldImpl(bool had_interacted_form) override;
  void OnFocusOnFormFieldImpl(const FormData& form,
                              const FieldGlobalId& field_id) override;
  void OnDidFillAutofillFormDataImpl(const FormData& form,
                                     const base::TimeTicks timestamp) override;
  void OnDidEndTextFieldEditingImpl() override;
  void OnHidePopupImpl() override;
  void OnSelectOrSelectListFieldOptionsDidChangeImpl(
      const FormData& form) override;
  void OnJavaScriptChangedAutofilledValueImpl(const FormData& form,
                                              const FieldGlobalId& field_id,
                                              const std::u16string& old_value,
                                              bool formatting_only) override;
  void Reset() override;

  // Retrieves the four digit combinations from the DOM of the current web page
  // and stores them in `four_digit_combinations_in_dom_`. This is used to check
  // for the virtual card last four when checking for standalone CVC field.
  void FetchPotentialCardLastFourDigitsCombinationFromDOM();

  // Returns true if either Profile or CreditCard Autofill is enabled.
  virtual bool IsAutofillEnabled() const;

  // Returns true if the value of the AutofillProfileEnabled pref is true and
  // the client supports Autofill.
  virtual bool IsAutofillProfileEnabled() const;

  // Returns true if the value of the AutofillCreditCardEnabled pref is true
  // and the client supports Autofill.
  virtual bool IsAutofillPaymentMethodsEnabled() const;

  // Shared code to determine if |form| should be uploaded to the Autofill
  // server. It verifies that uploading is allowed and |form| meets conditions
  // to be uploadable. Exposed for testing.
  bool ShouldUploadForm(const FormStructure& form);

  // Returns the last form the autofill manager considered in this frame.
  virtual const FormData& last_query_form() const;

  // Reports whether a document collects phone numbers, uses one time code, uses
  // WebOTP. There are cases that the reporting is not expected:
  //   1. some unit tests do not set necessary members,
  //   |browser_autofill_manager_|
  //   2. there is no form and WebOTP is not used
  void ReportAutofillWebOTPMetrics(bool used_web_otp) override;

  // Set Fast Checkout run ID on the corresponding form event logger.
  virtual void SetFastCheckoutRunId(FieldTypeGroup field_type_group,
                                    int64_t run_id);

  TouchToFillDelegate* touch_to_fill_delegate() {
    return touch_to_fill_delegate_.get();
  }

  void set_touch_to_fill_delegate(
      std::unique_ptr<TouchToFillDelegate> touch_to_fill_delegate) {
    touch_to_fill_delegate_ = std::move(touch_to_fill_delegate);
  }

  FastCheckoutDelegate* fast_checkout_delegate() {
    return fast_checkout_delegate_.get();
  }

  void set_fast_checkout_delegate(
      std::unique_ptr<FastCheckoutDelegate> fast_checkout_delegate) {
    fast_checkout_delegate_ = std::move(fast_checkout_delegate);
  }

  // Returns the field corresponding to |form| and |field| that can be
  // autofilled. Returns NULL if the field cannot be autofilled.
  [[nodiscard]] AutofillField* GetAutofillField(
      const FormData& form,
      const FormFieldData& field) const;

  // Notifies the `BrowserAutofillManager` that `credit_card` has been fetched
  // from the server. Opens a manual filling dialog for virtual credit cards.
  // Caches the credit card data for server and virtual credit cards.
  void OnCreditCardFetchedSuccessfully(const CreditCard& credit_card);

  autofill_metrics::AutocompleteUnrecognizedFallbackEventLogger&
  GetAutocompleteUnrecognizedFallbackEventLogger() {
    return *autocomplete_unrecognized_fallback_logger_;
  }

  autofill_metrics::ManualFallbackEventLogger& GetManualFallbackEventLogger() {
    return *manual_fallback_logger_;
  }

 protected:
  // Stores a `callback` for `form_signature`, possibly overriding an older
  // callback for `form_signature` or triggering a pending callback in case too
  // many callbacks are stored to create space.
  virtual void StoreUploadVotesAndLogQualityCallback(
      FormSignature form_signature,
      base::OnceClosure callback);

  // Triggers and wipes all pending QualityAndVotesUploadCallbacks.
  void FlushPendingLogQualityAndVotesUploadCallbacks();

  // Removes a callback for the given `form_signature` without calling it.
  void WipeLogQualityAndVotesUploadCallback(FormSignature form_signature);

  // Logs quality metrics for the |submitted_form| and uploads votes for the
  // field types to the crowdsourcing server, if appropriate.
  // |observed_submission| indicates whether the upload is a result of an
  // observed submission event.
  virtual void UploadVotesAndLogQuality(
      std::unique_ptr<FormStructure> submitted_form,
      base::TimeTicks interaction_time,
      base::TimeTicks submission_time,
      bool observed_submission,
      ukm::SourceId source_id);

  // Returns the card image for `credit_card`. If the `credit_card` has a card
  // art image linked, prefer it. Otherwise fall back to the network icon.
  virtual const gfx::Image& GetCardImage(const CreditCard& credit_card);

  // AutofillManager:
  void OnFormSubmittedImpl(const FormData& form,
                           bool known_success,
                           mojom::SubmissionSource source) override;
  void OnCaretMovedInFormFieldImpl(const FormData& form,
                                   const FieldGlobalId& field_id,
                                   const gfx::Rect& caret_bounds) override {}
  void OnTextFieldDidChangeImpl(const FormData& form,
                                const FieldGlobalId& field_id,
                                const base::TimeTicks timestamp) override;
  void OnTextFieldDidScrollImpl(const FormData& form,
                                const FieldGlobalId& field_id) override {}
  void OnAskForValuesToFillImpl(
      const FormData& form,
      const FieldGlobalId& field_id,
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source) override;
  void OnSelectControlDidChangeImpl(const FormData& form,
                                    const FieldGlobalId& field_id) override;
  bool ShouldParseForms() override;
  void OnBeforeProcessParsedForms() override;
  void OnFormProcessed(const FormData& form,
                       const FormStructure& form_structure) override;

 private:
  friend class BrowserAutofillManagerTestApi;

  // When `AuthenticateThenFillCreditCardForm()` fetches a credit card, this
  // gets called once the fetching has finished. If successful, the
  // `credit_card` is filled.
  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* credit_card);

  // Updates event loggers with information about data stored for Autofill.
  void UpdateLoggersReadinessData();

  // Creates a FormStructure using the FormData received from the renderer. Will
  // return an empty scoped_ptr if the data should not be processed for upload
  // or personal data.
  std::unique_ptr<FormStructure> ValidateSubmittedForm(const FormData& form);

  // Method called after the values present on submitted fields were associated
  // with Autofill field types. It is used to route calls to
  // `UploadVotesAndLogQuality()` and
  // `AutofillClient::TriggerUserPerceptionOfAutofillSurvey()`, since both
  // depend on the field types being determined.
  void OnSubmissionFieldTypesDetermined(
      std::unique_ptr<FormStructure> submitted_form,
      base::TimeTicks interaction_time,
      base::TimeTicks submission_time,
      bool observed_submission,
      ukm::SourceId source_id);

  // Returns suggestions for the `form`, if suggestions were triggered using
  // the `trigger_source` on the `field`. The field's type is `field_type`.
  // The `trigger_source` controls which fields are considered for filling and
  // thus influences the suggestion labels.
  // `form_structure` and `autofill_field` can be null when the `field` from
  // which Autofill was triggered is not an address field. This means the user
  // chose the address manual fallback option to fill an arbitrary non address
  // field.
  std::vector<Suggestion> GetProfileSuggestions(
      const FormData& form,
      const FormStructure* form_structure,
      const FormFieldData& trigger_field,
      const AutofillField* trigger_autofill_field,
      AutofillSuggestionTriggerSource trigger_source) const;

  // Returns a list of values from the stored credit cards that match
  // `trigger_field_type` and the value of `trigger_field` and returns the
  // labels of the matching credit cards.
  std::vector<Suggestion> GetCreditCardSuggestions(
      const FormData& form,
      const FormFieldData& trigger_field,
      FieldType trigger_field_type,
      AutofillSuggestionTriggerSource trigger_source);

  // Returns a mapping of credit card guid values to virtual card last fours for
  // standalone CVC field. Cards will only be added to the returned map if they
  // have usage data on the webpage and the VCN last four was found on webpage
  // DOM.
  base::flat_map<std::string, VirtualCardUsageData::VirtualCardLastFour>
  GetVirtualCreditCardsForStandaloneCvcField(const url::Origin& origin) const;

  // If |initial_interaction_timestamp_| is unset or is set to a later time than
  // |interaction_timestamp|, updates the cached timestamp.  The latter check is
  // needed because IPC messages can arrive out of order.
  void UpdateInitialInteractionTimestamp(base::TimeTicks interaction_timestamp);

  // Examines |form| and returns true if it is in a non-secure context or
  // its action attribute targets a HTTP url.
  bool IsFormNonSecure(const FormData& form) const;

  // Checks whether JavaScript cleared an autofilled value within
  // kLimitBeforeRefill after the filling and records metrics for this. This
  // method should be called after we learned that JavaScript modified an
  // autofilled field. It's responsible for assessing the nature of the
  // modification. `cleared_value` is true if JS wiped the previous value, and
  // `formatting_only` is true if JS only modified whitespaces, symbols and
  // capitalization.
  // TODO(crbug.com/40227496): Remove `cleared_value` when `field` starts
  // containing the actual current value of the field.
  void AnalyzeJavaScriptChangedAutofilledValue(const FormStructure& form,
                                               AutofillField& field,
                                               bool cleared_value,
                                               bool formatting_only);

  // Populates all the fields (except for ablation stuy related fields) in
  // `SuggestionsContext` based on the given params.
  SuggestionsContext BuildSuggestionsContext(
      const FormData& form,
      const FormStructure* form_structure,
      const FormFieldData& field,
      const AutofillField* autofill_field,
      AutofillSuggestionTriggerSource trigger_source);

  // Returns a list with the suggestions available for `field`. Which fields of
  // the `form` are filled depends on the `trigger_source`. `context` could
  // contain additional information about the suggestions, such as ablation
  // study related fields.
  // TODO(crbug.com/340494671): Move ablation study fields out of the function
  // and make the context a const ref.
  std::vector<Suggestion> GetAvailableAddressAndCreditCardSuggestions(
      const FormData& form,
      const FormStructure* form_structure,
      const FormFieldData& field,
      const AutofillField* autofill_field,
      AutofillSuggestionTriggerSource trigger_source,
      SuggestionsContext& context);

  // Generates and prioritizes different kinds of suggestions and
  // suggestion surfaces accordingly (e.g. Fast Checkout,
  // SingleFieldFormFiller(s), address and credit card popups). Suggestion flows
  // that handle their own UI flow (e.g. FastCheckout, TTF,
  // SingleFieldFormFiller) are triggered from within this function. Other flows
  // that rely on the `external_delegate_` to show their suggestions, pass the
  // suggestions list to the delegate on `OnGenerateSuggestionsComplete` and
  // request them to be shown (via `show_suggestions`). Note that the `callback`
  // is always called regardless of the suggestion surface. The only case when
  // it's not called is when suggestions are suppressed (See
  // `ShouldSuppressSuggestions`).
  void GenerateSuggestionsAndMaybeShowUI(
      const FormData& form,
      const FormStructure* form_structure,
      const FormFieldData& field,
      const AutofillField* autofill_field,
      AutofillSuggestionTriggerSource trigger_source,
      SuggestionsContext& context,
      OnGenerateSuggestionsCallback callback);

  // Receives the lists of plus address and single field form fill suggestions
  // and combines them. It gives priority to the plus address suggestions,
  // ensuring they appear first in the final combined list that's sent to
  // `OnGenerateSuggestionsCallback`.
  void OnGeneratedPlusAddressAndSingleFieldFormFillSuggestions(
      AutofillPlusAddressDelegate::SuggestionContext suggestions_context,
      AutofillClient::PasswordFormType password_form_type,
      const FormData& form,
      const FormFieldData& field,
      OnGenerateSuggestionsCallback callback,
      std::vector<std::vector<Suggestion>> suggestion_lists);

  // Displays IPH for manual fallbacks if the form can be autofilled and the
  // user has profiles which can fill the current field.
  void MaybeShowIphForManualFallback(
      const FormFieldData& field,
      const AutofillField* autofill_field,
      AutofillSuggestionTriggerSource trigger_source,
      SuppressReason suppress_reason);

  // The function receives a the list of `suggestions` from
  // `GenerateSuggestionsAndMaybeShowUI` and displays them if `show_suggestions`
  // is true (via the `external_delegate_`). It also logs whether there is a
  // suggestion for the user and whether the suggestion is shown.
  void OnGenerateSuggestionsComplete(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source,
      const SuggestionsContext& context,
      bool show_suggestions,
      std::vector<Suggestion> suggestions);

  void OnGetPlusAddressSuggestions(
      AutofillPlusAddressDelegate::SuggestionContext suggestions_context,
      AutofillClient::PasswordFormType password_form_type,
      const FormData& form,
      const FormFieldData& field,
      std::vector<Suggestion> address_suggestions,
      OnGenerateSuggestionsCallback callback,
      std::vector<Suggestion> suggestions);

  // For each submitted field in the |form_structure|, it determines whether
  // |ADDRESS_HOME_STATE| is a possible matching type.
  // This method is intended to run matching type detection on the browser UI
  // thread.
  void PreProcessStateMatchingTypes(
      const std::vector<AutofillProfile>& profiles,
      FormStructure* form_structure);

  // Returns an appropriate EventFormLogger, depending on the given `field`'s
  // type. May return nullptr.
  autofill_metrics::FormEventLoggerBase* GetEventFormLogger(
      const AutofillField& field) const;

  // Iterate through all the fields in the form to process the log events for
  // each field and record into FieldInfo UKM event.
  void ProcessFieldLogEventsInForm(const FormStructure& form_structure);

  // Log the number of log events of all types which have been recorded until
  // the FieldInfo metric is recorded into UKM at form submission or form
  // destruction time (whatever comes first).
  void LogEventCountsUMAMetric(const FormStructure& form_structure);

  // Returns whether the form is considered parseable and meets a couple of
  // other requirements which makes uploading UKM data worthwhile. E.g. the
  // form should not be a search form, the forms should have at least one
  // focusable input field with a type from heuristics or the server.
  bool ShouldUploadUkm(const FormStructure& form_structure,
                       bool require_classified_field);

  // Returns a compose suggestion if the compose service is available for
  // `field` and `trigger_source`.
  std::optional<Suggestion> MaybeGetComposeSuggestion(
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source);

  // Delegates to perform external processing (display, selection) on
  // our behalf.
  std::unique_ptr<AutofillExternalDelegate> external_delegate_;
  std::unique_ptr<TouchToFillDelegate> touch_to_fill_delegate_;
  std::unique_ptr<FastCheckoutDelegate> fast_checkout_delegate_;

  std::string app_locale_;

  // Handles routing single-field form filling requests, such as for
  // Autocomplete and merchant promo codes.
  std::unique_ptr<SingleFieldFormFillRouter> single_field_form_fill_router_ =
      std::make_unique<SingleFieldFormFillRouter>(
          client().GetAutocompleteHistoryManager(),
          client().GetPaymentsAutofillClient()->GetIbanManager(),
          client().GetPaymentsAutofillClient()->GetMerchantPromoCodeManager());

  // Utilities for logging form events. The loggers emit metrics during their
  // destruction, effectively when the BrowserAutofillManager is reset or
  // destroyed.
  // The address and credit card event loggers are used to emit key and funnel
  // metrics.
  std::unique_ptr<autofill_metrics::AddressFormEventLogger>
      address_form_event_logger_;
  std::unique_ptr<autofill_metrics::CreditCardFormEventLogger>
      credit_card_form_event_logger_;
  // The autocomplete unrecognized fallback logger is used to collect metrics
  // around the manual fallback for autocomplete=unrecognized fields.
  // Since no metrics for autocomplete=unrecognized fields are emitted through
  // the `address_form_event_logger_`, a separate logger specifically for
  // autocomplete=unrecognized fields is used.
  std::unique_ptr<autofill_metrics::AutocompleteUnrecognizedFallbackEventLogger>
      autocomplete_unrecognized_fallback_logger_;
  // The manual fallback logger is used to collect metrics
  // around Autofill being triggered on unclassified fields or fields that are
  // classified differently from the target `FillingProduct` (address or
  // payments).
  std::unique_ptr<autofill_metrics::ManualFallbackEventLogger>
      manual_fallback_logger_;

  // Have we logged whether Autofill is enabled for this page load?
  bool has_logged_autofill_enabled_ = false;
  // Has the user manually edited at least one form field among the autofillable
  // ones?
  bool user_did_type_ = false;

  // TODO(crbug.com/354043809): Move out of BAM.
  // Does |this| have any parsed forms?
  bool has_parsed_forms_ = false;
  // Is there a field with autocomplete="one-time-code" observed?
  bool has_observed_one_time_code_field_ = false;
  // Is there a field with phone number collection observed?
  bool has_observed_phone_number_field_ = false;
  // If this is true, we consider the form to be secure. (Only use this for
  // testing purposes).
  std::optional<bool> consider_form_as_secure_for_testing_;

  // When the user first interacted with a potentially fillable form on this
  // page.
  base::TimeTicks initial_interaction_timestamp_;

  // A copy of the currently interacted form data.
  std::unique_ptr<FormData> pending_form_data_;

  // The credit card access manager, used to access local and server cards.
  // Lazily initialized: access only through GetCreditCardAccessManager().
  std::unique_ptr<CreditCardAccessManager> credit_card_access_manager_;

  // Helper class to autofill forms and fields. Do not use directly, use
  // form_filler() instead.
  std::unique_ptr<FormFiller> form_filler_;

  // Collected information about the autofill form where a credit card will be
  // filled.
  FormData credit_card_form_;
  FormFieldData credit_card_field_;
  CreditCard credit_card_;
  std::u16string last_unlocked_credit_card_cvc_;

  // Used to record metrics. This should be set at the beginning of the
  // interaction and re-used throughout the context of this manager.
  AutofillMetrics::PaymentsSigninState signin_state_for_metrics_ =
      AutofillMetrics::PaymentsSigninState::kUnknown;

  // List of callbacks to be called for sending blur votes. Only one callback is
  // stored per FormSignature. We rely on FormSignatures rather than
  // FormGlobalId to send votes for the various signatures of a form while it
  // evolves (when fields are added or removed). The list of callbacks is
  // ordered by time of creation: newest elements first. If the list becomes too
  // long, the oldest pending callbacks are just called and popped removed the
  // list.
  //
  // Callbacks are triggered in the following situations:
  // - We observe a form submission.
  // - The list becomes to large.

  // Callbacks are wiped in the following situations:
  // - A form is submitted.
  // - A callback is overridden by a more recent version.
  std::list<std::pair<FormSignature, base::OnceClosure>> queued_vote_uploads_;

  // This task runner sequentializes calls to
  // DeterminePossibleFieldTypesForUpload to ensure that blur votes are
  // processed before form submission votes. This is important so that a
  // submission can trigger the upload of blur votes.
  scoped_refptr<base::SequencedTaskRunner> vote_upload_task_runner_;

  // When the form was submitted.
  base::TimeTicks form_submitted_timestamp_;

  // The source that triggered unlocking a server card with the CVC.
  std::optional<AutofillTriggerSource> fetched_credit_card_trigger_source_;

  // Contains a list of four digit combinations that were found in the webpage
  // DOM. Populated after a standalone cvc field is processed on a form. Used to
  // confirm that the virtual card last four is present in the webpage for card
  // on file case.
  std::vector<std::string> four_digit_combinations_in_dom_;

  base::WeakPtrFactory<BrowserAutofillManager> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_BROWSER_AUTOFILL_MANAGER_H_
