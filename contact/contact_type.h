// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved
//
// Based on code that is:
//
// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTACT_CONTACT_TYPE_H_
#define CONTACT_CONTACT_TYPE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted_memory.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "contact/contact_typedefs.h"
#include "url/gurl.h"

#include "email_type.h"
#include "phonenumber_type.h"

namespace contact {

enum class ContactPropertyNameEnum { NONE = 0, EMAIL, PHONENUMBER };

// Bit flags determing which fields should be updated in the
// UpdateContact method
enum UpdateContactFields {
  CONTACT_ID = 1 << 0,
  NAME = 1 << 1,
  BIRTHDAY = 1 << 2,
  NOTE = 1 << 3,
};

// Represents a simplified version of a Contact.

struct Contact {
  Contact();
  Contact(const Contact& contact);
  ~Contact();
  ContactID contact_id;
  base::string16 name;
  base::Time birthday;
  base::string16 note;
  int updateFields;
};

class AddPropertyObject {
 public:
  AddPropertyObject();
  AddPropertyObject(const AddPropertyObject& contact);

  AddPropertyObject& operator=(const AddPropertyObject& other);

  ~AddPropertyObject();
  ContactPropertyNameEnum property_name;
  ContactID contact_id;
  base::string16 value;
  std::string type;
};

class UpdatePropertyObject {
 public:
  UpdatePropertyObject();
  UpdatePropertyObject(const UpdatePropertyObject& contact);

  UpdatePropertyObject& operator=(const UpdatePropertyObject& other);

  ~UpdatePropertyObject();
  ContactPropertyNameEnum property_name;
  ContactID contact_id;
  PropertyID property_id;
  base::string16 value;
  std::string type;
};

class RemovePropertyObject {
 public:
  RemovePropertyObject();
  RemovePropertyObject(const RemovePropertyObject& contact);
  ~RemovePropertyObject();
  ContactPropertyNameEnum property_name;
  ContactID contact_id;
  PropertyID property_id;
};

// Holds all information associated with a specific contact.
class ContactRow {
 public:
  ContactRow();
  ContactRow(ContactID id, base::string16 name);
  ~ContactRow();

  ContactRow(const ContactRow& row);

  ContactRow& operator=(const ContactRow& other);

  ContactID contact_id() const { return contact_id_; }
  void set_contact_id(ContactID contact_id) { contact_id_ = contact_id; }

  base::string16 name() const { return name_; }
  void set_name(base::string16 name) { name_ = name; }

  base::Time birthday() const { return birthday_; }
  void set_birthday(base::Time birthday) { birthday_ = birthday; }

  base::string16 note() const { return note_; }
  void set_note(base::string16 note) { note_ = note; }

  EmailRows emails() const { return emails_; }
  void set_emails(EmailRows emails) { emails_ = emails; }

  PhonenumberRows phones() const { return phones_; }
  void set_phones(PhonenumberRows phones) { phones_ = phones; }

  ContactID contact_id_;
  base::string16 name_;
  base::Time birthday_;
  base::string16 note_;
  EmailRows emails_;
  PhonenumberRows phones_;

 protected:
  void Swap(ContactRow* other);
};

typedef std::vector<ContactRow> ContactRows;

class ContactResult : public ContactRow {
 public:
  ContactResult();
  ContactResult(const ContactRow& contact_row);
  ContactResult(const ContactResult& other);
  ~ContactResult();

  void SwapResult(ContactResult* other);
};

class ContactQueryResults {
 public:
  typedef std::vector<std::unique_ptr<ContactResult>> ContactResultVector;

  ContactQueryResults();
  ~ContactQueryResults();

  size_t size() const { return results_.size(); }
  bool empty() const { return results_.empty(); }

  ContactResult& back() { return *results_.back(); }
  const ContactResult& back() const { return *results_.back(); }

  ContactResult& operator[](size_t i) { return *results_[i]; }
  const ContactResult& operator[](size_t i) const { return *results_[i]; }

  ContactResultVector::const_iterator begin() const { return results_.begin(); }
  ContactResultVector::const_iterator end() const { return results_.end(); }
  ContactResultVector::const_reverse_iterator rbegin() const {
    return results_.rbegin();
  }
  ContactResultVector::const_reverse_iterator rend() const {
    return results_.rend();
  }

  // Swaps the current result with another. This allows ownership to be
  // efficiently transferred without copying.
  void Swap(ContactQueryResults* other);

  // Adds the given result to the map, using swap() on the members to avoid
  // copying (there are a lot of strings and vectors). This means the parameter
  // object will be cleared after this call.
  void AppendContactBySwapping(ContactResult* result);

 private:
  // The ordered list of results. The pointers inside this are owned by this
  // QueryResults object.
  ContactResultVector results_;

  DISALLOW_COPY_AND_ASSIGN(ContactQueryResults);
};

struct CreateContactResult {
 public:
  CreateContactResult();
  bool success;
  ContactRow createdRow;

 private:
  DISALLOW_COPY_AND_ASSIGN(CreateContactResult);
};

struct UpdateContactResult {
 public:
  UpdateContactResult();
  bool success;
  ContactRow updated_contact_result;

 private:
  DISALLOW_COPY_AND_ASSIGN(UpdateContactResult);
};

struct DeleteContactResult {
 public:
  DeleteContactResult();
  bool success;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeleteContactResult);
};

}  // namespace contact

#endif  // CONTACT_CONTACT_TYPE_H_
