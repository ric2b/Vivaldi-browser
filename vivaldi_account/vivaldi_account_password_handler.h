// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_PASSWORD_HANDLER_H_
#define VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_PASSWORD_HANDLER_H_

#include <string>

#include "components/password_manager/core/browser/password_store/password_store.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"

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

  explicit VivaldiAccountPasswordHandler(
      scoped_refptr<password_manager::PasswordStoreInterface> password_store,
      Delegate* delegate);
  ~VivaldiAccountPasswordHandler() override;
  VivaldiAccountPasswordHandler(const VivaldiAccountPasswordHandler&) = delete;
  VivaldiAccountPasswordHandler& operator=(
      const VivaldiAccountPasswordHandler&) = delete;

  std::string password() const { return password_; }

  void SetPassword(const std::string& password);
  void ForgetPassword();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Implementing password_manager::PasswordStoreConsumer
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<password_manager::PasswordForm>> results)
      override;

  // Implementing password_manager::PasswordStore::Observer
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

 private:
  const raw_ptr<Delegate> delegate_;
  scoped_refptr<password_manager::PasswordStoreInterface> password_store_;

  base::ObserverList<Observer> observers_;

  std::string password_;

  void PasswordReceived(const std::string& password);
  void UpdatePassword();

  base::WeakPtrFactory<VivaldiAccountPasswordHandler> weak_ptr_factory_{this};
};

}  // namespace vivaldi

#endif  // VIVALDI_ACCOUNT_VIVALDI_ACCOUNT_PASSWORD_HANDLER_H_
