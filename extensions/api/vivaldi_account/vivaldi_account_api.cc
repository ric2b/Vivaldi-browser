// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/vivaldi_account/vivaldi_account_api.h"

#include "base/base64.h"
#include "base/lazy_instance.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "extensions/api/extension_action_utils/extension_action_utils_api.h"
#include "extensions/schema/vivaldi_account.h"
#include "extensions/tools/vivaldi_tools.h"
#include "prefs/vivaldi_pref_names.h"

namespace extensions {

namespace {
vivaldi::vivaldi_account::FetchErrorType ToVivaldiAccountAPIFetchErrorType(
    ::vivaldi::VivaldiAccountManager::FetchErrorType error) {
  switch (error) {
    case ::vivaldi::VivaldiAccountManager::NONE:
      return vivaldi::vivaldi_account::FetchErrorType::kNoError;
    case ::vivaldi::VivaldiAccountManager::NETWORK_ERROR:
      return vivaldi::vivaldi_account::FetchErrorType::kNetworkError;
    case ::vivaldi::VivaldiAccountManager::SERVER_ERROR:
      return vivaldi::vivaldi_account::FetchErrorType::kServerError;
    case ::vivaldi::VivaldiAccountManager::INVALID_CREDENTIALS:
      return vivaldi::vivaldi_account::FetchErrorType::kInvalidCredentials;
  }
}

vivaldi::vivaldi_account::FetchError ToVivaldiAccountAPIFetchError(
    ::vivaldi::VivaldiAccountManager::FetchError fetch_error) {
  vivaldi::vivaldi_account::FetchError api_fetch_error;
  api_fetch_error.error_type =
      ToVivaldiAccountAPIFetchErrorType(fetch_error.type);
  api_fetch_error.server_message = fetch_error.server_message;
  api_fetch_error.error_code = fetch_error.error_code;

  return api_fetch_error;
}

vivaldi::vivaldi_account::State GetState(Profile* profile) {
  auto* account_manager =
      ::vivaldi::VivaldiAccountManagerFactory::GetForProfile(profile);

  ::vivaldi::VivaldiAccountManager::AccountInfo account_info =
      account_manager->account_info();

  vivaldi::vivaldi_account::State state;
  state.has_token = account_manager->has_refresh_token();
  state.access_token = account_manager->access_token();
  state.has_encrypted_token = account_manager->has_encrypted_refresh_token();
  state.account_info.username = account_info.username;
  state.account_info.account_id = account_info.account_id;
  state.account_info.picture_url = account_info.picture_url;
  state.account_info.donation_tier = account_info.donation_tier;
  state.has_saved_password =
      !account_manager->password_handler()->password().empty();

  state.last_token_fetch_error =
      ToVivaldiAccountAPIFetchError(account_manager->last_token_fetch_error());
  state.last_account_info_fetch_error = ToVivaldiAccountAPIFetchError(
      account_manager->last_account_info_fetch_error());

  state.token_request_time =
      account_manager->GetTokenRequestTime().InMillisecondsFSinceUnixEpoch();
  state.next_token_request_time = account_manager->GetNextTokenRequestTime()
                                      .InMillisecondsFSinceUnixEpoch();

  state.is_ready = true;

  return state;
}
}  // anonymous namespace

VivaldiAccountEventRouter::VivaldiAccountEventRouter(Profile* profile)
    : profile_(profile) {
  auto* account_manager =
      ::vivaldi::VivaldiAccountManagerFactory::GetForProfile(profile_);
  account_manager->AddObserver(this);
  account_manager->password_handler()->AddObserver(this);
}

VivaldiAccountEventRouter::~VivaldiAccountEventRouter() {}

void VivaldiAccountEventRouter::OnVivaldiAccountUpdated() {
  ::vivaldi::BroadcastEvent(
      vivaldi::vivaldi_account::OnAccountStateChanged::kEventName,
      vivaldi::vivaldi_account::OnAccountStateChanged::Create(
          GetState(profile_)),
      profile_);
}

void VivaldiAccountEventRouter::OnTokenFetchSucceeded() {
  ::vivaldi::BroadcastEvent(
      vivaldi::vivaldi_account::OnAccountStateChanged::kEventName,
      vivaldi::vivaldi_account::OnAccountStateChanged::Create(
          GetState(profile_)),
      profile_);
}

void VivaldiAccountEventRouter::OnTokenFetchFailed() {
  ::vivaldi::BroadcastEvent(
      vivaldi::vivaldi_account::OnAccountStateChanged::kEventName,
      vivaldi::vivaldi_account::OnAccountStateChanged::Create(
          GetState(profile_)),
      profile_);
}

void VivaldiAccountEventRouter::OnAccountPasswordStateChanged() {
  ::vivaldi::BroadcastEvent(
      vivaldi::vivaldi_account::OnAccountStateChanged::kEventName,
      vivaldi::vivaldi_account::OnAccountStateChanged::Create(
          GetState(profile_)),
      profile_);
}

void VivaldiAccountEventRouter::OnVivaldiAccountShutdown() {
  auto* account_manager =
      ::vivaldi::VivaldiAccountManagerFactory::GetForProfile(profile_);
  account_manager->RemoveObserver(this);
  account_manager->password_handler()->RemoveObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<VivaldiAccountAPI>>::
    DestructorAtExit g_factory_vivaldi_account = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<VivaldiAccountAPI>*
VivaldiAccountAPI::GetFactoryInstance() {
  return g_factory_vivaldi_account.Pointer();
}

template <>
void BrowserContextKeyedAPIFactory<
    VivaldiAccountAPI>::DeclareFactoryDependencies() {
  DependsOn(::vivaldi::VivaldiAccountManagerFactory::GetInstance());
}

VivaldiAccountAPI::VivaldiAccountAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, vivaldi::vivaldi_account::OnAccountStateChanged::kEventName);
}

VivaldiAccountAPI::~VivaldiAccountAPI() {}

void VivaldiAccountAPI::OnListenerAdded(const EventListenerInfo& details) {
  vivaldi_account_event_router_.reset(new VivaldiAccountEventRouter(
      Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

// KeyedService implementation.
void VivaldiAccountAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

ExtensionFunction::ResponseAction VivaldiAccountLoginFunction::Run() {
  std::optional<vivaldi::vivaldi_account::Login::Params> params(
      vivaldi::vivaldi_account::Login::Params::Create(args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  auto* account_manager =
      ::vivaldi::VivaldiAccountManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!account_manager)
    return RespondNow(Error("Account manager is unavailable"));

  account_manager->Login(params->username, params->password,
                         params->save_password);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction VivaldiAccountLogoutFunction::Run() {
  auto* account_manager =
      ::vivaldi::VivaldiAccountManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!account_manager)
    return RespondNow(Error("Account manager is unavailable"));
  account_manager->Logout();
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction VivaldiAccountGetStateFunction::Run() {
  auto* account_manager =
      ::vivaldi::VivaldiAccountManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!account_manager)
    return RespondNow(Error("Account manager is unavailable"));

  return RespondNow(
      ArgumentList(vivaldi::vivaldi_account::GetState::Results::Create(
          GetState(Profile::FromBrowserContext(browser_context())))));
}

ExtensionFunction::ResponseAction
VivaldiAccountSetPendingRegistrationFunction::Run() {
  std::optional<vivaldi::vivaldi_account::SetPendingRegistration::Params>
      params(vivaldi::vivaldi_account::SetPendingRegistration::Params::Create(
          args()));

  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();

  if (!params->registration) {
    prefs->ClearPref(vivaldiprefs::kVivaldiAccountPendingRegistration);
    return RespondNow(NoArguments());
  }
  std::string password = params->registration->password;
  auto encrypted_password = std::make_unique<std::string>();
  // encrypted_password_ptr is expected to be valid as long as
  // encrypted_password is valid, which should be at least until OnEncryptDone
  // is called. So, it should be safe to use during OSCrypt:EncryptString
  std::string* encrypted_password_ptr = encrypted_password.get();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OSCrypt::EncryptString, password, encrypted_password_ptr),
      base::BindOnce(
          &VivaldiAccountSetPendingRegistrationFunction::OnEncryptDone, this,
          std::move(params->registration), std::move(encrypted_password)));
  return RespondLater();
}

void VivaldiAccountSetPendingRegistrationFunction::OnEncryptDone(
    std::optional<vivaldi::vivaldi_account::PendingRegistration>
        pending_registration,
    std::unique_ptr<std::string> encrypted_password,
    bool result) {
  if (!result) {
    Respond(Error("Failed to encrypt pending registration password"));
    return;
  }

  pending_registration->password = base::Base64Encode(*encrypted_password);
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();
  prefs->SetDict(vivaldiprefs::kVivaldiAccountPendingRegistration,
                 pending_registration->ToValue());

  Respond(NoArguments());
}

ExtensionFunction::ResponseAction
VivaldiAccountGetPendingRegistrationFunction::Run() {
  PrefService* prefs =
      Profile::FromBrowserContext(browser_context())->GetPrefs();

  auto pending_registration =
      vivaldi::vivaldi_account::PendingRegistration::FromValue(
          prefs->GetValue(vivaldiprefs::kVivaldiAccountPendingRegistration));

  if (!pending_registration) {
    return RespondNow(ArgumentList({}));
  }
  std::string encrypted_password;
  if (!base::Base64Decode(pending_registration->password,
                          &encrypted_password) ||
      encrypted_password.empty()) {
    return RespondNow(Error("Failed to decode pending registration password"));
  }
  std::string* password_ptr = &pending_registration->password;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&OSCrypt::DecryptString, encrypted_password, password_ptr),
      base::BindOnce(
          &VivaldiAccountGetPendingRegistrationFunction::OnDecryptDone, this,
          std::move(pending_registration)));
  return RespondLater();
}

void VivaldiAccountGetPendingRegistrationFunction::OnDecryptDone(
    std::optional<vivaldi::vivaldi_account::PendingRegistration>
        pending_registration,
    bool result) {
  if (!result) {
    Respond(Error("Failed to decrypt pending registration password"));
    return;
  }

  Respond(ArgumentList(
      vivaldi::vivaldi_account::GetPendingRegistration::Results::Create(
          *pending_registration)));
}
}  // namespace extensions
