// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#include "vivaldi_account/vivaldi_account_manager.h"

#include "app/vivaldi_version_info.h"
#include "base/base64.h"
#include "base/i18n/case_conversion.h"
#include "base/json/json_reader.h"
#include "base/rand_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/os_crypt/sync/os_crypt.h"
#include "components/prefs/pref_service.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "prefs/vivaldi_pref_names.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "vivaldi/prefs/vivaldi_gen_prefs.h"
#include "vivaldi_account/vivaldi_account_manager_request_handler.h"

#if BUILDFLAG(IS_ANDROID)
#include "vivaldi_account/vivaldi_account_manager_android.h"
#endif

namespace vivaldi {

namespace {

constexpr char kClientId[] = "AxWv6DRV0M0WK03xcNof5gNf6RAa";
constexpr char kClientSecret[] = "OgyH7rCuaGaLLdIJ9tlVYw416y4a";

constexpr char kRequestTokenFromRefreshToken[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=refresh_token&"
    "refresh_token=%s&"
    "scope=openid device_%s";

constexpr char kRequestTokenFromCredentials[] =
    "client_id=%s&"
    "client_secret=%s&"
    "grant_type=password&"
    "username=%s&password=%s&"
    "scope=openid device_%s";

constexpr char kAccessTokenKey[] = "access_token";
constexpr char kRefreshTokenKey[] = "refresh_token";
constexpr char kExpiresInKey[] = "expires_in";

constexpr char kAccountIdKey[] = "sub";
constexpr char kPictureUrlKey[] = "picture";
constexpr char kDonationTierKey[] = "donator";

constexpr char kErrorDescriptionKey[] = "error_description";

const std::u16string kVivaldiDomain = u"vivaldi.net";

std::optional<base::Value::Dict> ParseServerResponse(
    std::unique_ptr<std::string> data) {
  if (!data)
    return std::nullopt;

  std::optional<base::Value> value = base::JSONReader::Read(*data);
  if (!value || !value->is_dict())
    return std::nullopt;

  return std::move(value->GetDict());
}

bool ParseGetAccessTokenSuccessResponse(
    std::unique_ptr<std::string> response_body,
    std::string& access_token,
    int& expires_in,
    std::string& refresh_token) {
  std::optional<base::Value::Dict> dict =
      ParseServerResponse(std::move(response_body));
  if (!dict)
    return false;
  std::string* access_token_value = dict->FindString(kAccessTokenKey);
  std::optional<int> expires_in_value = dict->FindInt(kExpiresInKey);
  std::string* refresh_token_value = dict->FindString(kRefreshTokenKey);
  if (!access_token_value || !expires_in_value || !refresh_token_value)
    return false;
  access_token = std::move(*access_token_value);
  expires_in = *expires_in_value;
  refresh_token = std::move(*refresh_token_value);
  return true;
}

std::string ParseFailureResponse(std::unique_ptr<std::string> response_body) {
  std::optional<base::Value::Dict> dict =
      ParseServerResponse(std::move(response_body));
  if (dict) {
    if (std::string* server_message = dict->FindString(kErrorDescriptionKey)) {
      return std::move(*server_message);
    }
  }
  return std::string();
}
}  // anonymous namespace

VivaldiAccountManager::FetchError::FetchError()
    : type(NONE), server_message(""), error_code(0) {}

VivaldiAccountManager::FetchError::FetchError(FetchErrorType type,
                                              std::string server_message,
                                              int error_code)
    : type(type), server_message(server_message), error_code(error_code) {}

bool VivaldiAccountManager::AccountInfo::operator==(
    const AccountInfo& other) const = default;

VivaldiAccountManager::VivaldiAccountManager(
    PrefService* prefs,
    PrefService* local_state,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    scoped_refptr<password_manager::PasswordStoreInterface> password_store)
    : prefs_(prefs),
      local_state_(local_state),
      url_loader_factory_(std::move(url_loader_factory)),
      password_handler_(std::move(password_store), this) {
#if BUILDFLAG(IS_ANDROID)
  VivaldiAccountManagerAndroid::CreateNow();
#endif

  account_info_.username =
      prefs_->GetString(vivaldiprefs::kVivaldiAccountUsername);
  account_info_.account_id = prefs_->GetString(vivaldiprefs::kVivaldiAccountId);
  device_id_ = prefs_->GetString(vivaldiprefs::kVivaldiAccountDeviceId);
  if (account_info_.account_id.empty()) {
    if (account_info_.username.empty()) {
      MigrateOldCredentialsIfNeeded();
    }
    return;
  }

  std::string encrypted_refresh_token;
  if (!device_id_.empty() &&
      base::Base64Decode(
          prefs_->GetString(vivaldiprefs::kVivaldiAccountRefreshToken),
          &encrypted_refresh_token) &&
      !encrypted_refresh_token.empty()) {
    std::string refresh_token;
    if (OSCrypt::DecryptString(encrypted_refresh_token, &refresh_token)) {
      refresh_token_ = refresh_token;
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&VivaldiAccountManager::RequestNewToken,
                                    weak_factory_.GetWeakPtr()));
      return;
    } else {
      has_encrypted_refresh_token_ = true;
    }
  }

  password_handler_.AddObserver(this);
}

VivaldiAccountManager::~VivaldiAccountManager() {}

void VivaldiAccountManager::MigrateOldCredentialsIfNeeded() {
  if (!prefs_->GetBoolean(vivaldiprefs::kSyncActive))
    return;
  prefs_->ClearPref(vivaldiprefs::kSyncActive);

  std::string username = prefs_->GetString(vivaldiprefs::kDotNetUsername);
  std::string account_id = prefs_->GetString(vivaldiprefs::kSyncUsername);
  prefs_->ClearPref(vivaldiprefs::kDotNetUsername);
  prefs_->ClearPref(vivaldiprefs::kSyncUsername);

  if (username.empty() || account_id.empty())
    return;

  account_info_.username = username;
  account_info_.account_id = account_id;

  password_handler_.AddObserver(this);
}

void VivaldiAccountManager::OnAccountPasswordStateChanged() {
  password_handler_.RemoveObserver(this);

  if (account_info_.account_id.empty() || !refresh_token_.empty())
    return;

  Login(account_info_.username, std::string(), false);
}

void VivaldiAccountManager::Login(const std::string& untrimmed_username,
                                  const std::string& password,
                                  bool save_password) {
  std::string username(
      base::TrimWhitespaceASCII(untrimmed_username, base::TRIM_ALL));
  DCHECK(!username.empty());

  if (!password.empty() || username != account_info_.username) {
    // If the user provided new credentials, we just want to forget the
    // previously stored ones.
    password_handler_.ForgetPassword();
  }

  if (username == account_info_.username && !account_info_.account_id.empty()) {
    // Trying to re-login into the same account. Don't do a full reset.
    ClearTokens();
  } else {
    Reset();
  }

  account_info_.username = username;
  prefs_->SetString(vivaldiprefs::kVivaldiAccountUsername, username);
  device_id_ = base::Uuid::GenerateRandomV4().AsLowercaseString();
  prefs_->SetString(vivaldiprefs::kVivaldiAccountDeviceId, device_id_);

  NotifyAccountUpdated();

  auto domain_position = username.find_first_of("@");
  if (domain_position != std::string::npos &&
      base::i18n::ToLower(base::UTF8ToUTF16(
          username.substr(domain_position + 1))) != kVivaldiDomain) {
    NotifyTokenFetchFailed(INVALID_CREDENTIALS, "", -1);
    return;
  }

  std::string url_encoded_client_id =
      base::EscapeUrlEncodedData(kClientId, true);
  std::string url_encoded_client_secret =
      base::EscapeUrlEncodedData(kClientSecret, true);
  std::string url_encoded_username =
      base::EscapeUrlEncodedData(username.substr(0, domain_position), true);
  std::string url_encoded_password = base::EscapeUrlEncodedData(
      password.empty() ? password_handler_.password() : password, true);
  std::string url_encoded_device_id =
      base::EscapeUrlEncodedData(device_id_, true);
  std::string body = base::StringPrintf(
      kRequestTokenFromCredentials, url_encoded_client_id.c_str(),
      url_encoded_client_secret.c_str(), url_encoded_username.c_str(),
      url_encoded_password.c_str(), url_encoded_device_id.c_str());

  const GURL identity_server_url(
      local_state_->GetString(vivaldiprefs::kVivaldiAccountServerUrlIdentity));

  access_token_request_handler_ =
      std::make_unique<VivaldiAccountManagerRequestHandler>(
          url_loader_factory_, identity_server_url, body,
          net::HttpRequestHeaders(),
          base::BindRepeating(&VivaldiAccountManager::OnTokenRequestDone,
                              weak_factory_.GetWeakPtr(), true));
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
  if (refresh_token_.empty() || device_id_.empty())
    return;

  // We already have a request in progress.
  if (access_token_request_handler_ && !access_token_request_handler_->done())
    return;

  std::string url_encoded_client_id =
      base::EscapeUrlEncodedData(kClientId, true);
  std::string url_encoded_client_secret =
      base::EscapeUrlEncodedData(kClientSecret, true);
  std::string url_encoded_refresh_token =
      base::EscapeUrlEncodedData(refresh_token_, true);
  std::string url_encoded_device_id =
      base::EscapeUrlEncodedData(device_id_, true);
  std::string body = base::StringPrintf(
      kRequestTokenFromRefreshToken, url_encoded_client_id.c_str(),
      url_encoded_client_secret.c_str(), url_encoded_refresh_token.c_str(),
      url_encoded_device_id.c_str());

  ClearTokens();

  const GURL identity_server_url(
      local_state_->GetString(vivaldiprefs::kVivaldiAccountServerUrlIdentity));

  access_token_request_handler_ =
      std::make_unique<VivaldiAccountManagerRequestHandler>(
          url_loader_factory_, identity_server_url, body,
          net::HttpRequestHeaders(),
          base::BindRepeating(&VivaldiAccountManager::OnTokenRequestDone,
                              weak_factory_.GetWeakPtr(), false));
}

void VivaldiAccountManager::ClearTokens() {
  access_token_.clear();
  token_received_time_ = base::Time();
  refresh_token_.clear();
  prefs_->ClearPref(vivaldiprefs::kVivaldiAccountRefreshToken);
  has_encrypted_refresh_token_ = false;

  last_token_fetch_error_ = FetchError();
  last_account_info_fetch_error_ = FetchError();
}

void VivaldiAccountManager::Reset() {
  access_token_request_handler_.reset();
  account_info_request_handler_.reset();

  ClearTokens();

  std::string username;
  username.swap(account_info_.username);
  account_info_ = AccountInfo();
  account_info_.username.swap(username);
  prefs_->ClearPref(vivaldiprefs::kVivaldiAccountId);
  device_id_.clear();
  prefs_->ClearPref(vivaldiprefs::kVivaldiAccountDeviceId);
}

void VivaldiAccountManager::OnTokenRequestDone(
    bool using_password,
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    std::unique_ptr<std::string> response_body) {
  if (!url_loader->ResponseInfo() || !url_loader->ResponseInfo()->headers) {
    access_token_request_handler_->Retry();
    NotifyTokenFetchFailed(NETWORK_ERROR,
                           net::ErrorToShortString(url_loader->NetError()),
                           url_loader->NetError());
    return;
  }

  int response_code = url_loader->ResponseInfo()->headers->response_code();

  if (response_code == net::HTTP_BAD_REQUEST) {
    std::string server_message = ParseFailureResponse(std::move(response_body));
    if (!using_password && !password_handler_.password().empty()) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(&VivaldiAccountManager::Login,
                         weak_factory_.GetWeakPtr(), account_info_.username,
                         std::string(), false));
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

  if (!ParseGetAccessTokenSuccessResponse(
          std::move(response_body), access_token, expires_in, refresh_token)) {
    access_token_request_handler_->Retry();
    NotifyTokenFetchFailed(SERVER_ERROR, "", response_code);
    return;
  }
  access_token_ = access_token;
  refresh_token_ = refresh_token;

  std::string encrypted_refresh_token;
  if (OSCrypt::EncryptString(refresh_token_, &encrypted_refresh_token)) {
    std::string encoded_refresh_token;
    encoded_refresh_token = base::Base64Encode(encrypted_refresh_token);
    prefs_->SetString(vivaldiprefs::kVivaldiAccountRefreshToken,
                      encoded_refresh_token);
  }

  token_received_time_ = base::Time::Now();
  NotifyTokenFetchSucceeded();

  if (UpdateAccountInfoFromJwt(access_token)) {
    NotifyAccountUpdated();
  }
}

bool VivaldiAccountManager::UpdateAccountInfoFromJwt(const std::string& jwt) {
  std::vector<std::string_view> jwt_parts = base::SplitStringPiece(
      access_token_, ".", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  if (jwt_parts.size() != 3) {
    return false;
  }

  std::string decoded;

  if (!base::Base64Decode(jwt_parts[1], &decoded,
                          base::Base64DecodePolicy::kForgiving)) {
    return false;
  }

  std::optional<base::Value> value = base::JSONReader::Read(decoded);

  if (!value || !value->is_dict()) {
    return false;
  }

  const base::Value::Dict& token_info = value->GetDict();
  const std::string* account_id = token_info.FindString(kAccountIdKey);
  const std::string* picture_url = token_info.FindString(kPictureUrlKey);
  const std::string* donation_tier = token_info.FindString(kDonationTierKey);

  if (!account_id) {
    return false;
  }

  AccountInfo new_account_info{
      .username = account_info_.username,
      .account_id = *account_id,
      .picture_url = picture_url ? *picture_url : "",
      .donation_tier = donation_tier ? *donation_tier: ""};

  if (account_info_ == new_account_info) {
    return false;
  }

  account_info_ = new_account_info;

  prefs_->SetString(vivaldiprefs::kVivaldiAccountId, account_info_.account_id);
  return true;
}

void VivaldiAccountManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void VivaldiAccountManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

base::Time VivaldiAccountManager::GetTokenRequestTime() const {
  if (access_token_request_handler_)
    return access_token_request_handler_->request_start_time();
  return base::Time();
}

base::Time VivaldiAccountManager::GetNextTokenRequestTime() const  {
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
    password_handler_.SetPassword(password_for_saving_);
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

void VivaldiAccountManager::NotifyShutdown() {
  for (auto& observer : observers_) {
    observer.OnVivaldiAccountShutdown();
  }
}

std::string VivaldiAccountManager::GetUsername() {
  return account_info_.username;
}

}  // namespace vivaldi
