// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "contact_backend.h"

#include <algorithm>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#include "sql/error_delegate_util.h"

#include "contact/contact_constants.h"
#include "contact/contact_database.h"
#include "contact/contact_database_params.h"
#include "contact/contact_type.h"
#include "contact/phonenumber_type.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace contact {

// ContactBackend -----------------------------------------------------------
ContactBackend::ContactBackend(
    ContactDelegate* delegate,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : delegate_(delegate) {}

ContactBackend::ContactBackend(ContactDelegate* delegate) {}

ContactBackend::~ContactBackend() {}

void ContactBackend::NotifyContactCreated(const ContactRow& row) {
  if (delegate_)
    delegate_->NotifyContactCreated(row);
}

void ContactBackend::NotifyContactModified(const ContactRow& row) {
  if (delegate_)
    delegate_->NotifyContactModified(row);
}

void ContactBackend::NotifyContactDeleted(const ContactRow& row) {
  if (delegate_)
    delegate_->NotifyContactDeleted(row);
}

void ContactBackend::Init(
    bool force_fail,
    const ContactDatabaseParams& contact_database_params) {
  // ContactBackend is created on the UI thread by ContactService, then the
  // ContactBackend::Init() method is called on the DB thread. Create the
  // base::SupportsUserData on the DB thread since it is not thread-safe.

  if (!force_fail)
    InitImpl(contact_database_params);
  delegate_->DBLoaded();
}

void ContactBackend::Closing() {
  // TODO(arnar): Add
  //  CancelScheduledCommit();

  // Release our reference to the delegate, this reference will be keeping the
  // contact service alive.
  delegate_.reset();
}

void ContactBackend::InitImpl(
    const ContactDatabaseParams& contact_database_params) {
  DCHECK(!db_) << "Initializing ContactBackend twice";

  // Compute the file names.
  contact_dir_ = contact_database_params.contact_dir;
  base::FilePath contact_name = contact_dir_.Append(kContactFilename);

  // Contact database.
  db_.reset(new ContactDatabase());

  sql::InitStatus status = db_->Init(contact_name);
  switch (status) {
    case sql::INIT_OK:
      break;
    case sql::INIT_FAILURE: {
      // TODO(arnar): add db init failure check
      LOG(ERROR) << "ContactBackend db init failure";
      [[fallthrough]];
    }  // Falls through.
    case sql::INIT_TOO_NEW: {
      LOG(ERROR) << "INIT_TOO_NEW";

      return;
    }
    default:
      NOTREACHED();
  }
}

void ContactBackend::GetAllContacts(
    std::shared_ptr<ContactQueryResults> results) {
  ContactRows rows;
  db_->GetAllContacts(&rows);
  for (size_t i = 0; i < rows.size(); i++) {
    ContactRow contact_row = rows[i];
    FillUpdatedContact(contact_row.contact_id(), contact_row);
    ContactResult result(contact_row);
    results->AppendContactBySwapping(&result);
  }
}

void ContactBackend::GetAllEmailAddresses(
    std::shared_ptr<EmailAddressRows> results) {
  EmailAddressRows rows;
  db_->GetAllEmailAddresses(&rows);
  for (size_t i = 0; i < rows.size(); i++) {
    results->push_back(rows[i]);
  }
}

void ContactBackend::CreateContact(ContactRow row,
                                   std::shared_ptr<ContactResults> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  ContactID id = db_->CreateContact(row);

  if (id) {
    row.set_contact_id(id);
    result->success = true;
    result->contact = row;
    NotifyContactCreated(row);
  } else {
    result->success = false;
  }
}

// Creates multiple contacts
void ContactBackend::CreateContacts(
    std::vector<ContactRow> contacts,
    std::shared_ptr<CreateContactsResult> result) {
  int success_counter = 0;
  int failed_counter = 0;

  size_t count = contacts.size();

  for (size_t i = 0; i < count; i++) {
    ContactRow contact = contacts[i];
    ContactID id = db_->CreateContact(contact);

    for (auto& item : contact.emails()) {
      item.set_contact_id(id);
      db_->AddEmailAddress(item);
    }

    for (auto& phone : contact.phones()) {
      contact::AddPropertyObject add;
      add.value = base::UTF8ToUTF16(phone.phonenumber());
      add.contact_id = id;
      db_->AddPhoneNumber(add);
    }

    for (auto& address : contact.postaladdresses()) {
      contact::AddPropertyObject add;
      add.value = address.postal_address();
      add.contact_id = id;
      db_->AddPostalAddress(add);
    }

    if (id) {
      success_counter++;
    } else {
      failed_counter++;
    }
  }

  result->number_success = success_counter;
  result->number_failed = failed_counter;
  ContactRow ev;
  NotifyContactCreated(ev);
}

void ContactBackend::UpdateContact(ContactID contact_id,
                                   const Contact& contact,
                                   std::shared_ptr<ContactResults> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  ContactRow contact_row;
  if (db_->GetRowForContact(contact_id, &contact_row)) {
    if (contact.updateFields & contact::NAME) {
      contact_row.set_name(contact.name);
    }

    if (contact.updateFields & contact::BIRTHDAY) {
      contact_row.set_birthday(contact.birthday);
    }

    if (contact.updateFields & contact::NOTE) {
      contact_row.set_note(contact.note);
    }

    if (contact.updateFields & contact::AVATAR_URL) {
      contact_row.set_avatar_url(contact.avatar_url);
    }

    if (contact.updateFields & contact::SEPARATOR) {
      contact_row.set_separator(contact.separator);
    }

    if (contact.updateFields & contact::GENERATED_FROM_SENT_MAIL) {
      contact_row.set_generated_from_sent_mail(
          contact.generated_from_sent_mail);
    }

    if (contact.updateFields & contact::TRUSTED) {
      contact_row.set_trusted(contact.trusted);
    }

    result->success = db_->UpdateContactRow(contact_row);

    if (result->success) {
      ContactRow changed_row;
      if (db_->GetRowForContact(contact_id, &changed_row)) {
        FillUpdatedContact(contact_id, changed_row);
        result->contact = changed_row;
        NotifyContactModified(changed_row);
      }
    }
  } else {
    result->success = false;
    NOTREACHED() << "Could not find contact row in DB";
    //return;
  }
}

void ContactBackend::AddProperty(AddPropertyObject row,
                                 std::shared_ptr<ContactResults> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  if (row.property_name == ContactPropertyNameEnum::PHONENUMBER) {
    AddPhoneNumber(row, result);
  } else if (row.property_name == ContactPropertyNameEnum::POSTAL_ADDRESS) {
    AddPostalAddress(row, result);
  }
}

void ContactBackend::UpdateProperty(UpdatePropertyObject row,
                                    std::shared_ptr<ContactResults> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  if (row.property_name == ContactPropertyNameEnum::PHONENUMBER) {
    UpdatePhonenumber(row, result);
  } else if (row.property_name == ContactPropertyNameEnum::POSTAL_ADDRESS) {
    UpdatePostalAddress(row, result);
  }
}

void ContactBackend::RemoveProperty(RemovePropertyObject row,
                                    std::shared_ptr<ContactResults> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  if (row.property_name == ContactPropertyNameEnum::PHONENUMBER) {
    DeletePhonenumber(row, result);
  } else if (row.property_name == ContactPropertyNameEnum::POSTAL_ADDRESS) {
    DeletePostalAddress(row, result);
  }
}

void ContactBackend::DeleteContact(ContactID contact_id,
                                   std::shared_ptr<ContactResults> result) {
  if (!db_) {
    result->success = false;
    return;
  }

  ContactRow contact_row;
  if (db_->GetRowForContact(contact_id, &contact_row)) {
    result->success = db_->DeletePostalAddressesForContact(contact_id) &&
                      db_->DeleteEmailsForContact(contact_id) &&
                      db_->DeletePhoneNumbersForContact(contact_id) &&
                      db_->DeleteContact(contact_id);
    NotifyContactDeleted(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::AddEmailAddress(EmailAddressRow row,
                                     std::shared_ptr<ContactResults> result) {
  EmailAddressID id = db_->AddEmailAddress(row);
  if (id) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id(), &contact_row);
    FillUpdatedContact(row.contact_id(), contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::UpdateEmailAddress(
    EmailAddressRow row,
    std::shared_ptr<ContactResults> result) {
  if (!db_->DoesEmailAddressIdExist(row.email_address_id(), row.contact_id())) {
    result->success = false;
    return;
  }

  if (db_->UpdateEmailAddress(row)) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id(), &contact_row);
    FillUpdatedContact(row.contact_id(), contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::RemoveEmailAddress(
    ContactID contact_id,
    EmailAddressID email_id,
    std::shared_ptr<ContactResults> result) {
  if (!db_->DoesEmailAddressIdExist(email_id, contact_id)) {
    result->success = false;
    return;
  }

  if (db_->DeleteEmail(email_id, contact_id)) {
    ContactRow contact_row;
    db_->GetRowForContact(contact_id, &contact_row);
    FillUpdatedContact(contact_id, contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::DeleteEmail(RemovePropertyObject row,
                                 std::shared_ptr<ContactResults> result) {
  bool deleteResult = db_->DeleteEmail(row.property_id, row.contact_id);
  if (deleteResult) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id, &contact_row);
    FillUpdatedContact(row.contact_id, contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::UpdatePhonenumber(UpdatePropertyObject row,
                                       std::shared_ptr<ContactResults> result) {
  if (!db_->DoesPhonumberIdExist(row.property_id, row.contact_id)) {
    result->success = false;
    return;
  }

  if (db_->UpdatePhoneNumber(row)) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id, &contact_row);
    FillUpdatedContact(row.contact_id, contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::DeletePhonenumber(RemovePropertyObject row,
                                       std::shared_ptr<ContactResults> result) {
  bool deleteResult = db_->DeletePhoneNumber(row.property_id, row.contact_id);

  if (deleteResult) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id, &contact_row);
    FillUpdatedContact(row.contact_id, contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::UpdatePostalAddress(
    UpdatePropertyObject row,
    std::shared_ptr<ContactResults> result) {
  if (!db_->DoesPostalAddressIdExist(row.property_id, row.contact_id)) {
    result->success = false;
    return;
  }

  if (db_->UpdatePostalAddress(row)) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id, &contact_row);
    FillUpdatedContact(row.contact_id, contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::DeletePostalAddress(
    RemovePropertyObject row,
    std::shared_ptr<ContactResults> result) {
  bool deleteResult = db_->DeletePostalAddress(row.property_id, row.contact_id);

  if (deleteResult) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id, &contact_row);
    FillUpdatedContact(row.contact_id, contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::FillUpdatedContact(ContactID id, ContactRow& updated_row) {
  EmailAddressRows emails;
  db_->GetEmailsForContact(id, &emails);
  updated_row.set_emails(emails);

  PhonenumberRows phone_numbers;
  db_->GetPhonenumbersForContact(id, &phone_numbers);
  updated_row.set_phones(phone_numbers);

  PostalAddressRows postal_addresses;
  db_->GetPostalAddressesForContact(id, &postal_addresses);
  updated_row.set_postaladdresses(postal_addresses);
}

void ContactBackend::AddPhoneNumber(AddPropertyObject row,
                                    std::shared_ptr<ContactResults> result) {
  PhonenumberID id = db_->AddPhoneNumber(row);

  if (id) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id, &contact_row);
    FillUpdatedContact(row.contact_id, contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::AddPostalAddress(AddPropertyObject row,
                                      std::shared_ptr<ContactResults> result) {
  PostalAddressID id = db_->AddPostalAddress(row);

  if (id) {
    ContactRow contact_row;
    db_->GetRowForContact(row.contact_id, &contact_row);
    FillUpdatedContact(row.contact_id, contact_row);
    result->success = true;
    result->contact = contact_row;
    NotifyContactModified(contact_row);
  } else {
    result->success = false;
  }
}

void ContactBackend::CloseAllDatabases() {
  if (db_) {
    // Commit the long-running transaction.
    db_->CommitTransaction();
    db_.reset();
  }
}

void ContactBackend::Commit() {
  if (!db_)
    return;

#if BUILDFLAG(IS_IOS)
  // Attempts to get the application running long enough to commit the database
  // transaction if it is currently being backgrounded.
  base::ios::ScopedCriticalAction scoped_critical_action;
#endif

  db_->CommitTransaction();
  DCHECK_EQ(db_->transaction_nesting(), 0)
      << "Somebody left a transaction open";
  db_->BeginTransaction();
}
}  // namespace contact
