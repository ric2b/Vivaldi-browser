// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_SERVICE_H_
#define CONTACT_CONTACT_SERVICE_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/cancelable_task_tracker.h"
#include "base/time/time.h"
#include "components/keyed_service/core/keyed_service.h"

#include "contact/contact_backend.h"
#include "contact/contact_database_params.h"
#include "contact/contact_model_observer.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace contact {

struct ContactDatabaseParams;
class ContactBackend;

class ContactService : public KeyedService {
 public:
  ContactService();
  ~ContactService() override;
  ContactService(const ContactService&) = delete;
  ContactService& operator=(const ContactService&) = delete;

  bool Init(bool no_db, const ContactDatabaseParams& contact_database_params);

  // Called from shutdown service before shutting down the browser
  void Shutdown() override;

  void AddObserver(ContactModelObserver* observer);
  void RemoveObserver(ContactModelObserver* observer);

  // Call to schedule a given task for running on the contact thread with the
  // specified priority. The task will have ownership taken.
  void ScheduleTask(base::OnceClosure task);

  typedef base::OnceCallback<void(std::shared_ptr<ContactResults>)>
      ContactCallback;

  typedef base::OnceCallback<void(std::shared_ptr<CreateContactsResult>)>
      CreateContactsCallback;

  typedef base::OnceCallback<void(std::shared_ptr<ContactQueryResults>)>
      QueryContactCallback;

  typedef base::OnceCallback<void(std::shared_ptr<EmailAddressRows>)>
      QueryEmailAddressesCallback;

  base::CancelableTaskTracker::TaskId CreateContact(
      ContactRow ev,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId CreateContacts(
      std::vector<contact::ContactRow> contacts,
      CreateContactsCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId AddEmailAddress(
      EmailAddressRow email,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateEmailAddress(
      EmailAddressRow email,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId RemoveEmailAddress(
      ContactID contact_id,
      EmailAddressID email_id,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId AddProperty(
      AddPropertyObject ev,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateProperty(
      UpdatePropertyObject ev,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId RemoveProperty(
      RemovePropertyObject ev,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllContacts(
      QueryContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId GetAllEmailAddresses(
      QueryEmailAddressesCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId UpdateContact(
      ContactID contact_id,
      Contact contact,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  base::CancelableTaskTracker::TaskId DeleteContact(
      ContactID contact_id,
      ContactCallback callback,
      base::CancelableTaskTracker* tracker);

  // Returns true if this contact service is currently in a mode where
  // extensive changes might happen, such as for import and sync. This is
  // helpful for observers that are created after the service has started,
  // and
  // want to check state during their own initializer.
  bool IsDoingExtensiveChanges() const { return extensive_changes_ > 0; }

 private:
  class ContactBackendDelegate;
  friend class base::RefCountedThreadSafe<ContactService>;
  friend class ContactBackendDelegate;
  friend class ContactBackend;

  friend std::unique_ptr<ContactService> CreateContactService(
      const base::FilePath& contact_dir,
      bool create_db);

  void OnDBLoaded();

  // Notify all ContactServiceObservers registered that the
  // ContactService has finished loading.
  void NotifyContactServiceLoaded();
  void NotifyContactServiceBeingDeleted();

  void OnContactCreated(const ContactRow& row);
  void OnContactDeleted(const ContactRow& row);
  void OnContactChanged(const ContactRow& row);

  void Cleanup();

  SEQUENCE_CHECKER(sequence_checker_);

  // The TaskRunner to which ContactBackend tasks are posted. Nullptr once
  // Cleanup() is called.
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;

  // This pointer will be null once Cleanup() has been called, meaning no
  // more calls should be made to the contact thread.
  scoped_refptr<ContactBackend> contact_backend_;

  // Has the backend finished loading? The backend is loaded once Init has
  // completed.
  bool backend_loaded_;

  // The observers.
  base::ObserverList<ContactModelObserver> observers_;

  // See description of IsDoingExtensiveChanges above.
  int extensive_changes_;

  // All vended weak pointers are invalidated in Cleanup().
  base::WeakPtrFactory<ContactService> weak_ptr_factory_;
};

}  // namespace contact

#endif  // CONTACT_CONTACT_SERVICE_H_
