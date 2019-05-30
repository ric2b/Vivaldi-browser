// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_account/vivaldi_account_manager.h"

#include "base/base64.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_reader.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chromium/net/http/http_request_headers.h"
#include "components/autofill/core/common/password_form.h"
#include "components/os_crypt/os_crypt.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager_request_handler.h"

namespace vivaldi {

namespace {

constexpr char kIdentityServerUrl[] =
    "https://login.vivaldi.net/oauth2/token?scope=openid";
constexpr char kOpenIdUrl[] =
    "https://login.vivaldi.net/oauth2/userinfo?schema=openid";
constexpr char kClientId[] = "2s_O6XheApKcXNKG3jUeHjPRKDMa";
constexpr char kClientSecret[] = "AESC013J3umoN7tpkuaHROXAD28a";

constexpr char kRequestTokenFromRefreshToken[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=refresh_token&"
    "refresh_token=%s";

constexpr char kRequestTokenFromCredentials[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=password&"
    "username=%s&password=%s";

constexpr char kAccessTokenKey[] = "access_token";
constexpr char kRefreshTokenKey[] = "refresh_token";
constexpr char kExpiresInKey[] = "expires_in";

constexpr char kAccountIdKey[] = "sub";
constexpr char kPictureUrlKey[] = "picture";

constexpr char kErrorDescriptionKey[] = "error_description";

const base::string16 kVivaldiDomain = base::UTF8ToUTF16("vivaldi.net");

base::Optional<base::DictionaryValue> ParseServerResponse(
    std::unique_ptr<std::string> data) {
  if (!data)
    return base::nullopt;

  base::Optional<base::Value> value = base::JSONReader::Read(*data);
  if (!value || value->type() != base::Value::Type::DICTIONARY)
    value.reset();

  base::DictionaryValue* dict_value = nullptr;
  if (!value.value().GetAsDictionary(&dict_value))
    return base::nullopt;

  return std::move(*dict_value);
}

bool ParseGetAccessTokenSuccessResponse(
    std::unique_ptr<std::string> response_body,
    std::string* access_token,
    int* expires_in,
    std::string* refresh_token) {
  CHECK(access_token);
  CHECK(refresh_token);
  base::Optional<base::DictionaryValue> value =
      ParseServerResponse(std::move(response_body));
  if (!value)
    return false;
  return value->GetString(kAccessTokenKey, access_token) &&
         value->GetInteger(kExpiresInKey, expires_in) &&
         value->GetString(kRefreshTokenKey, refresh_token);
}

bool ParseGetAccountInfoSuccessResponse(
    std::unique_ptr<std::string> response_body,
    std::string* account_id,
    std::string* picture_url) {
  base::Optional<base::DictionaryValue> value =
      ParseServerResponse(std::move(response_body));
  if (!value)
    return false;
  // Picture url field is optional.
  value->GetString(kPictureUrlKey, picture_url);
  return value->GetString(kAccountIdKey, account_id);
}

bool ParseFailureResponse(std::unique_ptr<std::string> response_body,
                          std::string* server_message) {
  CHECK(server_message);
  base::Optional<base::DictionaryValue> value =
      ParseServerResponse(std::move(response_body));
  return value ? value->GetString(kErrorDescriptionKey, server_message) : false;
}

}  // anonymous namespace

const std::string VivaldiAccountManager::kSyncSignonRealm =
    "vivaldi-sync-login";
const std::string VivaldiAccountManager::kSyncOrigin =
    "vivaldi://settings/sync";

VivaldiAccountManager::FetchError::FetchError()
    : type(NONE), server_message(""), error_code(0) {}

VivaldiAccountManager::FetchError::FetchError(FetchErrorType type,
                                              std::string server_message,
                                              int error_code)
    : type(type), server_message(server_message), error_code(error_code) {}

VivaldiAccountManager::VivaldiAccountManager(Profile* profile)
    : profile_(profile) {
  account_info_.username =
      profile_->GetPrefs()->GetString(vivaldiprefs::kVivaldiAccountUsername);
  account_info_.account_id =
      profile_->GetPrefs()->GetString(vivaldiprefs::kVivaldiAccountId);
  if (account_info_.account_id.empty()) {
    if (account_info_.username.empty()) {
      MigrateOldCredentialsIfNeeded();
    }
    return;
  }

  std::string encrypted_refresh_token;
  if (base::Base64Decode(profile_->GetPrefs()->GetString(
                             vivaldiprefs::kVivaldiAccountRefreshToken),
                         &encrypted_refresh_token) &&
      !encrypted_refresh_token.empty()) {
    std::string refresh_token;
    if (OSCrypt::DecryptString(encrypted_refresh_token, &refresh_token)) {
      refresh_token_ = refresh_token;
      base::PostTaskWithTraits(
          FROM_HERE, {content::BrowserThread::UI},
          base::Bind(&VivaldiAccountManager::RequestNewToken,
                     base::Unretained(this)));
      return;
    } else {
      has_encrypted_refresh_token_ = true;
    }
  }

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(&VivaldiAccountManager::TryLoginWithSavedPassword,
                 base::Unretained(this)));
}

VivaldiAccountManager::~VivaldiAccountManager() {}

void VivaldiAccountManager::MigrateOldCredentialsIfNeeded() {
  PrefService* prefs = profile_->GetPrefs();

  if (!prefs->GetBoolean(vivaldiprefs::kSyncActive))
    return;
  prefs->ClearPref(vivaldiprefs::kSyncActive);

  std::string username = prefs->GetString(vivaldiprefs::kDotNetUsername);
  std::string account_id = prefs->GetString(vivaldiprefs::kSyncUsername);
  prefs->ClearPref(vivaldiprefs::kDotNetUsername);
  prefs->ClearPref(vivaldiprefs::kSyncUsername);

  if (username.empty() || account_id.empty())
    return;

  account_info_.username = username;
  account_info_.account_id = account_id;

  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::UI},
      base::Bind(&VivaldiAccountManager::TryLoginWithSavedPassword,
                 base::Unretained(this)));
}

void VivaldiAccountManager::TryLoginWithSavedPassword() {
  scoped_refptr<password_manager::PasswordStore> password_store(
      PasswordStoreFactory::GetForProfile(profile_,
                                          ServiceAccessType::IMPLICIT_ACCESS));

  password_manager::PasswordStore::FormDigest form_digest(
      autofill::PasswordForm::SCHEME_OTHER, kSyncSignonRealm,
      GURL(kSyncOrigin));

  password_store->GetLogins(form_digest, this);
}

void VivaldiAccountManager::OnGetPasswordStoreResults(
    std::vector<std::unique_ptr<autofill::PasswordForm>> results) {
  for (const auto& result : results) {
    const std::string username = base::UTF16ToUTF8(result->username_value);
    if (username == account_info_.username &&
        !account_info_.account_id.empty()) {
      Login(username, base::UTF16ToUTF8(result->password_value), false);
    }
  }

  if (!delayed_failure_notification_.is_null())
    std::move(delayed_failure_notification_).Run();
}

void VivaldiAccountManager::Login(const std::string& username,
                                  const std::string& password,
                                  bool save_password) {
  DCHECK(!username.empty());

  if (username == account_info_.username && !account_info_.account_id.empty()) {
    // Trying to re-login into the same account. Don't do a full reset.
    ClearTokens();
  } else {
    Reset();
  }

  account_info_.username = username;
  profile_->GetPrefs()->SetString(vivaldiprefs::kVivaldiAccountUsername,
                                  username);

  NotifyAccountUpdated();

  auto domain_position = username.find_first_of("@");
  if (domain_position != std::string::npos &&
      base::i18n::ToLower(base::UTF8ToUTF16(
          username.substr(domain_position + 1))) != kVivaldiDomain) {
    NotifyTokenFetchFailed(INVALID_CREDENTIALS, "", -1);
    return;
  }

  std::string enc_client_id = net::EscapeUrlEncodedData(kClientId, true);
  std::string enc_client_secret =
      net::EscapeUrlEncodedData(kClientSecret, true);
  std::string enc_username =
      net::EscapeUrlEncodedData(username.substr(0, domain_position), true);
  std::string enc_password = net::EscapeUrlEncodedData(password, true);
  std::string body = base::StringPrintf(
      kRequestTokenFromCredentials, enc_client_id.c_str(),
      enc_client_secret.c_str(), enc_username.c_str(), enc_password.c_str());

  access_token_request_handler_ =
      std::make_unique<VivaldiAccountManagerRequestHandler>(
          profile_, GURL(kIdentityServerUrl), body, net::HttpRequestHeaders(),
          base::BindRepeating(&VivaldiAccountManager::OnTokenRequestDone,
                              base::Unretained(this), true));
  if (save_password)
    password_for_saving_ = password;
  else
    password_for_saving_.clear();
}

void VivaldiAccountManager::Logout() {
  Reset();
  NotifyAccountUpdated();
}

void VivaldiAccountManager::RequestNewToken() {
  if (refresh_token_.empty())
    return;

  // We already have a request in progress.
  if (access_token_request_handler_ && !access_token_request_handler_->done())
    return;

  std::string enc_client_id = net::EscapeUrlEncodedData(kClientId, true);
  std::string enc_client_secret =
      net::EscapeUrlEncodedData(kClientSecret, true);
  std::string enc_refresh_token =
      net::EscapeUrlEncodedData(refresh_token_, true);
  std::string body =
      base::StringPrintf(kRequestTokenFromRefreshToken, enc_client_id.c_str(),
                         enc_client_secret.c_str(), enc_refresh_token.c_str());

  ClearTokens();

  access_token_request_handler_ =
      std::make_unique<VivaldiAccountManagerRequestHandler>(
          profile_, GURL(kIdentityServerUrl), body, net::HttpRequestHeaders(),
          base::BindRepeating(&VivaldiAccountManager::OnTokenRequestDone,
                              base::Unretained(this), false));
}

void VivaldiAccountManager::ClearTokens() {
  access_token_.clear();
  token_received_time_ = base::Time();
  refresh_token_.clear();
  profile_->GetPrefs()->ClearPref(vivaldiprefs::kVivaldiAccountRefreshToken);
  has_encrypted_refresh_token_ = false;

  last_token_fetch_error_ = FetchError();
  last_account_info_fetch_error_ = FetchError();

  delayed_failure_notification_.Reset();
}

void VivaldiAccountManager::Reset() {
  access_token_request_handler_.reset();
  account_info_request_handler_.reset();

  ClearTokens();

  account_info_ = AccountInfo();
  profile_->GetPrefs()->ClearPref(vivaldiprefs::kVivaldiAccountId);
}

void VivaldiAccountManager::OnTokenRequestDone(
    bool using_password,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response_body) {
  if (!url_loader->ResponseInfo() || !url_loader->ResponseInfo()->headers) {
    access_token_request_handler_->Retry();
    NotifyTokenFetchFailed(NETWORK_ERROR, "", url_loader->NetError());
    return;
  }

  int response_code = url_loader->ResponseInfo()->headers->response_code();

  if (response_code == net::HTTP_BAD_REQUEST) {
    std::string server_message;
    ParseFailureResponse(std::move(response_body), &server_message);
    if (!using_password) {
      delayed_failure_notification_ =
          base::Bind(&VivaldiAccountManager::NotifyTokenFetchFailed,
                     base::Unretained(this), INVALID_CREDENTIALS,
                     server_message, response_code);
      TryLoginWithSavedPassword();
    } else {
      NotifyTokenFetchFailed(INVALID_CREDENTIALS, server_message,
                             response_code);
    }
    return;
  }

  if (response_code != net::HTTP_OK) {
    access_token_request_handler_->Retry();
    std::string server_message;
    if (response_body.get())
      server_message.swap(*response_body);
    NotifyTokenFetchFailed(SERVER_ERROR, server_message, response_code);
    return;
  }

  std::string access_token;
  std::string refresh_token;
  int expires_in;

  if (!ParseGetAccessTokenSuccessResponse(std::move(response_body),
                                          &access_token, &expires_in,
                                          &refresh_token)) {
    access_token_request_handler_->Retry();
    NotifyTokenFetchFailed(SERVER_ERROR, "", response_code);
    return;
  }
  access_token_ = access_token;
  refresh_token_ = refresh_token;

  std::string encrypted_refresh_token;
  if (OSCrypt::EncryptString(refresh_token_, &encrypted_refresh_token)) {
    std::string encoded_refresh_token;
    base::Base64Encode(encrypted_refresh_token, &encoded_refresh_token);
    profile_->GetPrefs()->SetString(vivaldiprefs::kVivaldiAccountRefreshToken,
                                    encoded_refresh_token);
  }

  token_received_time_ = base::Time::Now();
  NotifyTokenFetchSucceeded();

  if (account_info_.account_id.empty() || account_info_.picture_url.empty()) {
    net::HttpRequestHeaders headers;
    headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                      std::string("Bearer ") + access_token_);
    account_info_request_handler_ =
        std::make_unique<VivaldiAccountManagerRequestHandler>(
            profile_, GURL(kOpenIdUrl), "", headers,
            base::BindRepeating(
                &VivaldiAccountManager::OnAccountInfoRequestDone,
                base::Unretained(this)));
  }

  return;
}

void VivaldiAccountManager::OnAccountInfoRequestDone(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response_body) {
  if (!url_loader->ResponseInfo() || !url_loader->ResponseInfo()->headers) {
    account_info_request_handler_->Retry();
    NotifyAccountInfoFetchFailed(NETWORK_ERROR, "", url_loader->NetError());
    return;
  }

  int response_code = url_loader->ResponseInfo()->headers->response_code();

  if (response_code >= net::HTTP_INTERNAL_SERVER_ERROR) {
    account_info_request_handler_->Retry();
    std::string server_message;
    if (response_body.get())
      server_message.swap(*response_body);
    NotifyAccountInfoFetchFailed(SERVER_ERROR, server_message, response_code);
    return;
  }

  if (response_code != net::HTTP_OK) {
    std::string server_message;
    ParseFailureResponse(std::move(response_body), &server_message);
    NotifyAccountInfoFetchFailed(INVALID_CREDENTIALS, server_message,
                                 response_code);
    RequestNewToken();
    return;
  }

  std::string account_id;
  std::string picture_url;
  if (!ParseGetAccountInfoSuccessResponse(std::move(response_body), &account_id,
                                          &picture_url)) {
    account_info_request_handler_->Retry();
    NotifyAccountInfoFetchFailed(SERVER_ERROR, "", response_code);
    return;
  }

  account_info_.account_id = account_id;
  account_info_.picture_url = picture_url;

  profile_->GetPrefs()->SetString(vivaldiprefs::kVivaldiAccountId, account_id);

  NotifyAccountUpdated();
  return;
}

void VivaldiAccountManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VivaldiAccountManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::Time VivaldiAccountManager::GetTokenRequestTime() {
  if (access_token_request_handler_)
    return access_token_request_handler_->request_start_time();
  return base::Time();
}

base::Time VivaldiAccountManager::GetNextTokenRequestTime() {
  if (access_token_request_handler_)
    return access_token_request_handler_->GetNextRequestTime();
  return base::Time();
}

// Called from shutdown service before shutting down the browser
void VivaldiAccountManager::Shutdown() {
  NotifyShutdown();
}

void VivaldiAccountManager::NotifyAccountUpdated() {
  for (auto& observer : observers_) {
    observer.OnVivaldiAccountUpdated();
  }
}

void VivaldiAccountManager::NotifyTokenFetchSucceeded() {
  if (!password_for_saving_.empty()) {
    scoped_refptr<password_manager::PasswordStore> password_store(
        PasswordStoreFactory::GetForProfile(
            profile_, ServiceAccessType::EXPLICIT_ACCESS));

    autofill::PasswordForm password_form = {};
    password_form.scheme = autofill::PasswordForm::SCHEME_OTHER;
    password_form.signon_realm = kSyncSignonRealm;
    password_form.origin = GURL(kSyncOrigin);
    password_form.username_value = base::UTF8ToUTF16(account_info_.username);
    password_form.password_value = base::UTF8ToUTF16(password_for_saving_);
    password_form.date_created = base::Time::Now();

    password_store->AddLogin(password_form);

    password_for_saving_.clear();
  }

  for (auto& observer : observers_) {
    observer.OnTokenFetchSucceeded();
  }
}

void VivaldiAccountManager::NotifyTokenFetchFailed(
    FetchErrorType type,
    const std::string& server_message,
    int error_code) {
  last_token_fetch_error_ = FetchError(type, server_message, error_code);
  password_for_saving_.clear();

  for (auto& observer : observers_) {
    observer.OnTokenFetchFailed();
  }
}

void VivaldiAccountManager::NotifyAccountInfoFetchFailed(
    FetchErrorType type,
    const std::string& server_message,
    int error_code) {
  last_account_info_fetch_error_ = FetchError(type, server_message, error_code);
  for (auto& observer : observers_) {
    observer.OnAccountInfoFetchFailed();
  }
}

void VivaldiAccountManager::NotifyShutdown() {
  for (auto& observer : observers_) {
    observer.OnVivaldiAccountShutdown();
  }
}

}  // namespace vivaldi
