// Copyright (c) 2013-2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact/contact_service.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/i18n/string_compare.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "components/variations/variations_associated_data.h"

#include "contact/contact_backend.h"
#include "contact/contact_model_observer.h"
#include "contact/contact_type.h"

using base::Time;

using contact::ContactBackend;
namespace contact {

// Sends messages from the db backend to us on the main thread. This must be a
// separate class from the contact service so that it can hold a reference to
// the contact service (otherwise we would have to manually AddRef and
// Release when the Backend has a reference to us).

class ContactService::ContactBackendDelegate
    : public ContactBackend::ContactDelegate {
 public:
  ContactBackendDelegate(
      const base::WeakPtr<ContactService>& contact_service,
      const scoped_refptr<base::SequencedTaskRunner>& service_task_runner)
      : contact_service_(contact_service),
        service_task_runner_(service_task_runner) {}

  void DBLoaded() override {
    service_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ContactService::OnDBLoaded, contact_service_));
  }

  void NotifyContactCreated(const ContactRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ContactService::OnContactCreated,
                                  contact_service_, row));
  }

  void NotifyContactModified(const ContactRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ContactService::OnContactChanged,
                                  contact_service_, row));
  }

  void NotifyContactDeleted(const ContactRow& row) override {
    service_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ContactService::OnContactDeleted,
                                  contact_service_, row));
  }

 private:
  const base::WeakPtr<ContactService> contact_service_;
  const scoped_refptr<base::SequencedTaskRunner> service_task_runner_;
};

ContactService::ContactService()
    : backend_loaded_(false), weak_ptr_factory_(this) {}

ContactService::~ContactService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  Cleanup();
}

void ContactService::Shutdown() {
  Cleanup();
}

bool ContactService::Init(
    bool no_db,
    const ContactDatabaseParams& contact_database_params) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!backend_task_runner_);

  backend_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskPriority::USER_BLOCKING,
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN});

  // Create the contact backend.
  scoped_refptr<ContactBackend> backend(new ContactBackend(
    new ContactBackendDelegate(weak_ptr_factory_.GetWeakPtr(),
      base::SingleThreadTaskRunner::GetCurrentDefault()),
    backend_task_runner_));
  contact_backend_.swap(backend);

  ScheduleTask(base::BindOnce(&ContactBackend::Init, contact_backend_, no_db,
                              contact_database_params));

  return true;
}

void ContactService::ScheduleTask(base::OnceClosure task) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(backend_task_runner_);

  backend_task_runner_->PostTask(FROM_HERE, std::move(task));
}

void ContactService::AddObserver(ContactModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.AddObserver(observer);
}

void ContactService::RemoveObserver(ContactModelObserver* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  observers_.RemoveObserver(observer);
}

void ContactService::OnDBLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  backend_loaded_ = true;
  NotifyContactServiceLoaded();
}

void ContactService::NotifyContactServiceLoaded() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (ContactModelObserver& observer : observers_)
    observer.OnContactServiceLoaded(this);
}

void ContactService::Cleanup() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!backend_task_runner_) {
    // We've already cleaned up.
    return;
  }

  NotifyContactServiceBeingDeleted();

  weak_ptr_factory_.InvalidateWeakPtrs();

  // Unload the backend.
  if (contact_backend_.get()) {
    ScheduleTask(
        base::BindOnce(&ContactBackend::Closing, std::move(contact_backend_)));
  }

  // Clear |backend_task_runner_| to make sure it's not used after Cleanup().
  backend_task_runner_ = nullptr;
}

void ContactService::NotifyContactServiceBeingDeleted() {}

void ContactService::OnContactCreated(const ContactRow& row) {
  for (ContactModelObserver& observer : observers_) {
    observer.OnContactCreated(this, row);
  }
}

void ContactService::OnContactDeleted(const ContactRow& row) {
  for (ContactModelObserver& observer : observers_) {
    observer.OnContactDeleted(this, row);
  }
}

void ContactService::OnContactChanged(const ContactRow& row) {
  for (ContactModelObserver& observer : observers_) {
    observer.OnContactChanged(this, row);
  }
}

base::CancelableTaskTracker::TaskId ContactService::CreateContact(
    ContactRow ev,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::CreateContact, contact_backend_, ev,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::CreateContacts(
    std::vector<contact::ContactRow> contacts,
    CreateContactsCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<CreateContactsResult> create_results =
      std::shared_ptr<CreateContactsResult>(new CreateContactsResult());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::CreateContacts, contact_backend_,
                     contacts, create_results),
      base::BindOnce(std::move(callback), create_results));
}

base::CancelableTaskTracker::TaskId ContactService::AddProperty(
    AddPropertyObject ev,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::AddProperty, contact_backend_, ev,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::AddEmailAddress(
    EmailAddressRow email,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::AddEmailAddress, contact_backend_, email,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::UpdateEmailAddress(
    EmailAddressRow email,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::UpdateEmailAddress, contact_backend_,
                     email, query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::RemoveEmailAddress(
    ContactID contact_id,
    EmailAddressID email_id,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::RemoveEmailAddress, contact_backend_,
                     contact_id, email_id, query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::UpdateProperty(
    UpdatePropertyObject update_property,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::UpdateProperty, contact_backend_,
                     update_property, query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::RemoveProperty(
    RemovePropertyObject ev,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<ContactResults> query_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::RemoveProperty, contact_backend_, ev,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::GetAllContacts(
    QueryContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<ContactQueryResults> query_results =
      std::shared_ptr<ContactQueryResults>(new ContactQueryResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::GetAllContacts, contact_backend_,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::GetAllEmailAddresses(
    QueryEmailAddressesCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<contact::EmailAddressRows> query_results =
      std::shared_ptr<EmailAddressRows>(new EmailAddressRows());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::GetAllEmailAddresses, contact_backend_,
                     query_results),
      base::BindOnce(std::move(callback), query_results));
}

base::CancelableTaskTracker::TaskId ContactService::UpdateContact(
    ContactID contact_id,
    Contact contact,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<ContactResults> update_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::UpdateContact, contact_backend_,
                     contact_id, contact, update_results),
      base::BindOnce(std::move(callback), update_results));
}

base::CancelableTaskTracker::TaskId ContactService::DeleteContact(
    ContactID contact_id,
    ContactCallback callback,
    base::CancelableTaskTracker* tracker) {
  DCHECK(backend_task_runner_) << "Contact service being called after cleanup";

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::shared_ptr<ContactResults> delete_results =
      std::shared_ptr<ContactResults>(new ContactResults());

  return tracker->PostTaskAndReply(
      backend_task_runner_.get(), FROM_HERE,
      base::BindOnce(&ContactBackend::DeleteContact, contact_backend_,
                     contact_id, delete_results),
      base::BindOnce(std::move(callback), delete_results));
}

}  // namespace contact
