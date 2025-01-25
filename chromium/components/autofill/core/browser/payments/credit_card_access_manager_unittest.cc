// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_access_manager.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/field_trial.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_data_importer_test_api.h"
#include "components/autofill/core/browser/metrics/form_events/credit_card_form_event_logger.h"
#include "components/autofill/core/browser/metrics/payments/better_auth_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_authentication_metrics.h"
#include "components/autofill/core/browser/metrics/payments/card_unmask_flow_metrics.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager_test_api.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/credit_card_risk_based_authenticator.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_payments_window_manager.h"
#include "components/autofill/core/browser/payments/test/test_credit_card_otp_authenticator.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_network_interface.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/test/test_sync_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"
#include "components/autofill/core/browser/payments/test_internal_authenticator.h"
#include "components/autofill/core/browser/strike_databases/payments/fido_authentication_strike_database.h"
#include "content/public/test/mock_navigation_handle.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

using base::ASCIIToUTF16;
using testing::NiceMock;

namespace autofill {
namespace {

using PaymentsRpcCardType =
    payments::PaymentsAutofillClient::PaymentsRpcCardType;
using PaymentsRpcResult = payments::PaymentsAutofillClient::PaymentsRpcResult;

constexpr char kTestGUID[] = "00000000-0000-0000-0000-000000000001";
constexpr char kTestGUID2[] = "00000000-0000-0000-0000-000000000002";
constexpr char kTestNumber[] = "4234567890123456";  // Visa
constexpr char kTestNumber2[] = "5454545454545454";
constexpr char16_t kTestNumber16[] = u"4234567890123456";
constexpr char16_t kTestCvc16[] = u"123";
constexpr char kTestServerId[] = "server_id_1";
constexpr char kTestServerId2[] = "server_id_2";

using autofill_metrics::CreditCardFormEventLogger;

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
constexpr char kTestCvc[] = "123";
// Base64 encoding of "This is a test challenge".
constexpr char kTestChallenge[] = "VGhpcyBpcyBhIHRlc3QgY2hhbGxlbmdl";
// Base64 encoding of "This is a test Credential ID".
constexpr char kCredentialId[] = "VGhpcyBpcyBhIHRlc3QgQ3JlZGVudGlhbCBJRC4=";
constexpr char kGooglePaymentsRpid[] = "google.com";

std::string BytesToBase64(const std::vector<uint8_t>& bytes) {
  return base::Base64Encode(bytes);
}
#endif

// Implements an `OnCreditCardFetched()` callback that stores the values it
// receives.
class TestAccessor {
 public:
  TestAccessor() = default;

  base::WeakPtr<TestAccessor> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnCreditCardFetched(CreditCardFetchResult result,
                           const CreditCard* card) {
    result_ = result;
    if (result == CreditCardFetchResult::kSuccess) {
      DCHECK(card);
      number_ = card->number();
      cvc_ = card->cvc();
      expiry_month_ = card->Expiration2DigitMonthAsString();
      expiry_year_ = card->Expiration4DigitYearAsString();
    }
  }

  std::u16string number() { return number_; }
  std::u16string cvc() { return cvc_; }
  std::u16string expiry_month() { return expiry_month_; }
  std::u16string expiry_year() { return expiry_year_; }
  CreditCardFetchResult result() { return result_; }

 private:
  // The result of the credit card fetching.
  CreditCardFetchResult result_ = CreditCardFetchResult::kNone;
  // The card number returned from OnCreditCardFetched().
  std::u16string number_;
  // The returned CVC, if any.
  std::u16string cvc_;
  // The two-digit expiration month in string.
  std::u16string expiry_month_;
  // The four-digit expiration year in string.
  std::u16string expiry_year_;
  base::WeakPtrFactory<TestAccessor> weak_ptr_factory_{this};
};

}  // namespace

class CreditCardAccessManagerTest : public testing::Test {
 public:
  // The type of request options to be returned with a CVC auth response.
  enum class TestFidoRequestOptionsType {
    kValid = 0,
    kInvalid = 1,
    kNotPresent = 2
  };

  CreditCardAccessManagerTest()
      : task_environment_(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME,
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::QUEUED) {
    // Advance the mock clock to 2021-01-01, 00:00:00.000.
    base::Time year_2021;
    CHECK(base::Time::FromUTCExploded({.year = 2021,
                                       .month = 1,
                                       .day_of_week = 4,
                                       .day_of_month = 1,
                                       .hour = 0,
                                       .minute = 0,
                                       .second = 0,
                                       .millisecond = 0},
                                      &year_2021));
    task_environment_.AdvanceClock(year_2021 -
                                   task_environment_.GetMockClock()->Now());
  }

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data().SetPrefService(autofill_client_.GetPrefs());
    personal_data().SetSyncServiceForTest(&sync_service_);
#if BUILDFLAG(IS_IOS)
    // On iOS mandatory reauth is by default enabled. Disable it explicitly
    // to not interfere with tests that do not test reauth functionalities.
    autofill_client_.GetPrefs()->SetBoolean(
        prefs::kAutofillPaymentMethodsMandatoryReauth, false);
#endif
    accessor_ = std::make_unique<TestAccessor>();
    autofill_driver_ = std::make_unique<TestAutofillDriver>(&autofill_client_);

    autofill_client_.GetPaymentsAutofillClient()
        ->set_test_payments_network_interface(
            std::make_unique<payments::TestPaymentsNetworkInterface>(
                autofill_client_.GetURLLoaderFactory(),
                autofill_client_.GetIdentityManager(), &personal_data()));
    autofill_client_.set_test_strike_database(
        std::make_unique<TestStrikeDatabase>());
    autofill_driver_->set_autofill_manager(
        std::make_unique<TestBrowserAutofillManager>(autofill_driver_.get()));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    autofill_driver_->SetAuthenticator(new TestInternalAuthenticator());
    test_api(credit_card_access_manager())
        .set_fido_authenticator(
            std::make_unique<TestCreditCardFidoAuthenticator>(
                autofill_driver_.get(), &autofill_client_));
#endif
    auto otp_authenticator =
        std::make_unique<TestCreditCardOtpAuthenticator>(&autofill_client_);
    otp_authenticator_ = otp_authenticator.get();
    autofill_client_.GetPaymentsAutofillClient()->set_otp_authenticator(
        std::move(otp_authenticator));

    // Force creation of the CreditCardAccessManager.
    std::ignore = credit_card_access_manager();
  }

  bool IsAuthenticationInProgress() {
    return test_api(credit_card_access_manager())
        .is_authentication_in_progress();
  }

  void ResetFetchCreditCard() {
    // Resets all variables related to credit card fetching.
    test_api(credit_card_access_manager())
        .set_is_authentication_in_progress(false);
    test_api(credit_card_access_manager()).set_can_fetch_unmask_details(true);
    test_api(credit_card_access_manager())
        .set_unmask_details_request_in_progress(false);
    test_api(credit_card_access_manager()).set_is_user_verifiable(std::nullopt);
  }

  void ClearCards() {
    personal_data().test_payments_data_manager().ClearCreditCards();
  }

  void CreateLocalCard(std::string guid, std::string number = std::string()) {
    CreditCard local_card = CreditCard();
    test::SetCreditCardInfo(&local_card, "Elvis Presley", number.c_str(),
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1", kTestCvc16);
    local_card.set_guid(guid);
    local_card.set_record_type(CreditCard::RecordType::kLocalCard);

    personal_data().payments_data_manager().AddCreditCard(local_card);
  }

  CreditCard* CreateServerCard(std::string guid,
                               std::string number = std::string(),
                               bool masked = true,
                               std::string server_id = std::string()) {
    CreditCard server_card = CreditCard();
    test::SetCreditCardInfo(&server_card, "Elvis Presley", number.c_str(),
                            test::NextMonth().c_str(), test::NextYear().c_str(),
                            "1", kTestCvc16);
    server_card.set_guid(guid);
    server_card.set_record_type(masked
                                    ? CreditCard::RecordType::kMaskedServerCard
                                    : CreditCard::RecordType::kFullServerCard);
    server_card.set_server_id(server_id);
    personal_data().test_payments_data_manager().AddServerCreditCard(
        server_card);
    return personal_data().payments_data_manager().GetCreditCardByGUID(guid);
  }

  CreditCardCvcAuthenticator& GetCvcAuthenticator() {
    return autofill_client_.GetPaymentsAutofillClient()->GetCvcAuthenticator();
  }

  void MockUserResponseForCvcAuth(std::u16string cvc, bool enable_fido) {
    payments::FullCardRequest* full_card_request =
        GetCvcAuthenticator().full_card_request_.get();
    if (!full_card_request)
      return;

    // Mock user response.
    payments::FullCardRequest::UserProvidedUnmaskDetails details;
    details.cvc = cvc;
#if BUILDFLAG(IS_ANDROID)
    details.enable_fido_auth = enable_fido;
#endif
    full_card_request->OnUnmaskPromptAccepted(details);
    full_card_request->OnDidGetUnmaskRiskData(/*risk_data=*/"");
  }

  // Returns true if full card request was sent from CVC auth.
  bool GetRealPanForCVCAuth(
      PaymentsRpcResult result,
      const std::string& real_pan,
      TestFidoRequestOptionsType test_fido_request_options_type =
          TestFidoRequestOptionsType::kNotPresent) {
    payments::FullCardRequest* full_card_request =
        GetCvcAuthenticator().full_card_request_.get();

    if (!full_card_request)
      return false;

    MockUserResponseForCvcAuth(kTestCvc16,
                               /*enable_fido=*/test_fido_request_options_type !=
                                   TestFidoRequestOptionsType::kNotPresent);

    payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    response.card_authorization_token = "dummy_card_authorization_token";
    if (test_fido_request_options_type == TestFidoRequestOptionsType::kValid) {
      response.fido_request_options = GetTestRequestOptions();
    } else if (test_fido_request_options_type ==
               TestFidoRequestOptionsType::kInvalid) {
      response.fido_request_options =
          GetTestRequestOptions(/*return_invalid_request_options=*/true);
    }
#endif
    response.card_type = PaymentsRpcCardType::kServerCard;
    full_card_request->OnDidGetRealPan(result,
                                       response.with_real_pan(real_pan));
    return true;
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  void AddMaxStrikes() {
    auto* strike_database =
        GetFIDOAuthenticator()->GetOrCreateFidoAuthenticationStrikeDatabase();
    CHECK(strike_database);
    strike_database->AddStrikes(strike_database->GetMaxStrikesLimit());
  }

  void ClearStrikes() {
    auto* strike_database =
        GetFIDOAuthenticator()->GetOrCreateFidoAuthenticationStrikeDatabase();
    CHECK(strike_database);
    strike_database->ClearAllStrikes();
  }

  int GetStrikes() {
    auto* strike_database =
        GetFIDOAuthenticator()->GetOrCreateFidoAuthenticationStrikeDatabase();
    CHECK(strike_database);
    return strike_database->GetStrikes();
  }

  base::Value::Dict GetTestRequestOptions(
      bool return_invalid_request_options = false) {
    base::Value::Dict request_options;
    request_options.Set("challenge", base::Value(kTestChallenge));
    request_options.Set("relying_party_id", base::Value(kGooglePaymentsRpid));

    // If invalid request options are to be returned, don't set key info or
    // credential ID.
    if (return_invalid_request_options) {
      return request_options;
    }

    base::Value::Dict key_info;
    key_info.Set("credential_id", base::Value(kCredentialId));
    request_options.Set("key_info", base::Value(base::Value::Type::LIST));
    request_options.FindList("key_info")->Append(std::move(key_info));
    return request_options;
  }

  base::Value::Dict GetTestCreationOptions() {
    base::Value::Dict creation_options;
    creation_options.Set("challenge", base::Value(kTestChallenge));
    creation_options.Set("relying_party_id", base::Value(kGooglePaymentsRpid));
    return creation_options;
  }

  // Returns true if full card request was sent from FIDO auth.
  bool GetRealPanForFIDOAuth(PaymentsRpcResult result,
                             const std::string& real_pan,
                             const std::string& dcvv = std::string(),
                             bool is_virtual_card = false) {
    payments::FullCardRequest* full_card_request =
        GetFIDOAuthenticator()->full_card_request_.get();

    if (!full_card_request)
      return false;

    payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
    response.card_type = is_virtual_card ? PaymentsRpcCardType::kVirtualCard
                                         : PaymentsRpcCardType::kServerCard;
    full_card_request->OnDidGetRealPan(
        result, response.with_real_pan(real_pan).with_dcvv(dcvv));
    return true;
  }

  // Mocks an OptChange response from the PaymentsNetworkInterface.
  void OptChange(PaymentsRpcResult result,
                 bool user_is_opted_in,
                 bool include_creation_options = false,
                 bool include_request_options = false) {
    payments::PaymentsNetworkInterface::OptChangeResponseDetails response;
    response.user_is_opted_in = user_is_opted_in;
    if (include_creation_options) {
      response.fido_creation_options = GetTestCreationOptions();
    }
    if (include_request_options) {
      response.fido_request_options = GetTestRequestOptions();
    }
    GetFIDOAuthenticator()->OnDidGetOptChangeResult(result, response);
  }

  TestCreditCardFidoAuthenticator* GetFIDOAuthenticator() {
    return static_cast<TestCreditCardFidoAuthenticator*>(
        credit_card_access_manager().GetOrCreateFidoAuthenticator());
  }
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  // Mocks user response for the offer dialog.
  void AcceptWebauthnOfferDialog(bool did_accept) {
    GetFIDOAuthenticator()->OnWebauthnOfferDialogUserResponse(did_accept);
  }
#endif

  void InvokeDelayedGetUnmaskDetailsResponse() {
    test_api(credit_card_access_manager())
        .OnDidGetUnmaskDetails(PaymentsRpcResult::kSuccess,
                               *payments_network_interface().unmask_details());
  }

  void InvokeUnmaskDetailsTimeout() {
    test_api(credit_card_access_manager())
        .ready_to_start_authentication()
        .Signal();
    test_api(credit_card_access_manager()).set_can_fetch_unmask_details(true);
  }

  void WaitForCallbacks() { task_environment_.RunUntilIdle(); }

  void SetCreditCardFIDOAuthEnabled(bool enabled) {
    prefs::SetCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs(), enabled);
  }

  bool IsCreditCardFIDOAuthEnabled() {
    return prefs::IsCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs());
  }

  UnmaskAuthFlowType GetUnmaskAuthFlowType() {
    return test_api(credit_card_access_manager()).unmask_auth_flow_type();
  }

  void MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      bool fido_authenticator_is_user_opted_in,
      bool is_user_verifiable,
      const std::vector<CardUnmaskChallengeOption>& challenge_options,
      int selected_index) {
    CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
    CreditCard* virtual_card =
        personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
    virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    fido_authenticator().set_is_user_opted_in(
        fido_authenticator_is_user_opted_in);
#endif

    // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
    // |is_user_verifiable_| related logic from CreditCardAccessManager to
    // CreditCardFidoAuthenticator.
    test_api(credit_card_access_manager())
        .set_is_user_verifiable(is_user_verifiable);
    credit_card_access_manager().FetchCreditCard(
        virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                     accessor_->GetWeakPtr()));

    // This checks risk-based authentication flow is successfully invoked,
    // because it is always the very first authentication flow in a VCN
    // unmasking flow.
    EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                    ->risk_based_authentication_invoked());
    // Mock server response with information regarding VCN auth.
    payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
    response.context_token = "fake_context_token";
    response.card_unmask_challenge_options = challenge_options;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    if (fido_authenticator_is_user_opted_in)
      response.fido_request_options = GetTestRequestOptions();
#endif
    credit_card_access_manager()
        .OnVirtualCardRiskBasedAuthenticationResponseReceived(
            PaymentsRpcResult::kSuccess, response);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
    // This if-statement ensures that fido-related flows run correctly.
    if (fido_authenticator_is_user_opted_in) {
      // Expect the CreditCardAccessManager invokes the FIDO authenticator
      // first.
      ASSERT_TRUE(fido_authenticator().authenticate_invoked());
      EXPECT_EQ(fido_authenticator().card().number(),
                base::UTF8ToUTF16(std::string(kTestNumber)));
      EXPECT_EQ(fido_authenticator().card().record_type(),
                CreditCard::RecordType::kVirtualCard);
      ASSERT_TRUE(fido_authenticator().context_token().has_value());
      EXPECT_EQ(fido_authenticator().context_token().value(),
                "fake_context_token");

      CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response{
          .did_succeed = false};
      test_api(credit_card_access_manager())
          .OnFIDOAuthenticationComplete(fido_response);
    }
#endif

    const CardUnmaskChallengeOption& challenge_option =
        response.card_unmask_challenge_options[selected_index];

    payments::PaymentsWindowManager::Vcn3dsContext vcn_3ds_context;
    if (challenge_option.type ==
        CardUnmaskChallengeOptionType::kThreeDomainSecure) {
      EXPECT_CALL(*static_cast<payments::MockPaymentsWindowManager*>(
                      autofill_client_.GetPaymentsAutofillClient()
                          ->GetPaymentsWindowManager()),
                  InitVcn3dsAuthentication)
          .Times(1)
          .WillOnce(
              [&vcn_3ds_context](
                  payments::PaymentsWindowManager::Vcn3dsContext context) {
                vcn_3ds_context = std::move(context);
              });
    }

    test_api(credit_card_access_manager())
        .OnUserAcceptedAuthenticationSelectionDialog(
            challenge_option.id.value());

    // TODO(crbug.com/329523854): Check that the challenge selection acceptance
    // was handled correctly using mocks instead of test classes.
    switch (challenge_option.type) {
      case CardUnmaskChallengeOptionType::kCvc: {
        CreditCardCvcAuthenticator& cvc_authenticator =
            autofill_client_.GetPaymentsAutofillClient()->GetCvcAuthenticator();
        payments::PaymentsNetworkInterface::UnmaskRequestDetails*
            request_details =
                cvc_authenticator.GetFullCardRequest()->request_.get();
        EXPECT_EQ(request_details->card.record_type(),
                  CreditCard::RecordType::kVirtualCard);
        EXPECT_EQ(request_details->card.number(),
                  base::UTF8ToUTF16(std::string(kTestNumber)));
        EXPECT_EQ(request_details->context_token, "fake_context_token");
        EXPECT_EQ(request_details->selected_challenge_option->id.value(),
                  "234");
        EXPECT_EQ(request_details->selected_challenge_option->type,
                  CardUnmaskChallengeOptionType::kCvc);
        break;
      }
      case CardUnmaskChallengeOptionType::kSmsOtp:
        VerifyOnSelectChallengeOptionInvoked();
        EXPECT_EQ(otp_authenticator_->selected_challenge_option().id.value(),
                  "123");
        EXPECT_EQ(otp_authenticator_->selected_challenge_option().type,
                  CardUnmaskChallengeOptionType::kSmsOtp);
        EXPECT_EQ(
            otp_authenticator_->selected_challenge_option().challenge_info,
            u"xxx-xxx-3547");
        break;
      case CardUnmaskChallengeOptionType::kEmailOtp:
        VerifyOnSelectChallengeOptionInvoked();
        EXPECT_EQ(otp_authenticator_->selected_challenge_option().id.value(),
                  "345");
        EXPECT_EQ(otp_authenticator_->selected_challenge_option().type,
                  CardUnmaskChallengeOptionType::kEmailOtp);
        EXPECT_EQ(
            otp_authenticator_->selected_challenge_option().challenge_info,
            u"a******b@google.com");
        break;
      case CardUnmaskChallengeOptionType::kThreeDomainSecure:
        EXPECT_EQ(vcn_3ds_context.context_token, response.context_token);
        EXPECT_EQ(vcn_3ds_context.card, *virtual_card);
        EXPECT_EQ(vcn_3ds_context.challenge_option.type,
                  CardUnmaskChallengeOptionType::kThreeDomainSecure);
        EXPECT_TRUE(vcn_3ds_context.user_consent_already_given);
        break;
      case CardUnmaskChallengeOptionType::kUnknownType:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  void VerifyOnSelectChallengeOptionInvoked() {
    DCHECK(otp_authenticator_);
    EXPECT_TRUE(otp_authenticator_->on_challenge_option_selected_invoked());
    EXPECT_EQ(otp_authenticator_->card().number(),
              base::UTF8ToUTF16(std::string(kTestNumber)));
    EXPECT_EQ(otp_authenticator_->card().record_type(),
              CreditCard::RecordType::kVirtualCard);
    EXPECT_EQ(otp_authenticator_->context_token(), "fake_context_token");
  }

 protected:
  CreditCardAccessManager& credit_card_access_manager() {
    return static_cast<BrowserAutofillManager&>(
               autofill_driver_->GetAutofillManager())
        .GetCreditCardAccessManager();
  }
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  TestCreditCardFidoAuthenticator& fido_authenticator() {
    return static_cast<TestCreditCardFidoAuthenticator&>(
        *credit_card_access_manager().GetOrCreateFidoAuthenticator());
  }
#endif
  payments::TestPaymentsNetworkInterface& payments_network_interface() {
    return *autofill_client_.GetPaymentsAutofillClient()
                ->GetPaymentsNetworkInterface();
  }
  TestPersonalDataManager& personal_data() {
    return *autofill_client_.GetPersonalDataManager();
  }

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  void OptUserInToFido() {
    std::string other_server_id = "00000000-0000-0000-0000-000000000034";
    // Add a random FIDO eligible card, it will return RequestOptions in unmask
    // details.
    payments_network_interface().AddFidoEligibleCard(
        other_server_id, kCredentialId, kGooglePaymentsRpid);
    GetFIDOAuthenticator()->SetUserVerifiable(true);
    SetCreditCardFIDOAuthEnabled(true);
  }
#endif

  std::unique_ptr<TestAccessor> accessor_;
  base::test::TaskEnvironment task_environment_;
  variations::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
  syncer::TestSyncService sync_service_;
  TestAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  scoped_refptr<AutofillWebDataService> database_;
  raw_ptr<TestCreditCardOtpAuthenticator> otp_authenticator_;
};

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_IOS)

class CreditCardAccessManagerMandatoryReauthTest
    : public CreditCardAccessManagerTest {
 public:
  CreditCardAccessManagerMandatoryReauthTest() = default;
  ~CreditCardAccessManagerMandatoryReauthTest() override = default;

 protected:
  void SetUp() override {
    CreditCardAccessManagerTest::SetUp();
    feature_list_.InitAndEnableFeature(
        features::kAutofillEnableFpanRiskBasedAuthentication);
#if BUILDFLAG(IS_ANDROID)
    if (base::android::BuildInfo::GetInstance()->is_automotive()) {
      autofill_client_.GetPrefs()->SetBoolean(
          prefs::kAutofillPaymentMethodsMandatoryReauth,
          /*value=*/true);
      return;
    }
#endif  // BUILDFLAG(IS_ANDROID)
    autofill_client_.GetPrefs()->SetBoolean(
        prefs::kAutofillPaymentMethodsMandatoryReauth,
        /*value=*/PrefIsEnabled());
  }

  void SetUpDeviceAuthenticatorResponseMock() {
    ON_CALL(mandatory_reauth_manager(), GetAuthenticationMethod)
        .WillByDefault(testing::Return(GetAuthenticationMethod()));

    // We should only expect an AuthenticateWithMessage() call if the feature
    // flag is on and the pref is enabled, or if the device is automotive.
    if (IsMandatoryReauthEnabled()) {
      ON_CALL(mandatory_reauth_manager(),
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
              AuthenticateWithMessage)
          .WillByDefault(testing::WithArg<1>(
#elif BUILDFLAG(IS_ANDROID)
              Authenticate)
          .WillByDefault(testing::WithArg<0>(
#endif
              testing::Invoke([mandatory_reauth_response_is_success =
                                   MandatoryReauthResponseIsSuccess()](
                                  base::OnceCallback<void(bool)> callback) {
                std::move(callback).Run(mandatory_reauth_response_is_success);
              })));
    } else {
      EXPECT_CALL(mandatory_reauth_manager(),
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_IOS)
                  AuthenticateWithMessage)
#elif BUILDFLAG(IS_ANDROID)
                  Authenticate)
#endif
          .Times(0);
    }
  }

  payments::MockMandatoryReauthManager& mandatory_reauth_manager() {
    return *static_cast<payments::MockMandatoryReauthManager*>(
        autofill_client_.GetOrCreatePaymentsMandatoryReauthManager());
  }

  virtual bool PrefIsEnabled() const = 0;

  virtual bool MandatoryReauthResponseIsSuccess() const = 0;

  virtual bool HasAuthenticator() const = 0;

  virtual payments::MandatoryReauthAuthenticationMethod
  GetAuthenticationMethod() const = 0;

  bool IsMandatoryReauthEnabled() {
#if BUILDFLAG(IS_ANDROID)
    if (base::android::BuildInfo::GetInstance()->is_automotive()) {
      return true;
    }
#endif
    return PrefIsEnabled();
  }

  base::test::ScopedFeatureList feature_list_;
};

// Parameters of the CreditCardAccessManagerMandatoryReauthFunctionalTest:
// - bool pref_is_enabled: Whether the mandatory re-auth pref is turned on or
// off.
// - bool mandatory_reauth_response_is_success: Whether the response from the
// mandatory re-auth is a success or failure.
// - bool authentication_method: The authentication method that is supported.
class CreditCardAccessManagerMandatoryReauthFunctionalTest
    : public CreditCardAccessManagerMandatoryReauthTest,
      public testing::WithParamInterface<
          std::tuple<bool,
                     bool,
                     payments::MandatoryReauthAuthenticationMethod>> {
 public:
  CreditCardAccessManagerMandatoryReauthFunctionalTest() = default;
  ~CreditCardAccessManagerMandatoryReauthFunctionalTest() override = default;

  bool PrefIsEnabled() const override { return std::get<0>(GetParam()); }

  bool MandatoryReauthResponseIsSuccess() const override {
    return std::get<1>(GetParam());
  }

  bool HasAuthenticator() const override {
    return std::get<2>(GetParam()) !=
           payments::MandatoryReauthAuthenticationMethod::kUnsupportedMethod;
  }

  payments::MandatoryReauthAuthenticationMethod GetAuthenticationMethod()
      const override {
    return std::get<2>(GetParam());
  }

  std::string GetStringForAuthenticationMethod() const {
    switch (GetAuthenticationMethod()) {
      case payments::MandatoryReauthAuthenticationMethod::kUnsupportedMethod:
        return ".UnsupportedMethod";
      case payments::MandatoryReauthAuthenticationMethod::kBiometric:
        return ".Biometric";
      case payments::MandatoryReauthAuthenticationMethod::kScreenLock:
        return ".ScreenLock";
      case payments::MandatoryReauthAuthenticationMethod::kUnknown:
        NOTIMPLEMENTED();
        return "";
    }
  }
};

// Tests that retrieving local cards works correctly in the context of the
// Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthFunctionalTest,
       MandatoryReauth_FetchLocalCard) {
  base::HistogramTester histogram_tester;
  CreateLocalCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  card->set_cvc(kTestCvc16);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  // TODO(crbug.com/40935048): Extract shared boilerplate code out for
  // CreditCardAccessManagerMandatoryReauthFunctionalTest tests.
  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  // The only time we should expect an error is if mandatory re-auth is
  // enabled, but the mandatory re-auth authentication was not successful.
  if (IsMandatoryReauthEnabled() && HasAuthenticator() &&
      !MandatoryReauthResponseIsSuccess()) {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
    EXPECT_TRUE(accessor_->number().empty());
  } else {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
    EXPECT_EQ(accessor_->number(), kTestNumber16);
    EXPECT_EQ(accessor_->cvc(), kTestCvc16);
  }
  std::string reauth_usage_histogram_name =
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage.LocalCard";
  reauth_usage_histogram_name += GetStringForAuthenticationMethod();
  if (IsMandatoryReauthEnabled()) {
    if (HasAuthenticator()) {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowStarted,
          1);
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowSucceeded
              : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowFailed,
          1);
      histogram_tester.ExpectUniqueSample(
          "Autofill.ServerCardUnmask.LocalCard.Result.DeviceUnlock",
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::ServerCardUnmaskResult::
                    kAuthenticationUnmasked
              : autofill_metrics::ServerCardUnmaskResult::kAuthenticationError,
          1);
    } else {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowSkipped,
          1);
    }
  } else {
    histogram_tester.ExpectBucketCount(
        reauth_usage_histogram_name,
        autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
        0);
  }
}

// Tests that retrieving virtual cards works correctly in the context of the
// Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthFunctionalTest,
       MandatoryReauth_FetchVirtualCard) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with valid card information.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.real_pan = "4111111111111111";
  response.dcvv = "321";
  response.expiration_month = test::NextMonth();
  response.expiration_year = test::NextYear();
  response.card_type = PaymentsRpcCardType::kVirtualCard;

  // TODO(crbug.com/40935048): Extract shared boilerplate code out for
  // CreditCardAccessManagerMandatoryReauthFunctionalTest tests.
  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kSuccess, response);

  // Ensure the accessor received the correct response.
  if (IsMandatoryReauthEnabled() && HasAuthenticator() &&
      !MandatoryReauthResponseIsSuccess()) {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  } else {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
    EXPECT_EQ(accessor_->number(), u"4111111111111111");
    EXPECT_EQ(accessor_->cvc(), u"321");
    EXPECT_EQ(accessor_->expiry_month(), base::UTF8ToUTF16(test::NextMonth()));
    EXPECT_EQ(accessor_->expiry_year(), base::UTF8ToUTF16(test::NextYear()));
  }
  std::string reauth_usage_histogram_name =
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage.VirtualCard";
  reauth_usage_histogram_name += GetStringForAuthenticationMethod();
  if (IsMandatoryReauthEnabled()) {
    if (HasAuthenticator()) {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowStarted,
          1);
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowSucceeded
              : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowFailed,
          1);
      histogram_tester.ExpectUniqueSample(
          "Autofill.CvcStorage.CvcFilling.VirtualCard",
          autofill_metrics::CvcFillingFlowType::kMandatoryReauth,
          MandatoryReauthResponseIsSuccess() ? 1 : 0);
      histogram_tester.ExpectUniqueSample(
          "Autofill.ServerCardUnmask.VirtualCard.Result.DeviceUnlock",
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::ServerCardUnmaskResult::
                    kAuthenticationUnmasked
              : autofill_metrics::ServerCardUnmaskResult::kAuthenticationError,
          1);
    } else {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowSkipped,
          1);
    }
  } else {
    histogram_tester.ExpectBucketCount(
        reauth_usage_histogram_name,
        autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
        0);
  }
}

// Tests that retrieving masked server cards triggers mandatory reauth (if
// applicable) when risk-based auth returned the card.
TEST_P(CreditCardAccessManagerMandatoryReauthFunctionalTest,
       MandatoryReauth_FetchMaskedServerCard) {
  std::string test_number = "4444333322221111";
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, test_number, /*masked=*/true, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());

  // Mock CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse to
  // successfully return the valid card number.
  CreditCard card = *masked_server_card;
  card.set_record_type(CreditCard::RecordType::kFullServerCard);

  // TODO(crbug.com/40935048): Extract shared boilerplate code out for
  // CreditCardAccessManagerMandatoryReauthFunctionalTest tests.
  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(card));

  // Ensure the accessor received the correct response.
  if (IsMandatoryReauthEnabled() && HasAuthenticator() &&
      !MandatoryReauthResponseIsSuccess()) {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  } else {
    EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
    EXPECT_EQ(accessor_->number(), base::UTF8ToUTF16(test_number));
  }
  std::string reauth_usage_histogram_name =
      "Autofill.PaymentMethods.CheckoutFlow.ReauthUsage.ServerCard";
  reauth_usage_histogram_name += GetStringForAuthenticationMethod();
  if (IsMandatoryReauthEnabled()) {
    if (HasAuthenticator()) {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowStarted,
          1);
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowSucceeded
              : autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
                    kFlowFailed,
          1);
      histogram_tester.ExpectUniqueSample(
          "Autofill.ServerCardUnmask.ServerCard.Result.DeviceUnlock",
          MandatoryReauthResponseIsSuccess()
              ? autofill_metrics::ServerCardUnmaskResult::
                    kAuthenticationUnmasked
              : autofill_metrics::ServerCardUnmaskResult::kAuthenticationError,
          1);
    } else {
      histogram_tester.ExpectBucketCount(
          reauth_usage_histogram_name,
          autofill_metrics::MandatoryReauthAuthenticationFlowEvent::
              kFlowSkipped,
          1);
    }
  } else {
    histogram_tester.ExpectBucketCount(
        reauth_usage_histogram_name,
        autofill_metrics::MandatoryReauthAuthenticationFlowEvent::kFlowStarted,
        0);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ,
    CreditCardAccessManagerMandatoryReauthFunctionalTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Values(
            payments::MandatoryReauthAuthenticationMethod::kUnsupportedMethod,
            payments::MandatoryReauthAuthenticationMethod::kBiometric,
            payments::MandatoryReauthAuthenticationMethod::kScreenLock)));

// Test suite built for testing mandatory re-auth's functionality as an
// integration with other projects.
// -- bool mandatory_reauth_response_is_success: Whether or not the re-auth
// authentication was successful.
class CreditCardAccessManagerMandatoryReauthIntegrationTest
    : public CreditCardAccessManagerMandatoryReauthTest,
      public testing::WithParamInterface<bool> {
 public:
  CreditCardAccessManagerMandatoryReauthIntegrationTest() = default;
  ~CreditCardAccessManagerMandatoryReauthIntegrationTest() override = default;

 protected:

  bool PrefIsEnabled() const override { return true; }

  bool MandatoryReauthResponseIsSuccess() const override { return GetParam(); }

  bool HasAuthenticator() const override { return true; }

  payments::MandatoryReauthAuthenticationMethod GetAuthenticationMethod()
      const override {
    return payments::MandatoryReauthAuthenticationMethod::kBiometric;
  }
};

// Tests that when retrieving local cards with a CVC stored, the CVC is filled.
// This test is in the context of the Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthIntegrationTest,
       MandatoryReauth_FetchLocalCard_CvcFillWorksCorrectly) {
  base::HistogramTester histogram_tester;
  CreateLocalCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  // TODO(crbug.com/40935048): Extract shared boilerplate code out for
  // CreditCardAccessManagerMandatoryReauthTest tests.
  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  EXPECT_EQ(accessor_->cvc(),
            MandatoryReauthResponseIsSuccess() ? kTestCvc16 : u"");
  histogram_tester.ExpectBucketCount(
      "Autofill.CvcStorage.CvcFilling.LocalCard",
      autofill_metrics::CvcFillingFlowType::kMandatoryReauth,
      MandatoryReauthResponseIsSuccess() ? 1 : 0);
}

// Tests that when retrieving local cards without a CVC stored, the CVC is not
// filled. This test is in the context of the Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthIntegrationTest,
       MandatoryReauth_FetchLocalCard_NoCvcFillWorksCorrectly) {
  base::HistogramTester histogram_tester;
  CreateLocalCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  card->set_cvc(u"");

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  // TODO(crbug.com/40935048): Extract shared boilerplate code out for
  // CreditCardAccessManagerMandatoryReauthTest tests.
  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  EXPECT_EQ(accessor_->cvc(), u"");
  histogram_tester.ExpectBucketCount(
      "Autofill.CvcStorage.CvcFilling.LocalCard",
      autofill_metrics::CvcFillingFlowType::kMandatoryReauth, 0);
}

// Tests that when retrieving masked server cards with a CVC stored, the CVC is
// filled.  This test is in the context of the Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthIntegrationTest,
       MandatoryReauth_FetchMaskedServerCard_CvcFillWorksCorrectly) {
  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  CreateServerCard(kTestGUID, test_number, /*masked=*/true, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // TODO(crbug.com/40935048): Extract shared boilerplate code out for
  // CreditCardAccessManagerMandatoryReauthTest tests.
  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(*masked_server_card));

  EXPECT_EQ(accessor_->cvc(),
            MandatoryReauthResponseIsSuccess() ? kTestCvc16 : u"");
  histogram_tester.ExpectBucketCount(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kMandatoryReauth,
      MandatoryReauthResponseIsSuccess() ? 1 : 0);
}

// Tests that when retrieving masked server cards without a CVC stored, the CVC
// is not filled. This test is in the context of the Mandatory Re-Auth feature.
TEST_P(CreditCardAccessManagerMandatoryReauthIntegrationTest,
       MandatoryReauth_FetchMaskedServerCard_NoCvcFillWorksCorrectly) {
  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  CreateServerCard(kTestGUID, test_number, /*masked=*/true, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  masked_server_card->set_cvc(u"");

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // TODO(crbug.com/40935048): Extract shared boilerplate code out for
  // CreditCardAccessManagerMandatoryReauthTest tests.
  SetUpDeviceAuthenticatorResponseMock();
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(*masked_server_card));

  EXPECT_EQ(accessor_->cvc(), u"");
  histogram_tester.ExpectBucketCount(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kMandatoryReauth, 0);
}

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerMandatoryReauthIntegrationTest,
                         testing::Bool());

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_ANDROID) ||
        // BUILDFLAG(IS_IOS)

// Tests retrieving local cards.
TEST_F(CreditCardAccessManagerTest, FetchLocalCardSuccess) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  CreateLocalCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed();
  ASSERT_TRUE(type.has_value());
  ASSERT_EQ(type.value(), NonInteractivePaymentMethodType::kLocalCard);
}

// Ensures that FetchCreditCard() reports a failure when a card does not exist.
TEST_F(CreditCardAccessManagerTest, FetchNullptrFailure) {
  personal_data().test_payments_data_manager().ClearCreditCards();

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      nullptr, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                              accessor_->GetWeakPtr()));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);
}

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// response from payments.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCSuccess) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  base::HistogramTester histogram_tester;
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Cvc";

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                            accessor_->GetWeakPtr()));
  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Attempt", true, 1);

  // Expect that we did not signal that there was no interactive
  // authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

// Ensures that FetchCreditCard() returns a failure upon a negative response
// from the server.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCNetworkError) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  EXPECT_TRUE(
      GetRealPanForCVCAuth(PaymentsRpcResult::kNetworkError, std::string()));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);
}

// Ensures that FetchCreditCard() returns a failure upon a negative response
// from the server.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCPermanentFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kPermanentFailure,
                                   std::string()));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);
}

// Ensures that a "try again" response from payments does not end the flow.
TEST_F(CreditCardAccessManagerTest, FetchServerCardCVCTryAgainFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  EXPECT_TRUE(
      GetRealPanForCVCAuth(PaymentsRpcResult::kTryAgainFailure, std::string()));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Params of the CreditCardAccessManagerBetterAuthLogTest:
// -- bool has_server_card;
// -- bool is_user_opted_in;
class CreditCardAccessManagerBetterAuthLogTest
    : public CreditCardAccessManagerTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CreditCardAccessManagerBetterAuthLogTest() = default;
  ~CreditCardAccessManagerBetterAuthLogTest() override = default;

  bool HasServerCard() { return std::get<0>(GetParam()); }
  bool IsUserOptedIn() { return std::get<1>(GetParam()); }

  void SetUp() override {
    ClearCards();
    if (HasServerCard()) {
      CreateServerCard(kTestGUID, kTestNumber);
    } else {
      CreateLocalCard(kTestGUID, kTestNumber);
    }
    CreditCardAccessManagerTest::SetUp();
  }

  const std::string kVerifiabilityCheckDurationMetrics =
      "Autofill.BetterAuth.UserVerifiabilityCheckDuration";
  const std::string kPreflightCallMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightCalledWithFidoOptInStatus";
  const std::string kPreflightLatencyMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightDuration";
  const std::string kPreflightFlowInitiatedMetrics =
      "Autofill.BetterAuth.CardUnmaskPreflightInitiated";
};

TEST_P(CreditCardAccessManagerBetterAuthLogTest,
       CardUnmaskPreflightCalledMetric_FidoEligible) {
  base::HistogramTester histogram_tester;
  auto* fido_authenticator = GetFIDOAuthenticator();
  fido_authenticator->SetUserVerifiable(/*is_user_verifiable=*/true);
  fido_authenticator->set_is_user_opted_in(IsUserOptedIn());
  ResetFetchCreditCard();
  credit_card_access_manager().PrepareToFetchCreditCard();
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();
  histogram_tester.ExpectTotalCount(/*name=*/kVerifiabilityCheckDurationMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  if (HasServerCard()) {
    histogram_tester.ExpectUniqueSample(kPreflightCallMetrics,
                                        /*sample=*/IsUserOptedIn(),
                                        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightCallMetrics,
                                      /*expected_count=*/0);
  }
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightCallMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightLatencyMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightFlowInitiatedMetrics,
                                    /*expected_count=*/HasServerCard() ? 1 : 0);
}

TEST_P(CreditCardAccessManagerBetterAuthLogTest,
       CardUnmaskPreflightCalledMetric_NotFidoEligible) {
  base::HistogramTester histogram_tester;
  GetFIDOAuthenticator()->SetUserVerifiable(/*is_user_verifiable=*/false);
  ResetFetchCreditCard();
  credit_card_access_manager().PrepareToFetchCreditCard();
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();
  if (HasServerCard()) {
    histogram_tester.ExpectUniqueSample(
        /*name=*/kPreflightFlowInitiatedMetrics, /*sample=*/true,
        /*expected_bucket_count=*/1);
    histogram_tester.ExpectTotalCount(
        /*name=*/kVerifiabilityCheckDurationMetrics,
        /*expected_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(/*name=*/kPreflightFlowInitiatedMetrics,
                                      /*expected_count=*/0);
    histogram_tester.ExpectTotalCount(
        /*name=*/kVerifiabilityCheckDurationMetrics,
        /*expected_count=*/0);
  }
  histogram_tester.ExpectTotalCount(
      /*name=*/kPreflightCallMetrics,
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(/*name=*/kPreflightLatencyMetrics,
                                    /*expected_count=*/0);
}

INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerBetterAuthLogTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// WebAuthn verification and response from payments.
TEST_F(CreditCardAccessManagerTest, FetchServerCardFIDOSuccess) {
  base::HistogramTester histogram_tester;
  std::string unmask_decision_histogram_name =
      "Autofill.BetterAuth.CardUnmaskTypeDecision";
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Fido";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();
  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // FIDO Success.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);

  EXPECT_EQ(kCredentialId,
            BytesToBase64(GetFIDOAuthenticator()->GetCredentialId()));
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  histogram_tester.ExpectUniqueSample(
      unmask_decision_histogram_name,
      autofill_metrics::CardUnmaskTypeDecisionMetric::kFidoOnly, 1);
  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido.ServerCard.Success", 1);
  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that CVC filling gets logged after FIDO success if the card has CVC.
TEST_F(CreditCardAccessManagerTest, LogCvcFillingFIDOSuccess) {
  base::HistogramTester histogram_tester;

  CreditCard server_card = test::WithCvc(test::GetMaskedServerCard());
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          server_card.instrument_id());
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // FIDO Success.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kFido, 1);
}

// Ensures that CVC filling doesn't get logged after FIDO success if the card
// doesn't have CVC.
TEST_F(CreditCardAccessManagerTest, DoNotLogCvcFillingFIDOSuccess) {
  base::HistogramTester histogram_tester;

  CreditCard server_card = test::GetMaskedServerCard();
  server_card.set_cvc(u"");
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByInstrumentId(
          server_card.instrument_id());
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // FIDO Success.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));

  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kFido, 0);
}

// Ensures that CVC filling gets logged if a card with CVC is retrieved with
// non-interactive authentication.
TEST_F(CreditCardAccessManagerTest,
       LogCvcFillingWithoutInteractiveAuthentication) {
  base::HistogramTester histogram_tester;
  CreditCard local_card = test::WithCvc(test::GetCreditCard());
  personal_data().payments_data_manager().AddCreditCard(local_card);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(
          local_card.guid());

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.LocalCard",
      autofill_metrics::CvcFillingFlowType::kNoInteractiveAuthentication, 1);
}

// Ensures that CVC filling doesn't get logged if a card without CVC is
// retrieved with non-interactive authentication
TEST_F(CreditCardAccessManagerTest,
       DoNotLogCvcFillingWithoutInteractiveAuthentication) {
  base::HistogramTester histogram_tester;
  CreditCard local_card = test::GetCreditCard();
  personal_data().payments_data_manager().AddCreditCard(local_card);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(
          local_card.guid());

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.LocalCard",
      autofill_metrics::CvcFillingFlowType::kNoInteractiveAuthentication, 0);
}

// Ensures that accessor retrieve empty CVC upon a successful
// WebAuthn verification and response from payments using masked server card
// without CVC.
TEST_F(CreditCardAccessManagerTest, FetchServerCardWithoutCvcFIDOSuccess) {
  CreditCard server_card = CreditCard();
  test::SetCreditCardInfo(&server_card, "Elvis Presley", kTestNumber,
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1", u"");
  server_card.set_guid(kTestGUID);
  server_card.set_record_type(CreditCard::RecordType::kMaskedServerCard);
  personal_data().test_payments_data_manager().AddServerCreditCard(server_card);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // FIDO Success.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(u"", accessor_->cvc());
}

// Ensures that FetchCreditCard() returns the full PAN upon a successful
// WebAuthn verification and response from payments.
TEST_F(CreditCardAccessManagerTest, FetchServerCardFIDOSuccessWithDcvv) {
  // Opt user in for FIDO auth.
  prefs::SetCreditCardFIDOAuthEnabled(autofill_client_.GetPrefs(), true);

  // General setup.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // FIDO Success.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);

  // Mock Payments response that includes DCVV along with Full PAN.
  EXPECT_TRUE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                    kTestCvc));

  // Expect accessor to successfully retrieve the DCVV.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

// Ensures that CVC prompt is invoked after WebAuthn fails.
TEST_F(CreditCardAccessManagerTest,
       FetchServerCardFIDOVerificationFailureCVCFallback) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";
  std::string flow_events_fido_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Fido";
  std::string flow_events_cvc_fallback_histogram_name =
      "Autofill.BetterAuth.FlowEvents.CvcFallbackFromFido";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();
  histogram_tester.ExpectUniqueSample(
      flow_events_fido_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // FIDO Failure.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);

  histogram_tester.ExpectBucketCount(
      flow_events_cvc_fallback_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_FALSE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);

  // Followed by a fallback to CVC.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::NONE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kNotAllowedError, 1);
  histogram_tester.ExpectBucketCount(
      flow_events_fido_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 0);
  histogram_tester.ExpectBucketCount(
      flow_events_cvc_fallback_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that CVC prompt is invoked after payments returns an error from
// GetRealPan via FIDO.
TEST_F(CreditCardAccessManagerTest,
       FetchServerCardFIDOServerFailureCVCFallback) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.ImmediateAuthentication";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // FIDO Failure.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::AUTHENTICATION_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  EXPECT_TRUE(
      GetRealPanForFIDOAuth(PaymentsRpcResult::kPermanentFailure, kTestNumber));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);

  // Followed by a fallback to CVC.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::NONE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.CardUnmaskDuration.Fido.ServerCard.Failure", 1);
}

// Ensures WebAuthn call is not made if Request Options is missing a Credential
// ID, and falls back to CVC.
TEST_F(CreditCardAccessManagerTest,
       FetchServerCardBadRequestOptionsCVCFallback) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  // Don't set Credential ID.
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), /*credential_id=*/"", kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // FIDO Failure.
  EXPECT_FALSE(GetRealPanForFIDOAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_NE(accessor_->result(), CreditCardFetchResult::kSuccess);

  // Followed by a fallback to CVC.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

// Ensures that CVC prompt is invoked when the pre-flight call to Google
// Payments times out.
TEST_F(CreditCardAccessManagerTest, FetchServerCardFIDOTimeoutCVCFallback) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

// Ensures the existence of user-perceived latency during the preflight call is
// correctly logged.
TEST_F(CreditCardAccessManagerTest,
       Metrics_LoggingExistenceOfUserPerceivedLatency) {
  // Setting up a FIDO-enabled user with a local card and a server card.
  std::string server_guid = "00000000-0000-0000-0000-000000000001";
  std::string local_guid = "00000000-0000-0000-0000-000000000003";
  CreateServerCard(server_guid, "4594299181086168");
  CreateLocalCard(local_guid, "4409763681177079");
  CreditCard* server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(server_guid);
  CreditCard* local_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(local_guid);
  GetFIDOAuthenticator()->SetUserVerifiable(true);

  for (bool user_is_opted_in : {true, false}) {
    std::string histogram_name =
        "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.";
    histogram_name += user_is_opted_in ? "OptedIn" : "OptedOut";
    SetCreditCardFIDOAuthEnabled(user_is_opted_in);

    {
      // Preflight call ignored because local card was chosen.
      base::HistogramTester histogram_tester;

      ResetFetchCreditCard();
      credit_card_access_manager().PrepareToFetchCreditCard();
      task_environment_.FastForwardBy(base::Seconds(4));
      WaitForCallbacks();

      credit_card_access_manager().FetchCreditCard(
          local_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                     accessor_->GetWeakPtr()));
      WaitForCallbacks();

      histogram_tester.ExpectUniqueSample(
          histogram_name,
          autofill_metrics::PreflightCallEvent::kDidNotChooseMaskedCard, 1);
    }

    {
      // Preflight call returned after card was chosen.
      base::HistogramTester histogram_tester;
      payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);

      ResetFetchCreditCard();
      credit_card_access_manager().PrepareToFetchCreditCard();
      credit_card_access_manager().FetchCreditCard(
          server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                      accessor_->GetWeakPtr()));
      task_environment_.FastForwardBy(base::Seconds(4));
      WaitForCallbacks();

      histogram_tester.ExpectUniqueSample(
          histogram_name,
          autofill_metrics::PreflightCallEvent::
              kCardChosenBeforePreflightCallReturned,
          1);
      histogram_tester.ExpectTotalCount(
          "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
          "Duration",
          static_cast<int>(user_is_opted_in));
      histogram_tester.ExpectTotalCount(
          "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
          "TimedOutCvcFallback",
          static_cast<int>(user_is_opted_in));
    }

    {
      // Preflight call returned before card was chosen.
      base::HistogramTester histogram_tester;
      // This is important because CreditCardFidoAuthenticator will update the
      // opted-in pref according to GetDetailsForGetRealPan response.
      payments_network_interface().AllowFidoRegistration(!user_is_opted_in);

      ResetFetchCreditCard();
      credit_card_access_manager().PrepareToFetchCreditCard();
      WaitForCallbacks();

      credit_card_access_manager().FetchCreditCard(
          server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                      accessor_->GetWeakPtr()));
      WaitForCallbacks();

      histogram_tester.ExpectUniqueSample(
          histogram_name,
          autofill_metrics::PreflightCallEvent::
              kPreflightCallReturnedBeforeCardChosen,
          1);
    }
  }
}

// Ensures that falling back to CVC because of preflight timeout is correctly
// logged.
TEST_F(CreditCardAccessManagerTest, Metrics_LoggingTimedOutCvcFallback) {
  // Setting up a FIDO-enabled user with a local card and a server card.
  std::string server_guid = "00000000-0000-0000-0000-000000000001";
  CreateServerCard(server_guid, "4594299181086168");
  CreditCard* server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(server_guid);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);

  std::string existence_perceived_latency_histogram_name =
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn";
  std::string perceived_latency_duration_histogram_name =
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "Duration";
  std::string timeout_cvc_fallback_histogram_name =
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "TimedOutCvcFallback";

  // Preflight call arrived before timeout, after card was chosen.
  {
    base::HistogramTester histogram_tester;

    ResetFetchCreditCard();
    credit_card_access_manager().PrepareToFetchCreditCard();
    credit_card_access_manager().FetchCreditCard(
        server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                    accessor_->GetWeakPtr()));

    // Mock a delayed response.
    InvokeDelayedGetUnmaskDetailsResponse();

    task_environment_.FastForwardBy(base::Seconds(4));
    WaitForCallbacks();

    histogram_tester.ExpectUniqueSample(
        existence_perceived_latency_histogram_name,
        autofill_metrics::PreflightCallEvent::
            kCardChosenBeforePreflightCallReturned,
        1);
    histogram_tester.ExpectTotalCount(perceived_latency_duration_histogram_name,
                                      1);
    histogram_tester.ExpectBucketCount(timeout_cvc_fallback_histogram_name,
                                       false, 1);
  }

  // Preflight call timed out and CVC fallback was invoked.
  {
    base::HistogramTester histogram_tester;

    ResetFetchCreditCard();
    credit_card_access_manager().PrepareToFetchCreditCard();
    credit_card_access_manager().FetchCreditCard(
        server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                    accessor_->GetWeakPtr()));
    task_environment_.FastForwardBy(base::Seconds(4));
    WaitForCallbacks();

    histogram_tester.ExpectUniqueSample(
        existence_perceived_latency_histogram_name,
        autofill_metrics::PreflightCallEvent::
            kCardChosenBeforePreflightCallReturned,
        1);
    histogram_tester.ExpectTotalCount(perceived_latency_duration_histogram_name,
                                      1);
    histogram_tester.ExpectBucketCount(timeout_cvc_fallback_histogram_name,
                                       true, 1);
  }
}

// Ensures that use of a server card that is not enrolled into FIDO invokes
// authorization flow when user is opted-in.
TEST_F(CreditCardAccessManagerTest, FIDONewCardAuthorization) {
  base::HistogramTester histogram_tester;
  std::string unmask_decision_histogram_name =
      "Autofill.BetterAuth.CardUnmaskTypeDecision";
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.AuthenticationAfterCVC";
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.CvcThenFido";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  OptUserInToFido();

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();
  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Do not return any RequestOptions or CreationOptions in GetRealPan.
  // RequestOptions should have been returned in unmask details response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock user response.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::FOLLOWUP_AFTER_CVC_AUTH_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that the form is filled after user verification (OnCreditCardFetched
  // is called).
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Mock OptChange payments call.
  OptChange(PaymentsRpcResult::kSuccess, true);

  histogram_tester.ExpectUniqueSample(
      unmask_decision_histogram_name,
      autofill_metrics::CardUnmaskTypeDecisionMetric::kCvcThenFido, 1);
  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that the use of a server card that is not enrolled into FIDO fills
// the form if the user is opted-in to FIDO but no request options are present.
TEST_F(CreditCardAccessManagerTest,
       FIDONewCardAuthorization_NoRequestOptions_FormFilled) {
  CreditCard* card = CreateServerCard(kTestGUID, kTestNumber);
  OptUserInToFido();

  // Clear the FIDO request options that were set.
  payments_network_interface().unmask_details()->fido_request_options.clear();

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Do not return any RequestOptions or CreationOptions in GetRealPan.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that the form is filled as there are no FIDO request options
  // present.
  EXPECT_EQ(accessor_->number(), kTestNumber16);
  EXPECT_EQ(accessor_->cvc(), kTestCvc16);
}

// Ensures that use of a server card that is not enrolled into FIDO fills the
// form if the user is opted-in to FIDO but the request options are invalid.
TEST_F(CreditCardAccessManagerTest,
       FIDONewCardAuthorization_InvalidRequestOptions_FormFilled) {
  CreditCard* card = CreateServerCard(kTestGUID, kTestNumber);
  OptUserInToFido();

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);

  // Set invalid FIDO request options.
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      /*credential_id=*/"", /*relying_party_id=*/"");

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Do not return any RequestOptions in GetRealPan. RequestOptions should have
  // been returned in unmask details response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that the form is filled as the only FIDO request options present are
  // invalid.
  EXPECT_EQ(accessor_->number(), kTestNumber16);
  EXPECT_EQ(accessor_->cvc(), kTestCvc16);
}

// Ensures expired cards always invoke a CVC prompt instead of WebAuthn.
TEST_F(CreditCardAccessManagerTest, FetchExpiredServerCardInvokesCvcPrompt) {
  // Creating an expired server card and opting the user in with authorized
  // card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  card->SetExpirationYearFromString(u"2010");
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
}

#if BUILDFLAG(IS_ANDROID)
// Ensures that the WebAuthn verification prompt is invoked after user opts in
// on unmask card checkbox.
TEST_F(CreditCardAccessManagerTest, FIDOOptInSuccess_Android) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set `test_fido_request_options_type` to valid to mock user
  // checking the opt-in checkbox and ensuring GetRealPan returns
  // RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kValid));
  WaitForCallbacks();

  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock user response.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that the form is filled after user verification (OnCreditCardFetched
  // is called).
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Mock OptChange payments call.
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);

  EXPECT_EQ(kGooglePaymentsRpid, GetFIDOAuthenticator()->GetRelyingPartyId());
  EXPECT_EQ(kTestChallenge,
            BytesToBase64(GetFIDOAuthenticator()->GetChallenge()));
  EXPECT_TRUE(GetFIDOAuthenticator()->IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kSuccess, 1);
}

// Ensures that the card is filled into the form if the request options returned
// are invalid when the user opts in through the checkbox.
TEST_F(CreditCardAccessManagerTest,
       FIDOOptInFailure_InvalidResponseRequestOptions) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Set the test request options returned to invalid to mock the user checking
  // the checkbox, but invalid request options are returned from the server.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kInvalid));
  WaitForCallbacks();

  // Ensure that the form is filled because the request options returned from
  // the response were invalid.
  EXPECT_EQ(accessor_->number(), kTestNumber16);
  EXPECT_EQ(accessor_->cvc(), kTestCvc16);
}

// Ensures that the failed user verification disallows enrollment.
TEST_F(CreditCardAccessManagerTest, FIDOOptInUserVerificationFailure) {
  base::HistogramTester histogram_tester;
  std::string histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set `test_fido_request_options_type` to valid to mock user
  // checking the opt-in checkbox and ensuring GetRealPan returns
  // RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kValid));
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock GetAssertion failure.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/false);
  // Ensure that form is still filled even if user verification fails
  // (OnCreditCardFetched is called). Note that this is different behavior than
  // registering a new card.
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      histogram_name, autofill_metrics::WebauthnResultMetric::kNotAllowedError,
      1);
}

// Ensures that enrollment does not happen if the server returns a failure.
TEST_F(CreditCardAccessManagerTest, FIDOOptInServerFailure) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set `test_fido_request_options_type` to valid to mock user
  // checking the opt-in checkbox and ensuring GetRealPan returns
  // RequestOptions.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kValid));
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // called and correct flow is set.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock user response and OptChange payments call.
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that the form is filled after user verification (OnCreditCardFetched
  // is called).
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
  OptChange(PaymentsRpcResult::kPermanentFailure, false);

  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());
}

// Ensures that enrollment does not happen if user unchecking the opt-in
// checkbox.
TEST_F(CreditCardAccessManagerTest, FIDOOptIn_CheckboxDeclined) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // For Android, set `test_fido_request_options_type` to not present to mock
  // user unchecking the opt-in checkbox resulting in GetRealPan not returning
  // request options.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that form is filled (OnCreditCardFetched is called).
  EXPECT_EQ(kTestNumber16, accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());
  // Check current flow to ensure CreditCardFidoAuthenticator::Authorize is
  // never called.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::NONE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  EXPECT_FALSE(GetFIDOAuthenticator()->IsUserOptedIn());
}

// Ensures that opting-in through settings page on Android successfully sends an
// opt-in request the next time the user downstreams a card.
TEST_F(CreditCardAccessManagerTest, FIDOSettingsPageOptInSuccess_Android) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);

  // Setting the local opt-in state as true and implying that Payments servers
  // has the opt-in state to false - this shows the user opted-in through the
  // settings page.
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AllowFidoRegistration(true);
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  MockUserResponseForCvcAuth(kTestCvc16, /*enable_fido=*/false);

  // Although the checkbox was hidden and |enable_fido_auth| was set to false in
  // the user request, because of the previous opt-in intention, the client must
  // request to opt-in.
  EXPECT_TRUE(payments_network_interface()
                  .unmask_request()
                  ->user_response.enable_fido_auth);
}

#else   // BUILDFLAG(IS_ANDROID)
// Ensures that the WebAuthn enrollment prompt is invoked after user opts in. In
// this case, the user is not yet enrolled server-side, and thus receives
// |creation_options|.
TEST_F(CreditCardAccessManagerTest,
       FIDOEnrollmentSuccess_CreationOptions_Desktop) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false,
            /*include_creation_options=*/true);

  // Mock user response and OptChange payments call.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/true);
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);

  EXPECT_EQ(kGooglePaymentsRpid, GetFIDOAuthenticator()->GetRelyingPartyId());
  EXPECT_EQ(kTestChallenge,
            BytesToBase64(GetFIDOAuthenticator()->GetChallenge()));
  EXPECT_TRUE(GetFIDOAuthenticator()->IsUserOptedIn());
  EXPECT_EQ(0, GetStrikes());
  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(opt_in_histogram_name, 2);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kWithCreationChallenge, 1);
  histogram_tester.ExpectTotalCount(promo_shown_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(
      promo_user_decision_histogram_name,
      autofill_metrics::WebauthnOptInPromoUserDecisionMetric::kAccepted, 1);
}

// Ensures that the correct number of strikes are added when the user declines
// the WebAuthn offer.
TEST_F(CreditCardAccessManagerTest, FIDOEnrollment_OfferDeclined_Desktop) {
  base::HistogramTester histogram_tester;
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  AcceptWebauthnOfferDialog(/*did_accept=*/false);
  EXPECT_EQ(
      FidoAuthenticationStrikeDatabase::kStrikesToAddWhenOptInOfferDeclined,
      GetStrikes());
  histogram_tester.ExpectTotalCount(promo_shown_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(
      promo_user_decision_histogram_name,
      autofill_metrics::WebauthnOptInPromoUserDecisionMetric::
          kDeclinedImmediately,
      1);
}

// Ensures that the correct number of strikes are added when the user declines
// the WebAuthn offer.
TEST_F(CreditCardAccessManagerTest,
       FIDOEnrollment_OfferDeclinedAfterAccepting_Desktop) {
  base::HistogramTester histogram_tester;
  std::string promo_shown_histogram_name =
      "Autofill.BetterAuth.OptInPromoShown.FromCheckoutFlow";
  std::string promo_user_decision_histogram_name =
      "Autofill.BetterAuth.OptInPromoUserDecision.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  AcceptWebauthnOfferDialog(/*did_accept=*/true);
  AcceptWebauthnOfferDialog(/*did_accept=*/false);
  EXPECT_EQ(
      FidoAuthenticationStrikeDatabase::kStrikesToAddWhenOptInOfferDeclined,
      GetStrikes());
  histogram_tester.ExpectTotalCount(promo_shown_histogram_name, 1);
  histogram_tester.ExpectUniqueSample(
      promo_user_decision_histogram_name,
      autofill_metrics::WebauthnOptInPromoUserDecisionMetric::
          kDeclinedAfterAccepting,
      1);
}

// Ensures that the correct number of strikes are added when the user fails to
// complete user-verification for an opt-in attempt.
TEST_F(CreditCardAccessManagerTest,
       FIDOEnrollment_UserVerificationFailed_Desktop) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  ClearStrikes();
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  InvokeUnmaskDetailsTimeout();
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  WaitForCallbacks();
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false,
            /*include_creation_options=*/true);

  // Mock user response.
  TestCreditCardFidoAuthenticator::MakeCredential(GetFIDOAuthenticator(),
                                                  /*did_succeed=*/false);
  EXPECT_EQ(FidoAuthenticationStrikeDatabase::
                kStrikesToAddWhenUserVerificationFailsOnOptInAttempt,
            GetStrikes());
  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kNotAllowedError, 1);
  histogram_tester.ExpectUniqueSample(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
}

// Ensures that the WebAuthn enrollment prompt is invoked after user opts in. In
// this case, the user is already enrolled server-side, and thus receives
// |request_options|.
TEST_F(CreditCardAccessManagerTest,
       FIDOEnrollmentSuccess_RequestOptions_Desktop) {
  base::HistogramTester histogram_tester;
  std::string webauthn_result_histogram_name =
      "Autofill.BetterAuth.WebauthnResult.CheckoutOptIn";
  std::string opt_in_histogram_name =
      "Autofill.BetterAuth.OptInCalled.FromCheckoutFlow";

  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(false);
  payments_network_interface().AllowFidoRegistration(true);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  WaitForCallbacks();

  // Mock user and payments response.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  WaitForCallbacks();
  AcceptWebauthnOfferDialog(/*did_accept=*/true);

  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false,
            /*include_creation_options=*/false,
            /*include_request_options=*/true);

  // Mock user response and OptChange payments call.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::OPT_IN_WITH_CHALLENGE_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/true);

  EXPECT_EQ(kGooglePaymentsRpid, GetFIDOAuthenticator()->GetRelyingPartyId());
  EXPECT_EQ(kTestChallenge,
            BytesToBase64(GetFIDOAuthenticator()->GetChallenge()));
  EXPECT_TRUE(GetFIDOAuthenticator()->IsUserOptedIn());

  histogram_tester.ExpectUniqueSample(
      webauthn_result_histogram_name,
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectTotalCount(opt_in_histogram_name, 2);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kFetchingChallenge, 1);
  histogram_tester.ExpectBucketCount(
      opt_in_histogram_name,
      autofill_metrics::WebauthnOptInParameters::kWithRequestChallenge, 1);
}

TEST_F(CreditCardAccessManagerTest, SettingsPage_OptOut) {
  base::HistogramTester histogram_tester;
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);

  EXPECT_TRUE(IsCreditCardFIDOAuthEnabled());
  credit_card_access_manager().OnSettingsPageFIDOAuthToggled(false);
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);

  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}
#endif  // BUILDFLAG(IS_ANDROID)

// Params of the CreditCardAccessManagerBetterAuthOptInLogTest:
// -- bool is_virtual_card;
// -- bool unmask_details_offer_fido_opt_in;
// -- bool card_authorization_token_present;
// -- bool max_strikes_limit_reached;
// -- bool has_opted_in_from_android_settings;
// -- bool is_opted_in_for_fido;
class CreditCardAccessManagerBetterAuthOptInLogTest
    : public CreditCardAccessManagerTest,
      public testing::WithParamInterface<
          std::tuple<bool, bool, bool, bool, bool, bool>> {
 public:
  CreditCardAccessManagerBetterAuthOptInLogTest() = default;
  ~CreditCardAccessManagerBetterAuthOptInLogTest() override = default;

  void SetUp() override {
    CreditCardAccessManagerTest::SetUp();

    if (MaxStrikesLimitReached()) {
      AddMaxStrikes();
    } else {
      ClearStrikes();
    }

    CreateServerCard(kTestGUID, kTestNumber);
    GetFIDOAuthenticator()->SetUserVerifiable(true);
#if BUILDFLAG(IS_ANDROID)
    SetCreditCardFIDOAuthEnabled(HasOptedInFromAndroidSettings());
#else
    SetCreditCardFIDOAuthEnabled(false);
#endif  // BUILDFLAG(OS_ANDROID)
    payments_network_interface().AllowFidoRegistration(
        /*offer_fido_opt_in=*/UnmaskDetailsOfferFidoOptIn());
    if (IsVirtualCard()) {
      GetCreditCard()->set_record_type(CreditCard::RecordType::kVirtualCard);
    }
    if (IsOptedIntoFido()) {
      // If user and device are already opted into FIDO, then add an eligible
      // card to ensure that the `unmask_details_` contains fido request
      // options.
      payments_network_interface().AddFidoEligibleCard(
          "random_id", kCredentialId, kGooglePaymentsRpid);
    }

    credit_card_access_manager().PrepareToFetchCreditCard();
    credit_card_access_manager().FetchCreditCard(
        GetCreditCard(), base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                        accessor_->GetWeakPtr()));
  }

  bool IsVirtualCard() { return std::get<0>(GetParam()); }
  bool UnmaskDetailsOfferFidoOptIn() { return std::get<1>(GetParam()); }
  bool CardAuthorizationTokenPresent() { return std::get<2>(GetParam()); }
  bool MaxStrikesLimitReached() { return std::get<3>(GetParam()); }
  bool HasOptedInFromAndroidSettings() { return std::get<4>(GetParam()); }
  bool IsOptedIntoFido() { return std::get<5>(GetParam()); }

  bool ShouldOfferFidoOptIn() {
    return !IsOptedIntoFido() && !IsVirtualCard() &&
           UnmaskDetailsOfferFidoOptIn() && CardAuthorizationTokenPresent() &&
           !MaxStrikesLimitReached();
  }

  bool ShouldOfferFidoOptInAndroid() {
    return !IsOptedIntoFido() && !IsVirtualCard() &&
           UnmaskDetailsOfferFidoOptIn() && !HasOptedInFromAndroidSettings();
  }

  const std::string GetFidoOptInNotOfferedHistogram() {
    return fido_opt_in_not_offered_histogram;
  }

  CreditCard* GetCreditCard() {
    return personal_data().payments_data_manager().GetCreditCardByGUID(
        kTestGUID);
  }

 private:
  const std::string fido_opt_in_not_offered_histogram =
      "Autofill.BetterAuth.OptInPromoNotOfferedReason";
};

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
// Ensures that the correct metrics are logged when the FIDO opt-in dialog is
// not shown on Desktop.
TEST_P(CreditCardAccessManagerBetterAuthOptInLogTest,
       FidoOptInNotShown_Desktop) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(test_api(credit_card_access_manager())
                .ShouldOfferFidoOptInDialog(
                    CreditCardCvcAuthenticator::CvcAuthenticationResponse()
                        .with_did_succeed(true)
                        .with_card(GetCreditCard())
                        .with_card_authorization_token(
                            CardAuthorizationTokenPresent()
                                ? "card_authorization_token"
                                : "")
                        .with_cvc(u"123")),
            ShouldOfferFidoOptIn());

  if (IsOptedIntoFido()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kAlreadyOptedIn,
        /*expected_bucket_count=*/1);
  } else if (!UnmaskDetailsOfferFidoOptIn()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kUnmaskDetailsOfferFidoOptInFalse,
        /*expected_bucket_count=*/1);
  } else if (!CardAuthorizationTokenPresent()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kCardAuthorizationTokenEmpty,
        /*expected_bucket_count=*/1);
  } else if (MaxStrikesLimitReached()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kBlockedByStrikeDatabase,
        /*expected_bucket_count=*/1);
  } else if (IsVirtualCard()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kVirtualCard,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(GetFidoOptInNotOfferedHistogram(),
                                      /*expected_count=*/0);
  }
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
// Ensures that the correct metrics are logged when the FIDO opt-in checkbox is
// not shown on Android.
TEST_P(CreditCardAccessManagerBetterAuthOptInLogTest,
       FidoOptInNotShown_Android) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ(test_api(credit_card_access_manager()).ShouldOfferFidoAuth(),
            ShouldOfferFidoOptInAndroid());

  if (IsOptedIntoFido()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kAlreadyOptedIn,
        /*expected_bucket_count=*/1);
  } else if (!UnmaskDetailsOfferFidoOptIn()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kUnmaskDetailsOfferFidoOptInFalse,
        /*expected_bucket_count=*/1);
  } else if (HasOptedInFromAndroidSettings()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::
            kOptedInFromSettings,
        /*expected_bucket_count=*/1);
  } else if (IsVirtualCard()) {
    histogram_tester.ExpectUniqueSample(
        GetFidoOptInNotOfferedHistogram(),
        /*sample=*/
        autofill_metrics::WebauthnOptInPromoNotOfferedReason::kVirtualCard,
        /*expected_bucket_count=*/1);
  } else {
    histogram_tester.ExpectTotalCount(GetFidoOptInNotOfferedHistogram(),
                                      /*expected_count=*/0);
  }
}
#endif  // BUILDFLAG(IS_ANDROID)
INSTANTIATE_TEST_SUITE_P(,
                         CreditCardAccessManagerBetterAuthOptInLogTest,
                         testing::Combine(testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool(),
                                          testing::Bool()));

// Ensure that when unmask detail response is delayed, we will automatically
// fall back to CVC even if local pref and Payments mismatch.
TEST_F(CreditCardAccessManagerTest,
       IntentToOptOut_DelayedUnmaskDetailsResponse) {
  base::HistogramTester histogram_tester;
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);
  // Delay the UnmaskDetailsResponse so that we can't discover the mismatch,
  // which will use local pref and fall back to CVC.
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  // Ensure the auth flow type is CVC because no unmask detail response is
  // returned and local pref denotes that user is opted out.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);
  // Also ensure that since local pref is disabled, we will directly fall back
  // to CVC instead of falling back after time out. Ensure that
  // CardChosenBeforePreflightCallReturned is logged to opted-out histogram.
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedOut",
      autofill_metrics::PreflightCallEvent::
          kCardChosenBeforePreflightCallReturned,
      1);
  // No bucket count for OptIn TimedOutCvcFallback.
  histogram_tester.ExpectTotalCount(
      "Autofill.BetterAuth.UserPerceivedLatencyOnCardSelection.OptedIn."
      "TimedOutCvcFallback",
      0);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  // Since no unmask detail returned, we can't discover the pref mismatch, we
  // won't call opt out and local pref is unchanged.
  EXPECT_FALSE(GetFIDOAuthenticator()->IsOptOutCalled());
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_F(CreditCardAccessManagerTest, IntentToOptOut_OptOutAfterUnmaskSucceeds) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Also ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth success.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  WaitForCallbacks();

  // Ensure calling opt out after a successful cvc auth.
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  // Mock opt out success response. Local pref is consistent with payments.
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_F(CreditCardAccessManagerTest, IntentToOptOut_OptOutAfterUnmaskFails) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth failure.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kPermanentFailure,
                                   std::string()));
  WaitForCallbacks();

  // Ensure calling opt out after cvc auth failure.
  EXPECT_TRUE(GetFIDOAuthenticator()->IsOptOutCalled());
  // Mock opt out success. Local pref is consistent with payments.
  OptChange(PaymentsRpcResult::kSuccess,
            /*user_is_opted_in=*/false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

TEST_F(CreditCardAccessManagerTest, IntentToOptOut_OptOutFailure) {
  // Setting up a FIDO-enabled user with a server card.
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  // The user is FIDO-enabled from Payments.
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  SetCreditCardFIDOAuthEnabled(true);
  payments_network_interface().AddFidoEligibleCard(
      card->server_id(), kCredentialId, kGooglePaymentsRpid);
  // Mock the user manually opt-out from Settings page, and Payments did not
  // update user status in time. The mismatch will set user INTENT_TO_OPT_OUT.
  SetCreditCardFIDOAuthEnabled(/*enabled=*/false);

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));

  // Ensure that the local pref is still unchanged after unmask detail returns.
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
  // Also ensure the auth flow type is CVC because the local pref and payments
  // mismatch indicates that user intended to opt out.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kCvc);

  // Mock cvc auth success.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  WaitForCallbacks();

  // Mock payments opt out failure. Local pref should be unchanged.
  OptChange(PaymentsRpcResult::kPermanentFailure, false);
  EXPECT_FALSE(IsCreditCardFIDOAuthEnabled());
}

// TODO(crbug.com/40253866): Extend the FIDOAuthOptChange tests to more
// use-cases.
TEST_F(CreditCardAccessManagerTest, FIDOAuthOptChange_OptOut) {
  credit_card_access_manager().FIDOAuthOptChange(/*opt_in=*/false);
  ASSERT_TRUE(fido_authenticator().IsOptOutCalled());
}

TEST_F(CreditCardAccessManagerTest, FIDOAuthOptChange_OptOut_OffTheRecord) {
  autofill_client_.set_is_off_the_record(true);
  credit_card_access_manager().FIDOAuthOptChange(/*opt_in=*/false);
  ASSERT_FALSE(fido_authenticator().IsOptOutCalled());
}

// TODO(crbug.com/40707930) Debug issues and re-enable this test on MacOS.
#if !BUILDFLAG(IS_APPLE)
// Ensures that PrepareToFetchCreditCard() is properly rate limited.
TEST_F(CreditCardAccessManagerTest, PreflightCallRateLimited) {
  // Create server card and set user as eligible for FIDO auth.
  base::HistogramTester histogram_tester;
  std::string preflight_call_metric =
      "Autofill.BetterAuth.CardUnmaskPreflightCalledWithFidoOptInStatus";

  ClearCards();
  CreateServerCard(kTestGUID, kTestNumber);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  ResetFetchCreditCard();

  // First call to PrepareToFetchCreditCard() should make RPC.
  credit_card_access_manager().PrepareToFetchCreditCard();
  histogram_tester.ExpectTotalCount(preflight_call_metric, 1);

  // Calling PrepareToFetchCreditCard() without a prior preflight call should
  // have set |can_fetch_unmask_details_| to false to prevent further ones.
  EXPECT_FALSE(
      test_api(credit_card_access_manager()).can_fetch_unmask_details());

  // Any subsequent calls should not make a RPC.
  credit_card_access_manager().PrepareToFetchCreditCard();
  histogram_tester.ExpectTotalCount(preflight_call_metric, 1);
}
#endif  // !BUILDFLAG(IS_APPLE)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

// Ensures that UnmaskAuthFlowEvents also log to a ".ServerCard" subhistogram
// when a masked server card is selected.
TEST_F(CreditCardAccessManagerTest,
       UnmaskAuthFlowEvent_AlsoLogsServerCardSubhistogram) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  base::HistogramTester histogram_tester;
  std::string flow_events_histogram_name =
      "Autofill.BetterAuth.FlowEvents.Cvc.ServerCard";

  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  histogram_tester.ExpectUniqueSample(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);

  histogram_tester.ExpectBucketCount(
      flow_events_histogram_name,
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that |is_authentication_in_progress_| is set correctly.
TEST_F(CreditCardAccessManagerTest, AuthenticationInProgress) {
  CreateServerCard(kTestGUID, kTestNumber);
  CreditCard* card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  EXPECT_FALSE(IsAuthenticationInProgress());

  credit_card_access_manager().FetchCreditCard(
      card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                           accessor_->GetWeakPtr()));
  EXPECT_TRUE(IsAuthenticationInProgress());

  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, kTestNumber));
  EXPECT_FALSE(IsAuthenticationInProgress());
}

// Ensures that the use of |unmasked_card_cache_| is set and logged correctly.
TEST_F(CreditCardAccessManagerTest, FetchCreditCardUsesUnmaskedCardCache) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false);
  CreditCard* unmasked_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  credit_card_access_manager().CacheUnmaskedCardInfo(*unmasked_card,
                                                     kTestCvc16);

  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true);
  CreditCard* masked_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().FetchCreditCard(
      masked_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                  accessor_->GetWeakPtr()));
  histogram_tester.ExpectBucketCount("Autofill.UsedCachedServerCard", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kLocalCacheHit, 1);

  credit_card_access_manager().FetchCreditCard(
      masked_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                  accessor_->GetWeakPtr()));
  histogram_tester.ExpectBucketCount("Autofill.UsedCachedServerCard", 2, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kLocalCacheHit, 2);

  // Create a virtual card.
  CreditCard virtual_card = CreditCard();
  test::SetCreditCardInfo(&virtual_card, "Elvis Presley", kTestNumber,
                          test::NextMonth().c_str(), test::NextYear().c_str(),
                          "1");
  virtual_card.set_record_type(CreditCard::RecordType::kVirtualCard);
  credit_card_access_manager().CacheUnmaskedCardInfo(virtual_card, kTestCvc16);

  // Mocks that user selects the virtual card option of the masked card.
  masked_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  credit_card_access_manager().FetchCreditCard(
      masked_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                  accessor_->GetWeakPtr()));

  histogram_tester.ExpectBucketCount("Autofill.UsedCachedVirtualCard", 1, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.UnspecifiedFlowType",
      autofill_metrics::ServerCardUnmaskResult::kLocalCacheHit, 1);
}

TEST_F(CreditCardAccessManagerTest, GetCachedUnmaskedCards) {
  // Assert that there are no cards cached initially.
  EXPECT_EQ(0U, credit_card_access_manager().GetCachedUnmaskedCards().size());

  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreateServerCard(kTestGUID2, kTestNumber2, /*masked=*/true, kTestServerId2);
  // Add a card to the cache.
  CreditCard* unmasked_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  credit_card_access_manager().CacheUnmaskedCardInfo(*unmasked_card,
                                                     kTestCvc16);

  // Verify that only the card added to the cache is returned.
  ASSERT_EQ(1U, credit_card_access_manager().GetCachedUnmaskedCards().size());
  EXPECT_EQ(*unmasked_card,
            credit_card_access_manager().GetCachedUnmaskedCards()[0]->card);
}

TEST_F(CreditCardAccessManagerTest, IsCardPresentInUnmaskedCache) {
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreateServerCard(kTestGUID2, kTestNumber2, /*masked=*/true, kTestServerId2);
  // Add a card to the cache.
  CreditCard* unmasked_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  credit_card_access_manager().CacheUnmaskedCardInfo(*unmasked_card,
                                                     kTestCvc16);

  // Verify that only one card is present in the cache.
  EXPECT_TRUE(credit_card_access_manager().IsCardPresentInUnmaskedCache(
      *unmasked_card));
  EXPECT_FALSE(credit_card_access_manager().IsCardPresentInUnmaskedCache(
      *personal_data().payments_data_manager().GetCreditCardByGUID(
          kTestGUID2)));
}

class CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest
    : public CreditCardAccessManagerTest {
 public:
  CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest() = default;
  ~CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest() override =
      default;

  base::test::ScopedFeatureList feature_list_{
      features::kAutofillEnableFpanRiskBasedAuthentication};

  void MockRiskBasedAuthSucceedsWithoutPanReturned(
      CreditCard* card,
      std::string context_token = "fake_context_token") {
    credit_card_access_manager().FetchCreditCard(
        card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                             accessor_->GetWeakPtr()));

    // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
    // invoked.
    EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                    ->risk_based_authentication_invoked());
    EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                    ->autofill_progress_dialog_shown());

    // Mock that
    // CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
    // indicates a yellow path with context token returned.
    credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
        CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
            .with_result(CreditCardRiskBasedAuthenticator::
                             RiskBasedAuthenticationResponse::Result::
                                 kAuthenticationRequired)
            .with_context_token(context_token));
  }
};

// Test the flow when the masked server card is successfully returned from
// the server during a risk-based retrieval.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_Success) {
#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, /*masked=*/true, kTestServerId);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  CreditCard card = *masked_server_card;
  card.set_record_type(CreditCard::RecordType::kFullServerCard);
  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(card));

  // Ensure the accessor received the correct response.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(accessor_->number(), base::UTF8ToUTF16(test_number));

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed();
  EXPECT_THAT(type, testing::Optional(
                        NonInteractivePaymentMethodType::kMaskedServerCard));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kRiskBasedUnmasked, 1);
}

// Ensures that the masked server card risk-based unmasking response is
// handled correctly if the retrieval failed.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_RetrievalError) {
  base::HistogramTester histogram_tester;
  CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true, kTestServerId);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a red path.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::kError));

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kUnexpectedError, 1);
}

// Ensures that the masked server card risk-based unmasking response is
// handled correctly if the flow is cancelled.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_FlowCancelled) {
  base::HistogramTester histogram_tester;
  CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true, kTestServerId);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock the authentication is cancelled.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationCancelled));

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.ServerCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
}

// Ensures that the masked server card risk-based authentication is not invoked
// when the feature is disabled.
TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    RiskBasedMaskedServerCardUnmasking_RiskBasedAuthenticationNotInvoked_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(
      features::kAutofillEnableFpanRiskBasedAuthentication);
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is not invoked.
  ASSERT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->risk_based_authentication_invoked());
}

TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    RiskBasedMaskedServerCardUnmasking_CvcAuthenticationRequired_ContextTokenSetCorrectly) {
  std::string context_token = "context_token";
  CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true, kTestServerId);

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card,
                                              context_token);

  // Expect the context_token is set in the full card request.
  EXPECT_EQ(GetCvcAuthenticator()
                .GetFullCardRequest()
                ->GetUnmaskRequestDetailsForTesting()
                ->context_token,
            context_token);
}

// Ensures the authentication is delegated to the CVC authenticator when
// `fido_request_options` is not returned.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_AuthenticationRequired_CvcOnly) {
  base::HistogramTester histogram_tester;

  std::string test_number = "4444333322221111";
  CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, /*masked=*/true, kTestServerId);

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Cvc",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, test_number));
  // Ensure that the form is filled.
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Ensures the masked server card risk-based unmasking response is handled
// correctly and authentication is delegated to the FIDO authenticator, when
// `fido_request_options` is returned.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_AuthenticationRequired_FidoOnly) {
  base::HistogramTester histogram_tester;

  std::string test_number = "4444333322221111";
  CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, /*masked=*/true, kTestServerId);
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
  fido_authenticator().set_is_user_opted_in(true);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is successfully
  // invoked.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_progress_dialog_shown());

  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a yellow path when `fido_request_options` and `context_token` are
  // returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kAuthenticationRequired)
          .with_fido_request_options(GetTestRequestOptions())
          .with_context_token("fake_context_token"));

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Fido",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect the CreditCardAccessManager invokes the FIDO authenticator.
  ASSERT_TRUE(fido_authenticator().authenticate_invoked());
  EXPECT_EQ(fido_authenticator().card().number(),
            base::UTF8ToUTF16(test_number));
  EXPECT_EQ(fido_authenticator().card().record_type(),
            CreditCard::RecordType::kMaskedServerCard);
  ASSERT_TRUE(fido_authenticator().context_token().has_value());
  EXPECT_EQ(fido_authenticator().context_token().value(), "fake_context_token");

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.CardUnmaskTypeDecision",
      autofill_metrics::CardUnmaskTypeDecisionMetric::kFidoOnly, 1);
}

// Ensures that use of new card invokes authorization flow when user is
// opted-in to FIDO.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       RiskBasedMaskedServerCardUnmasking_AuthenticationRequired_CvcThenFido) {
  base::HistogramTester histogram_tester;

  OptUserInToFido();
  std::string test_number = "4444333322221111";
  CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, /*masked=*/true, kTestServerId);

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(true);
  payments_network_interface().SetFidoRequestOptionsInUnmaskDetails(
      kCredentialId, kGooglePaymentsRpid);
  credit_card_access_manager().PrepareToFetchCreditCard();
  WaitForCallbacks();

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.CvcThenFido",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, test_number,
                                   TestFidoRequestOptionsType::kNotPresent));
  // Ensure that the form is not filled yet (OnCreditCardFetched is not called).
  EXPECT_EQ(accessor_->number(), std::u16string());
  EXPECT_EQ(accessor_->cvc(), std::u16string());

  // Mock user response.
  EXPECT_EQ(CreditCardFidoAuthenticator::Flow::FOLLOWUP_AFTER_CVC_AUTH_FLOW,
            GetFIDOAuthenticator()->current_flow());
  TestCreditCardFidoAuthenticator::GetAssertion(GetFIDOAuthenticator(),
                                                /*did_succeed=*/true);
  // Ensure that the form is filled after user verification (OnCreditCardFetched
  // is called).
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Mock OptChange payments call.
  OptChange(PaymentsRpcResult::kSuccess, true);

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.CardUnmaskTypeDecision",
      autofill_metrics::CardUnmaskTypeDecisionMetric::kCvcThenFido, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.WebauthnResult.AuthenticationAfterCVC",
      autofill_metrics::WebauthnResultMetric::kSuccess, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.BetterAuth.FlowEvents.CvcThenFido",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptCompleted, 1);
}

// Ensures that the kCvc instead of kCvcThenFido flow is invoked if
// GetUnmaskDetails preflight call is not finished.
TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    RiskBasedMaskedServerCardUnmasking_AuthenticationRequired_PreflightCallNotFinished) {
  base::HistogramTester histogram_tester;

  OptUserInToFido();
  std::string test_number = "4444333322221111";
  CreditCard* masked_server_card =
      CreateServerCard(kTestGUID, test_number, /*masked=*/true, kTestServerId);

  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(false);
  credit_card_access_manager().PrepareToFetchCreditCard();

  MockRiskBasedAuthSucceedsWithoutPanReturned(masked_server_card);

  histogram_tester.ExpectUniqueSample(
      "Autofill.BetterAuth.FlowEvents.Cvc",
      CreditCardFormEventLogger::UnmaskAuthFlowEvent::kPromptShown, 1);

  // Expect CVC prompt to be invoked.
  EXPECT_TRUE(GetRealPanForCVCAuth(PaymentsRpcResult::kSuccess, test_number));
  // Ensure that the form is filled.
  EXPECT_EQ(base::UTF8ToUTF16(test_number), accessor_->number());
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

// Ensures that CVC filling gets logged after masked server card risk-based
// unmasking success if the card has CVC.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       LogCvcFilling_RiskBasedMaskedServerCardUnmaskingSuccess) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  masked_server_card->set_cvc(kTestCvc16);

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Mock CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse to
  // successfully return the valid card number.
  CreditCard card = *masked_server_card;
  card.set_record_type(CreditCard::RecordType::kFullServerCard);
  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(card));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kNoInteractiveAuthentication, 1);
}

// Ensures that CVC filling doesn't get logged after after masked server card
// risk-based unmasking success if the card doesn't have CVC.
TEST_F(CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
       DoNotLogCvcFilling_RiskBasedMaskedServerCardUnmaskingSuccess) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  masked_server_card->set_cvc(u"");

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Mock CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse to
  // successfully return the valid card number.
  CreditCard card = *masked_server_card;
  card.set_record_type(CreditCard::RecordType::kFullServerCard);
  // Mock that CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse
  // indicates a green path with valid card number returned.
  credit_card_access_manager().OnRiskBasedAuthenticationResponseReceived(
      CreditCardRiskBasedAuthenticator::RiskBasedAuthenticationResponse()
          .with_result(CreditCardRiskBasedAuthenticator::
                           RiskBasedAuthenticationResponse::Result::
                               kNoAuthenticationRequired)
          .with_card(card));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.CvcStorage.CvcFilling.ServerCard",
      autofill_metrics::CvcFillingFlowType::kNoInteractiveAuthentication, 0);
}

// Ensures that the masked server card risk-based authentication is not invoked
// when the card is expired.
TEST_F(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
    RiskBasedMaskedServerCardUnmasking_RiskBasedAuthenticationNotInvoked_CardExpired) {
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/true, kTestServerId);
  CreditCard* masked_server_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  masked_server_card->SetExpirationYearFromString(u"2010");

  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));

  // Ensures CreditCardRiskBasedAuthenticator::Authenticate is not invoked.
  ASSERT_FALSE(autofill_client_.GetPaymentsAutofillClient()
                   ->risk_based_authentication_invoked());
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Params of the
// CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest:
// -- bool fido_opted_in;
// -- bool preflight_call_returned;
class
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest
    : public CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest() =
      default;
  ~CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest()
      override = default;

  bool FidoOptedIn() { return std::get<0>(GetParam()); }
  bool PreflightCallReturned() { return std::get<1>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    ,
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest,
    testing::Combine(testing::Bool(), testing::Bool()));

// Ensures that the metric for if the preflight call's response is received
// before card selection is logged correctly.
TEST_P(
    CreditCardAccessManagerRiskBasedMaskedServerCardUnmaskingPreflightCallReturnedTest,
    Metrics_LogPreflightCallResponseReceivedOnCardSelection) {
  base::HistogramTester histogram_tester;
  std::string test_number = "4444333322221111";
  CreditCard* masked_server_card = CreateServerCard(kTestGUID, test_number);
  GetFIDOAuthenticator()->SetUserVerifiable(true);
  if (FidoOptedIn()) {
    OptUserInToFido();
  }
  payments_network_interface().ShouldReturnUnmaskDetailsImmediately(
      PreflightCallReturned());

  std::string histogram_name =
      "Autofill.BetterAuth.PreflightCallResponseReceivedOnCardSelection.";
  histogram_name += FidoOptedIn() ? "OptedIn." : "OptedOut.";
  histogram_name += "ServerCard";

  credit_card_access_manager().PrepareToFetchCreditCard();
  credit_card_access_manager().FetchCreditCard(
      masked_server_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                         accessor_->GetWeakPtr()));
  WaitForCallbacks();

  histogram_tester.ExpectUniqueSample(
      histogram_name,
      PreflightCallReturned() ? autofill_metrics::PreflightCallEvent::
                                    kPreflightCallReturnedBeforeCardChosen
                              : autofill_metrics::PreflightCallEvent::
                                    kCardChosenBeforePreflightCallReturned,
      1);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)

TEST_F(CreditCardAccessManagerTest, IsVirtualCardPresentInUnmaskedCache) {
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* unmasked_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  unmasked_card->set_record_type(CreditCard::RecordType::kVirtualCard);

  // Add the virtual card to the cache.
  credit_card_access_manager().CacheUnmaskedCardInfo(*unmasked_card,
                                                     kTestCvc16);

  // Verify that the virtual card is present in the cache.
  EXPECT_TRUE(credit_card_access_manager().IsCardPresentInUnmaskedCache(
      *unmasked_card));
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Success_VirtualCardsEnabled) {
  base::HistogramTester histogram_tester;

#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableVirtualCards};
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with valid card information.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.real_pan = "4111111111111111";
  response.dcvv = "321";
  response.expiration_month = test::NextMonth();
  response.expiration_year = test::NextYear();
  response.card_type = PaymentsRpcCardType::kVirtualCard;
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kSuccess, response);

  // Ensure the accessor received the correct response.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(accessor_->number(), u"4111111111111111");
  EXPECT_EQ(accessor_->cvc(), u"321");
  EXPECT_EQ(accessor_->expiry_month(), base::UTF8ToUTF16(test::NextMonth()));
  EXPECT_EQ(accessor_->expiry_year(), base::UTF8ToUTF16(test::NextYear()));

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed();
  EXPECT_THAT(type,
              testing::Optional(NonInteractivePaymentMethodType::kVirtualCard));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kRiskBasedUnmasked, 1);
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Success_VirtualCardsDisabled) {
  base::HistogramTester histogram_tester;

#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(
      features::kAutofillEnableVirtualCards);
#endif  // BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
  if (base::android::BuildInfo::GetInstance()->is_automotive()) {
    GTEST_SKIP() << "This test should not run on automotive.";
  }
#endif  // BUILDFLAG(IS_ANDROID)

  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with valid card information.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.real_pan = "4111111111111111";
  response.dcvv = "321";
  response.expiration_month = test::NextMonth();
  response.expiration_year = test::NextYear();
  response.card_type = PaymentsRpcCardType::kVirtualCard;
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kSuccess, response);

  // Ensure the accessor received the correct response.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(accessor_->number(), u"4111111111111111");
  EXPECT_EQ(accessor_->cvc(), u"321");
  EXPECT_EQ(accessor_->expiry_month(), base::UTF8ToUTF16(test::NextMonth()));
  EXPECT_EQ(accessor_->expiry_year(), base::UTF8ToUTF16(test::NextYear()));

  // There was no interactive authentication in this flow, so check that this
  // is signaled correctly.
  std::optional<NonInteractivePaymentMethodType> type =
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed();
  EXPECT_THAT(type,
              testing::Optional(NonInteractivePaymentMethodType::kVirtualCard));

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kRiskBasedUnmasked, 1);
}

// Ensures the virtual card risk-based unmasking response is handled correctly
// and authentication is delegated to the OTP authenticator, when only the OTP
// challenge option is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_OtpOnly) {
  base::HistogramTester histogram_tester;

#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableVirtualCards};
#endif  // BUILDFLAG(IS_IOS)

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kEmailOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0);

  CreditCard card = test::GetCreditCard();
  credit_card_access_manager().OnOtpAuthenticationComplete(
      CreditCardOtpAuthenticator::OtpAuthenticationResponse()
          .with_result(CreditCardOtpAuthenticator::OtpAuthenticationResponse::
                           Result::kSuccess)
          .with_card(&card)
          .with_cvc(kTestCvc16));

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  // Expect accessor to successfully retrieve the CVC.
  EXPECT_EQ(kTestCvc16, accessor_->cvc());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Otp",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures the virtual card risk-based unmasking response is handled correctly
// and authentication is delegated to the CVC authenticator, when only the CVC
// challenge option is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_CvcOnly) {
  base::HistogramTester histogram_tester;

#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableVirtualCards};
#endif  // BUILDFLAG(IS_IOS)

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kCvc});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0);

  CreditCard card = test::GetCreditCard();
  credit_card_access_manager().OnCvcAuthenticationComplete(
      CreditCardCvcAuthenticator::CvcAuthenticationResponse()
          .with_did_succeed(true)
          .with_card(&card)
          .with_cvc(u"123"));

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  // TODO(crbug.com/40240970): Add metrics checks for Virtual Card CVC auth
  // result.
}

// Ensures the virtual card risk-based unmasking flow type is set to
// kThreeDomainSecure when only the 3DS challenge option is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Only3dsChallengeReturned) {
#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableVirtualCards};
#endif  // BUILDFLAG(IS_IOS)

  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // Mock server response with information regarding VCN auth.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  response.card_unmask_challenge_options = test::GetCardUnmaskChallengeOptions(
      {CardUnmaskChallengeOptionType::kThreeDomainSecure});

  EXPECT_CALL(*static_cast<payments::MockPaymentsWindowManager*>(
                  autofill_client_.GetPaymentsAutofillClient()
                      ->GetPaymentsWindowManager()),
              InitVcn3dsAuthentication)
      .Times(1)
      .WillOnce([&](payments::PaymentsWindowManager::Vcn3dsContext context) {
        EXPECT_EQ(context.context_token, response.context_token);
        EXPECT_EQ(context.card, *virtual_card);
        EXPECT_EQ(context.challenge_option.type,
                  CardUnmaskChallengeOptionType::kThreeDomainSecure);
        EXPECT_FALSE(context.user_consent_already_given);
      });
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kSuccess, response);

  // If VCN 3DS is the only challenge option returned, verify that flow type is
  // kThreeDomainSecure.
  EXPECT_EQ(GetUnmaskAuthFlowType(), UnmaskAuthFlowType::kThreeDomainSecure);
}

// Ensures the virtual card risk-based unmasking response is handled correctly
// and authentication is delegated to the correct authenticator when multiple
// challenge options are returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_OtpAndCvcAnd3ds) {
  base::HistogramTester histogram_tester;

#if BUILDFLAG(IS_IOS)
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillEnableVirtualCards};
#endif  // BUILDFLAG(IS_IOS)

  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp,
           CardUnmaskChallengeOptionType::kCvc,
           CardUnmaskChallengeOptionType::kThreeDomainSecure});

  for (size_t selected_index = 0; selected_index < challenge_options.size();
       selected_index++) {
    MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
        /*fido_authenticator_is_user_opted_in=*/false,
        /*is_user_verifiable=*/false, challenge_options, selected_index);

    switch (challenge_options[selected_index].type) {
      case CardUnmaskChallengeOptionType::kSmsOtp: {
        CreditCard card = test::GetCreditCard();
        credit_card_access_manager().OnOtpAuthenticationComplete(
            CreditCardOtpAuthenticator::OtpAuthenticationResponse()
                .with_result(CreditCardOtpAuthenticator::
                                 OtpAuthenticationResponse::Result::kSuccess)
                .with_card(&card)
                .with_cvc(u"123"));
        break;
      }
      case CardUnmaskChallengeOptionType::kCvc: {
        CreditCard card = test::GetCreditCard();
        credit_card_access_manager().OnCvcAuthenticationComplete(
            CreditCardCvcAuthenticator::CvcAuthenticationResponse()
                .with_did_succeed(true)
                .with_card(&card)
                .with_cvc(u"123"));
        break;
      }
      case CardUnmaskChallengeOptionType::kThreeDomainSecure:
        // VCN 3DS is one of the challenge options returned in the challenge
        // selection dialog, and user selected the 3DS challenge option. Verify
        // that flow type is kThreeDomainSecureConsentAlreadyGiven.
        EXPECT_EQ(GetUnmaskAuthFlowType(),
                  UnmaskAuthFlowType::kThreeDomainSecureConsentAlreadyGiven);
        break;
      case CardUnmaskChallengeOptionType::kEmailOtp:
      case CardUnmaskChallengeOptionType::kUnknownType:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }

  // Expect that we did not signal that there was no interactive authentication.
  EXPECT_FALSE(
      test_api(*autofill_client_.GetFormDataImporter())
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 3);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Otp",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
  // TODO(crbug.com/40240970): Add metrics checks for Virtual Card CVC auth
  // result.
}

#if !BUILDFLAG(IS_IOS)
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_CreditCardAccessManagerReset_TriggersOtpAuthenticatorResetOnFlowCancelled) {
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/false, challenge_options, /*selected_index=*/0);

  // This check already happens in
  // MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(), but double
  // checking here helps show this test works correctly.
  EXPECT_TRUE(otp_authenticator_->on_challenge_option_selected_invoked());

  test_api(credit_card_access_manager()).OnVirtualCardUnmaskCancelled();
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
}

// Test that a success response for a VCN 3DS authentication is handled
// correctly and notifies the caller with the proper fields set.
TEST_F(CreditCardAccessManagerTest,
       VirtualCardUnmasking_3dsResponseReceived_Success) {
  // Set up the test.
  CreditCard card = test::GetVirtualCard();
  credit_card_access_manager().FetchCreditCard(
      &card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                            accessor_->GetWeakPtr()));
  payments::PaymentsWindowManager::Vcn3dsAuthenticationResponse response;
  response.card = card;

  // Mock the VCN 3DS authentication response.
  test_api(credit_card_access_manager())
      .OnVcn3dsAuthenticationComplete(response);

  // Check that `accessor_` was triggered with the expected values.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kSuccess);
  EXPECT_EQ(accessor_->number(), response.card->number());
  EXPECT_EQ(accessor_->cvc(), response.card->cvc());
}

// Test that a failure response for a VCN 3DS authentication is handled
// correctly and notifies the caller with the proper fields set.
TEST_F(CreditCardAccessManagerTest,
       VirtualCardUnmasking_3dsResponseReceived_Error) {
  // Set up the test.
  CreditCard card = test::GetVirtualCard();
  credit_card_access_manager().FetchCreditCard(
      &card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                            accessor_->GetWeakPtr()));
  payments::PaymentsWindowManager::Vcn3dsAuthenticationResponse response;

  // Mock the VCN 3DS authentication response.
  test_api(credit_card_access_manager())
      .OnVcn3dsAuthenticationComplete(response);

  // Check that `accessor_` was triggered with the expected error.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated to the FIDO authenticator, when
// only the FIDO challenge options is returned.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoOnly) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
  fido_authenticator().set_is_user_opted_in(true);

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with information regarding FIDO auth.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  response.fido_request_options = GetTestRequestOptions();
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kSuccess, response);

  // Expect the CreditCardAccessManager invokes the FIDO authenticator.
  ASSERT_TRUE(fido_authenticator().authenticate_invoked());
  EXPECT_EQ(fido_authenticator().card().number(),
            base::UTF8ToUTF16(std::string(kTestNumber)));
  EXPECT_EQ(fido_authenticator().card().record_type(),
            CreditCard::RecordType::kVirtualCard);
  ASSERT_TRUE(fido_authenticator().context_token().has_value());
  EXPECT_EQ(fido_authenticator().context_token().value(), "fake_context_token");

  // Mock FIDO authentication completed.
  CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response;
  fido_response.did_succeed = true;
  CreditCard card = test::WithCvc(test::GetVirtualCard(), u"234");
  fido_response.card = &card;
  test_api(credit_card_access_manager())
      .OnFIDOAuthenticationComplete(fido_response);

  // Expect accessor to successfully retrieve the virtual card CVC.
  EXPECT_EQ(u"234", accessor_->cvc());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Fido",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated to the FIDO authenticator, when
// both FIDO and OTP challenge options are returned.
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_PrefersFido) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
  fido_authenticator().set_is_user_opted_in(true);

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with information regarding both FIDO and OTP auth.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  CardUnmaskChallengeOption challenge_option =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp})[0];
  response.card_unmask_challenge_options.emplace_back(challenge_option);
  response.fido_request_options = GetTestRequestOptions();
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kSuccess, response);

  // Expect the CreditCardAccessManager invokes the FIDO authenticator.
  ASSERT_TRUE(fido_authenticator().authenticate_invoked());
  EXPECT_EQ(fido_authenticator().card().number(),
            base::UTF8ToUTF16(std::string(kTestNumber)));
  EXPECT_EQ(fido_authenticator().card().record_type(),
            CreditCard::RecordType::kVirtualCard);
  ASSERT_TRUE(fido_authenticator().context_token().has_value());
  EXPECT_EQ(fido_authenticator().context_token().value(), "fake_context_token");

  // Mock FIDO authentication completed.
  CreditCardFidoAuthenticator::FidoAuthenticationResponse fido_response;
  fido_response.did_succeed = true;
  CreditCard card = test::GetVirtualCard();
  fido_response.card = &card;
  fido_response.cvc = u"123";
  test_api(credit_card_access_manager())
      .OnFIDOAuthenticationComplete(fido_response);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Fido",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated to the OTP authenticator, when both
// FIDO and OTP challenge options are returned but FIDO local preference is not
// opted in.
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_FidoNotOptedIn) {
  base::HistogramTester histogram_tester;
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/false,
      /*is_user_verifiable=*/true, challenge_options, /*selected_index=*/0);

  CreditCardOtpAuthenticator::OtpAuthenticationResponse otp_response;
  otp_response.result =
      CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess;
  CreditCard card = test::GetCreditCard();
  otp_response.card = &card;
  otp_response.cvc = u"123";
  credit_card_access_manager().OnOtpAuthenticationComplete(otp_response);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Otp",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly and authentication is delegated first to the FIDO authenticator,
// when both FIDO and OTP challenge options are returned, but fall back to OTP
// authentication if FIDO failed.
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoAndOtp_FidoFailedFallBackToOtp) {
  base::HistogramTester histogram_tester;
  std::vector<CardUnmaskChallengeOption> challenge_options =
      test::GetCardUnmaskChallengeOptions(
          {CardUnmaskChallengeOptionType::kSmsOtp});
  MockCardUnmaskFlowUpToAuthenticationSelectionDialogAccepted(
      /*fido_authenticator_is_user_opted_in=*/true,
      /*is_user_verifiable=*/true, challenge_options, /*selected_index=*/0);

  CreditCardOtpAuthenticator::OtpAuthenticationResponse otp_response;
  otp_response.result =
      CreditCardOtpAuthenticator::OtpAuthenticationResponse::Result::kSuccess;
  CreditCard card = test::GetCreditCard();
  otp_response.card = &card;
  otp_response.cvc = u"123";
  credit_card_access_manager().OnOtpAuthenticationComplete(otp_response);

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.OtpFallbackFromFido",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationUnmasked, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly if there is only FIDO option returned by the server but the user
// is not opted in.
TEST_F(
    CreditCardAccessManagerTest,
    RiskBasedVirtualCardUnmasking_AuthenticationRequired_FidoOnly_FidoNotOptedIn) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
  fido_authenticator().set_is_user_opted_in(false);

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with information regarding FIDO auth.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  response.fido_request_options = GetTestRequestOptions();
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kSuccess, response);

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.Fido",
      autofill_metrics::ServerCardUnmaskResult::kOnlyFidoAvailableButNotOptedIn,
      1);
}

#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
#endif  // !BUILDFLAG(IS_IOS)

// Ensures that the virtual card risk-based unmasking response is handled
// correctly if there is no challenge option returned by the server.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Failure_NoOptionReturned) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // |is_user_verifiable_| related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator().set_is_user_opted_in(true);
#endif

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with no challenge options.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.context_token = "fake_context_token";
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kPermanentFailure, response);

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
#endif

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kAuthenticationError, 1);
}

// Ensures that the virtual card risk-based unmasking response is handled
// correctly if there is virtual card retrieval error.
TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Failure_VirtualCardRetrievalError) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator().set_is_user_opted_in(true);
#endif

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock server response with no challenge options.
  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kVcnRetrievalPermanentFailure, response);

  // Expect the CreditCardAccessManager to end the session.
  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
#endif

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError, 1);
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_Failure_MerchantOptedOut) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  AutofillErrorDialogContext autofill_error_dialog_context;
  autofill_error_dialog_context.server_returned_title =
      "test_server_returned_title";
  autofill_error_dialog_context.server_returned_description =
      "test_server_returned_description";

  payments::PaymentsNetworkInterface::UnmaskResponseDetails response;
  response.autofill_error_dialog_context = autofill_error_dialog_context;
  credit_card_access_manager()
      .OnVirtualCardRiskBasedAuthenticationResponseReceived(
          PaymentsRpcResult::kVcnRetrievalTryAgainFailure, response);

  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->autofill_error_dialog_shown());
  const AutofillErrorDialogContext& displayed_error_dialog_context =
      autofill_client_.GetPaymentsAutofillClient()
          ->autofill_error_dialog_context();
  EXPECT_EQ(*displayed_error_dialog_context.server_returned_title,
            *autofill_error_dialog_context.server_returned_title);
  EXPECT_EQ(*displayed_error_dialog_context.server_returned_description,
            *autofill_error_dialog_context.server_returned_description);

  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kVirtualCardRetrievalError, 1);
}

TEST_F(CreditCardAccessManagerTest,
       RiskBasedVirtualCardUnmasking_FlowCancelled) {
  base::HistogramTester histogram_tester;
  CreateServerCard(kTestGUID, kTestNumber, /*masked=*/false, kTestServerId);
  CreditCard* virtual_card =
      personal_data().payments_data_manager().GetCreditCardByGUID(kTestGUID);
  virtual_card->set_record_type(CreditCard::RecordType::kVirtualCard);
  // TODO(crbug.com/40197696): Switch to SetUserVerifiable after moving all
  // is_user_veriable_ related logic from CreditCardAccessManager to
  // CreditCardFidoAuthenticator.
  test_api(credit_card_access_manager()).set_is_user_verifiable(true);
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  fido_authenticator().set_is_user_opted_in(true);
#endif

  credit_card_access_manager().FetchCreditCard(
      virtual_card, base::BindOnce(&TestAccessor::OnCreditCardFetched,
                                   accessor_->GetWeakPtr()));

  // This checks risk-based authentication flow is successfully invoked,
  // because it is always the very first authentication flow in a VCN
  // unmasking flow.
  EXPECT_TRUE(autofill_client_.GetPaymentsAutofillClient()
                  ->risk_based_authentication_invoked());
  // Mock that the flow was cancelled by the user.
  test_api(credit_card_access_manager()).OnVirtualCardUnmaskCancelled();

  EXPECT_EQ(accessor_->result(), CreditCardFetchResult::kTransientError);
  EXPECT_FALSE(otp_authenticator_->on_challenge_option_selected_invoked());
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_ANDROID)
  EXPECT_FALSE(fido_authenticator().authenticate_invoked());
#endif

  // Expect the metrics are logged correctly.
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Attempt", true, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.ServerCardUnmask.VirtualCard.Result.RiskBased",
      autofill_metrics::ServerCardUnmaskResult::kFlowCancelled, 1);
}

// Test that the CreditCardAccessManager's destructor resets the record type of
// the card that had no interactive authentication flows completed in the
// associated FormDataImporter.
TEST_F(CreditCardAccessManagerTest, DestructorResetsCardIdentifier) {
  auto* form_data_importer = autofill_client_.GetFormDataImporter();
  form_data_importer
      ->SetPaymentMethodTypeIfNonInteractiveAuthenticationFlowCompleted(
          NonInteractivePaymentMethodType::kLocalCard);
  EXPECT_TRUE(
      test_api(*form_data_importer)
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
  autofill_driver_.reset();
  EXPECT_FALSE(
      test_api(*form_data_importer)
          .payment_method_type_if_non_interactive_authentication_flow_completed()
          .has_value());
}

}  // namespace autofill
