// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_PASSWORD_HANDLER_H_
#define VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_PASSWORD_HANDLER_H_

#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "chromium/components/password_manager/core/browser/password_store.h"
#include "chromium/components/password_manager/core/browser/password_store_consumer.h"

class Profile;

namespace vivaldi {

class VivaldiAccountPasswordHandler
    : public password_manager::PasswordStore::Observer,
      public password_manager::PasswordStoreConsumer {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual std::string GetUsername() = 0;
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override {}

    virtual void OnAccountPasswordStateChanged() = 0;
  };

  explicit VivaldiAccountPasswordHandler(Profile* profile, Delegate* delegate);
  ~VivaldiAccountPasswordHandler() override;

  std::string password() const { return password_; }

  void SetPassword(const std::string& password);
  void ForgetPassword();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Implementing password_manager::PasswordStoreConsumer
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

  // Implementing password_manager::PasswordStore::Observer
  void OnLoginsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

 private:
  Delegate* delegate_;
  scoped_refptr<password_manager::PasswordStore> password_store_;

  base::ObserverList<Observer> observers_;

  std::string password_;

  void PasswordReceived(const std::string& password);
  void UpdatePassword();

  DISALLOW_COPY_AND_ASSIGN(VivaldiAccountPasswordHandler);
};

}  // namespace vivaldi

#endif  // VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_PASSWORD_HANDLER_H_
