// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_H_
#define VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_H_

#include <memory>
#include <string>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/service/sync_token_status.h"
#include "vivaldi_account/vivaldi_account_password_handler.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace vivaldi {

class VivaldiAccountManagerRequestHandler;

class VivaldiAccountManager : public KeyedService,
                              VivaldiAccountPasswordHandler::Delegate,
                              VivaldiAccountPasswordHandler::Observer {
 public:
  enum FetchErrorType {
    NONE = 0,
    NETWORK_ERROR = 1,
    SERVER_ERROR = 2,
    INVALID_CREDENTIALS = 3
  };

  // Keeps data about errors encountered during an attempt to communicate with
  // the login server
  struct FetchError {
    FetchError();
    FetchError(FetchErrorType type, std::string server_message, int error_code);
    FetchErrorType type;
    std::string server_message;
    int error_code;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override {}

    // Called everytime there has been a change that affects the output of
    // account_info().
    virtual void OnVivaldiAccountUpdated() {}
    // Called immediately after a new access token/refresh token pair has been
    // obtained from the server.
    virtual void OnTokenFetchSucceeded() {}
    // Called when an attempt to obtain a new token from the server failed for
    // any reason.
    virtual void OnTokenFetchFailed() {}
    // This service is about to shut down. Observers should unregister
    // themselves.
    virtual void OnVivaldiAccountShutdown() = 0;
  };

  struct AccountInfo {
    // The username that was provided by the user.
    std::string username;
    // A unique account identifier, retrieved from the server. It can be used to
    // know with certainty whether the logged in account has changed and when
    // all account informations have been recovered from the server. This is
    // typically a case-sensitive version of the user name.
    std::string account_id;
    // A link to the user's avatar, as provided by the server. It will typically
    // be an https url, but it can be any sort of URL
    std::string picture_url;

    // The reward tier corresponding to the amount donated by the user.
    std::string donation_tier;

    bool operator==(const AccountInfo& other) const;
  };

  explicit VivaldiAccountManager(
      PrefService* prefs,
      PrefService* local_state,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      scoped_refptr<password_manager::PasswordStoreInterface> password_store);
  ~VivaldiAccountManager() override;
  VivaldiAccountManager(const VivaldiAccountManager&) = delete;
  VivaldiAccountManager& operator=(const VivaldiAccountManager&) = delete;

  // Attempts to log in to the vivaldi account server. If |save_password| is
  // true, the provided credentials will be saved to the password manager if the
  // login is successful.
  void Login(const std::string& username,
             const std::string& password,
             bool save_password);
  // Clears all tokens and user-retrieved account informations
  void Logout();

  // Can be called by a consumer of the account manager if an issue is detected
  // with the current access token, in order to attempt to renew it. Usually,
  // expiring tokens will be renewed automatically.
  void RequestNewToken();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  AccountInfo account_info() const { return account_info_; }
  // Wether the account manager has a known valid refresh token available. As
  // long as this is the case, the user is considered logged in and it is
  // possible to request new access tokens
  bool has_refresh_token() const { return !refresh_token_.empty(); }
  // Whether an encrypted refresh token was found after a browser restart, but
  // could not be decrypted.
  bool has_encrypted_refresh_token() const { return has_encrypted_refresh_token_; }

  std::string access_token() const { return access_token_; }
  base::Time token_received_time() const { return token_received_time_; }
  FetchError last_token_fetch_error() const { return last_token_fetch_error_; }
  FetchError last_account_info_fetch_error() const {
    return last_account_info_fetch_error_;
  }

  // The time at which the last request for a token was performed.
  base::Time GetTokenRequestTime() const;
  // If the last token request failed, this provides the time at which the next
  // attempt will be made (if it can be retried).
  base::Time GetNextTokenRequestTime() const;

  VivaldiAccountPasswordHandler* password_handler() {
    return &password_handler_;
  }

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  // Implementing VivaldiAccountPasswordHandler::Delegate
  std::string GetUsername() override;

  // Implementing VivaldiAccountPasswordHandler::Observer
  void OnAccountPasswordStateChanged() override;

 private:
  void OnTokenRequestDone(bool using_password,
                          std::unique_ptr<network::SimpleURLLoader> url_loader,
                          std::unique_ptr<std::string> response_body);
  bool UpdateAccountInfoFromJwt(const std::string& jwt);

  void NotifyAccountUpdated();
  void NotifyTokenFetchSucceeded();
  void NotifyTokenFetchFailed(FetchErrorType type,
                              const std::string& server_message,
                              int error_code);
  void NotifyShutdown();

  void MigrateOldCredentialsIfNeeded();
  void TryLoginWithSavedPassword();

  void ClearTokens();
  void Reset();

  const raw_ptr<PrefService> prefs_;
  const raw_ptr<PrefService> local_state_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  base::ObserverList<Observer> observers_;

  AccountInfo account_info_;
  std::string device_id_;
  std::string access_token_;
  base::Time token_received_time_;
  std::string refresh_token_;
  bool has_encrypted_refresh_token_ = false;

  // Temporarily keeps a copy of the password if Login was called with
  // save_password set to true
  std::string password_for_saving_;
  VivaldiAccountPasswordHandler password_handler_;

  std::unique_ptr<VivaldiAccountManagerRequestHandler>
      access_token_request_handler_;
  std::unique_ptr<VivaldiAccountManagerRequestHandler>
      account_info_request_handler_;

  FetchError last_token_fetch_error_;
  FetchError last_account_info_fetch_error_;

  base::WeakPtrFactory<VivaldiAccountManager> weak_factory_{this};
};

}  // namespace vivaldi

#endif  // VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_MANAGER_H_
