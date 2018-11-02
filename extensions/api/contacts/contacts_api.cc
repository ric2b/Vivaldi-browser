// Copyright (c) 2017 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/contacts/contacts_api.h"

#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"

#include "contact/contact_model_observer.h"
#include "contact/contact_service.h"
#include "contact/contact_service_factory.h"

using contact::ContactPropertyNameEnum;
using contact::ContactService;
using contact::ContactServiceFactory;

namespace extensions {

using vivaldi::contacts::Contact;
using vivaldi::contacts::ContactPropertyName;
using vivaldi::contacts::EmailAddress;
using vivaldi::contacts::Phonenumber;
using vivaldi::contacts::PostalAddress;

namespace OnContactCreated = vivaldi::contacts::OnContactCreated;
namespace OnContactRemoved = vivaldi::contacts::OnContactRemoved;
namespace OnContactChanged = vivaldi::contacts::OnContactChanged;

typedef std::vector<vivaldi::contacts::Contact> ContactList;
typedef std::vector<vivaldi::contacts::EmailAddress> EmailItemList;

namespace {

bool GetIdAsInt64(const std::string& id_string, int64_t* id) {
  if (base::StringToInt64(id_string, id))
    return true;

  return false;
}

double MilliSecondsFromTime(const base::Time& time) {
  return 1000 * time.ToDoubleT();
}

base::Time GetTime(double ms_from_epoch) {
  double seconds_from_epoch = ms_from_epoch / 1000.0;
  return (seconds_from_epoch == 0)
             ? base::Time::UnixEpoch()
             : base::Time::FromDoubleT(seconds_from_epoch);
}

ContactPropertyNameEnum APIAddpropertyTypeToInternal(
    ContactPropertyName transition) {
  switch (transition) {
    case vivaldi::contacts::CONTACT_PROPERTY_NAME_PHONENUMBER:
      return ContactPropertyNameEnum::PHONENUMBER;
    case vivaldi::contacts::CONTACT_PROPERTY_NAME_POSTALADDRESS:
      return ContactPropertyNameEnum::POSTAL_ADDRESS;
    default:
      NOTREACHED();
  }
  return ContactPropertyNameEnum::NONE;
}

EmailAddress GetEmail(const contact::EmailAddressRow& row) {
  EmailAddress email;
  email.id = base::Int64ToString(row.email_address_id());
  email.email_address.reset(
      new std::string(base::UTF16ToUTF8(row.email_address())));
  email.type.reset(new std::string(row.type()));
  email.trusted.reset(new bool(row.trusted()));
  email.is_default.reset(new bool(row.is_default()));
  email.obsolete.reset(new bool(row.obsolete()));

  return email;
}

Phonenumber GetPhonenumber(const contact::PhonenumberRow& row) {
  Phonenumber phonenumber;
  phonenumber.id = base::Int64ToString(row.phonenumber_id());
  phonenumber.phone_number.reset(new std::string(row.phonenumber()));
  phonenumber.type.reset(new std::string(row.type()));

  return phonenumber;
}

PostalAddress GetPostalAddress(const contact::PostalAddressRow& row) {
  PostalAddress postaladdress;
  postaladdress.id = base::Int64ToString(row.postal_address_id());
  postaladdress.postal_address.reset(
      new std::string(base::UTF16ToUTF8(row.postal_address())));
  postaladdress.type.reset(new std::string(row.type()));

  return postaladdress;
}

Contact GetContact(const contact::ContactRow& row) {
  Contact contact;
  contact.id = base::Int64ToString(row.contact_id());
  contact.name.reset(new std::string(base::UTF16ToUTF8(row.name())));
  contact.birthday.reset(new double(MilliSecondsFromTime(row.birthday())));
  contact.note.reset(new std::string(base::UTF16ToUTF8(row.note())));
  contact.avatar_url.reset(
      new std::string(base::UTF16ToUTF8(row.avatar_url())));
  contact.generated_from_sent_mail = row.generated_from_sent_mail();

  for (const contact::EmailAddressRow& visit : row.emails()) {
    contact.email_addresses.push_back(GetEmail(visit));
  }

  for (const contact::PhonenumberRow& phonenumber : row.phones()) {
    contact.phone_numbers.push_back(GetPhonenumber(phonenumber));
  }

  for (const contact::PostalAddressRow& postaladdress : row.postaladdresses()) {
    contact.postal_addresses.push_back(GetPostalAddress(postaladdress));
  }

  return contact;
}
}  // namespace

ContactEventRouter::ContactEventRouter(Profile* profile)
    : browser_context_(profile),
      model_(ContactServiceFactory::GetForProfile(profile)) {
  model_->AddObserver(this);
}

ContactEventRouter::~ContactEventRouter() {
  model_->RemoveObserver(this);
}

void ContactEventRouter::ExtensiveContactChangesBeginning(
    ContactService* model) {}

void ContactEventRouter::ExtensiveContactChangesEnded(ContactService* model) {}

void ContactEventRouter::OnContactCreated(ContactService* service,
                                          const contact::ContactRow& row) {
  Contact createdEvent = GetContact(row);
  std::unique_ptr<base::ListValue> args =
      OnContactCreated::Create(createdEvent);
  DispatchEvent(OnContactCreated::kEventName, std::move(args));
}

void ContactEventRouter::OnContactDeleted(ContactService* service,
                                          const contact::ContactRow& row) {
  Contact deletedEvent = GetContact(row);
  std::unique_ptr<base::ListValue> args =
      OnContactRemoved::Create(deletedEvent);
  DispatchEvent(OnContactCreated::kEventName, std::move(args));
}

void ContactEventRouter::OnContactChanged(ContactService* service,
                                          const contact::ContactRow& row) {
  Contact changedEvent = GetContact(row);
  std::unique_ptr<base::ListValue> args =
      OnContactChanged::Create(changedEvent);
  DispatchEvent(OnContactChanged::kEventName, std::move(args));
}

// Helper to actually dispatch an event to extension listeners.
void ContactEventRouter::DispatchEvent(
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(base::WrapUnique(
        new extensions::Event(extensions::events::VIVALDI_EXTENSION_EVENT,
                              event_name, std::move(event_args))));
  }
}

ContactsAPI::ContactsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, OnContactCreated::kEventName);
  event_router->RegisterObserver(this, OnContactRemoved::kEventName);
  event_router->RegisterObserver(this, OnContactChanged::kEventName);
}

ContactsAPI::~ContactsAPI() {}

void ContactsAPI::Shutdown() {
  contact_event_router_.reset();
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<ContactsAPI>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ContactsAPI>* ContactsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void ContactsAPI::OnListenerAdded(const EventListenerInfo& details) {
  contact_event_router_.reset(
      new ContactEventRouter(Profile::FromBrowserContext(browser_context_)));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

std::unique_ptr<Contact> CreateVivaldiContact(
    const contact::ContactResult& contact_res) {
  std::unique_ptr<Contact> contact(new Contact());

  contact->id = base::Int64ToString(contact_res.contact_id());
  contact->name.reset(new std::string(base::UTF16ToUTF8(contact_res.name())));
  contact->birthday.reset(
      new double(MilliSecondsFromTime(contact_res.birthday())));
  contact->note.reset(new std::string(base::UTF16ToUTF8(contact_res.note())));
  contact->avatar_url.reset(
      new std::string(base::UTF16ToUTF8(contact_res.avatar_url())));
  contact->separator = contact_res.separator();
  contact->generated_from_sent_mail = contact_res.generated_from_sent_mail();

  for (const contact::EmailAddressRow& visit : contact_res.emails()) {
    contact->email_addresses.push_back(GetEmail(visit));
  }

  for (const contact::PhonenumberRow& phonenumber : contact_res.phones()) {
    contact->phone_numbers.push_back(GetPhonenumber(phonenumber));
  }

  for (const contact::PostalAddressRow& postaladdress :
       contact_res.postaladdresses()) {
    contact->postal_addresses.push_back(GetPostalAddress(postaladdress));
  }

  return contact;
}

ContactsGetAllFunction::~ContactsGetAllFunction() {}

ExtensionFunction::ResponseAction ContactsGetAllFunction::Run() {
  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->GetAllContacts(
      base::Bind(&ContactsGetAllFunction::GetAllComplete, this),
      &task_tracker_);

  return RespondLater();  // GetAllComplete() will be called
                          // asynchronously.
}

void ContactsGetAllFunction::GetAllComplete(
    std::shared_ptr<contact::ContactQueryResults> results) {
  ContactList contactList;

  if (results && !results->empty()) {
    for (contact::ContactQueryResults::ContactResultVector::const_iterator
             iterator = results->begin();
         iterator != results->end(); ++iterator) {
      contactList.push_back(std::move(
          *base::WrapUnique((CreateVivaldiContact(**iterator).release()))));
    }
  }
  Respond(
      ArgumentList(vivaldi::contacts::GetAll::Results::Create(contactList)));
}

ContactsGetAllEmailAddressesFunction::~ContactsGetAllEmailAddressesFunction() {}

ExtensionFunction::ResponseAction ContactsGetAllEmailAddressesFunction::Run() {
  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->GetAllEmailAddresses(
      base::Bind(
          &ContactsGetAllEmailAddressesFunction::GetAllEmailAddressesComplete,
          this),
      &task_tracker_);

  return RespondLater();  // GetAllEmailAddressesComplete() will be called
                          // asynchronously.
}

void ContactsGetAllEmailAddressesFunction::GetAllEmailAddressesComplete(
    std::shared_ptr<contact::EmailAddressRows> results) {
  EmailItemList emailList;

  if (results && !results->empty()) {
    for (const contact::EmailAddressRow& emailAddress : *results) {
      emailList.push_back(GetEmail(emailAddress));
    }

    Respond(ArgumentList(
        vivaldi::contacts::GetAllEmailAddresses::Results::Create(emailList)));
  }
}

Profile* ContactAsyncFunction::GetProfile() const {
  return Profile::FromBrowserContext(browser_context());
}

ExtensionFunction::ResponseAction ContactsUpdateFunction::Run() {
  std::unique_ptr<vivaldi::contacts::Update::Params> params(
      vivaldi::contacts::Update::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  contact::ContactID contact_id;
  if (!GetIdAsInt64(params->id, &contact_id)) {
    return RespondNow(Error("Error. Invalid contact id"));
  }

  contact::Contact updated_contact;

  if (params->changes.name.get()) {
    updated_contact.name = base::UTF8ToUTF16(*params->changes.name);
    updated_contact.updateFields |= contact::NAME;
  }

  if (params->changes.birthday.get()) {
    updated_contact.birthday = GetTime(*params->changes.birthday);
    updated_contact.updateFields |= contact::BIRTHDAY;
  }

  if (params->changes.note.get()) {
    updated_contact.note = base::UTF8ToUTF16(*params->changes.note);
    updated_contact.updateFields |= contact::NOTE;
  }

  if (params->changes.avatar_url.get()) {
    updated_contact.avatar_url = base::UTF8ToUTF16(*params->changes.avatar_url);
    updated_contact.updateFields |= contact::AVATAR_URL;
  }

  if (params->changes.separator.get()) {
    updated_contact.separator = *params->changes.separator;
    updated_contact.updateFields |= contact::SEPARATOR;
  }

  if (params->changes.generated_from_sent_mail.get()) {
    updated_contact.generated_from_sent_mail =
        *params->changes.generated_from_sent_mail;
    updated_contact.updateFields |= contact::GENERATED_FROM_SENT_MAIL;
  }

  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->UpdateContact(
      contact_id, updated_contact,
      base::Bind(&ContactsUpdateFunction::UpdateContactComplete, this),
      &task_tracker_);

  return RespondLater();  // UpdateContactComplete() will be called
                          // asynchronously.
}

void ContactsUpdateFunction::UpdateContactComplete(
    std::shared_ptr<contact::ContactResults> results) {
  if (!results->success) {
    Respond(Error("Error updating contact"));
  } else {
    Contact ev = GetContact(results->contact);
    Respond(ArgumentList(
        extensions::vivaldi::contacts::Update::Results::Create(ev)));
  }
}

ExtensionFunction::ResponseAction ContactsDeleteFunction::Run() {
  std::unique_ptr<vivaldi::contacts::Delete::Params> params(
      vivaldi::contacts::Delete::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  contact::ContactID contact_id;
  if (!GetIdAsInt64(params->id, &contact_id)) {
    return RespondNow(Error("Error. Invalid contact id"));
  }

  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->DeleteContact(
      contact_id,
      base::Bind(&ContactsDeleteFunction::DeleteContactComplete, this),
      &task_tracker_);

  return RespondLater();  // DeleteContactComplete() will be called
                          // asynchronously.
}

void ContactsDeleteFunction::DeleteContactComplete(
    std::shared_ptr<contact::ContactResults> results) {
  if (!results->success) {
    Respond(Error("Error deleting contact"));
  } else {
    Respond(NoArguments());
  }
}

ExtensionFunction::ResponseAction ContactsCreateFunction::Run() {
  std::unique_ptr<vivaldi::contacts::Create::Params> params(
      vivaldi::contacts::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  contact::ContactRow createContact;

  if (params->contact.name.get()) {
    base::string16 name;
    name = base::UTF8ToUTF16(*params->contact.name);
    createContact.set_name(name);
  }

  if (params->contact.birthday.get()) {
    createContact.set_birthday(GetTime(*params->contact.birthday));
  }

  if (params->contact.note.get()) {
    base::string16 note;
    note = base::UTF8ToUTF16(*params->contact.note);
    createContact.set_note(note);
  }

  if (params->contact.avatar_url.get()) {
    base::string16 avatar_url;
    avatar_url = base::UTF8ToUTF16(*params->contact.avatar_url);
    createContact.set_avatar_url(avatar_url);
  }

  if (params->contact.separator.get()) {
    base::string16 avatar_url;
    avatar_url = base::UTF8ToUTF16(*params->contact.avatar_url);
    createContact.set_avatar_url(avatar_url);
  }

  if (params->contact.separator.get()) {
    createContact.set_separator(*params->contact.separator);
  }

  if (params->contact.generated_from_sent_mail.get()) {
    createContact.set_generated_from_sent_mail(
        *params->contact.generated_from_sent_mail);
  }

  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->CreateContact(
      createContact, base::Bind(&ContactsCreateFunction::CreateComplete, this),
      &task_tracker_);
  return RespondLater();
}

void ContactsCreateFunction::CreateComplete(
    std::shared_ptr<contact::ContactResults> results) {
  if (!results->success) {
    Respond(Error("Error creating contact"));
  } else {
    Contact ev = GetContact(results->contact);
    Respond(ArgumentList(
        extensions::vivaldi::contacts::Create::Results::Create(ev)));
  }
}

ExtensionFunction::ResponseAction ContactsAddPropertyItemFunction::Run() {
  std::unique_ptr<vivaldi::contacts::AddPropertyItem::Params> params(
      vivaldi::contacts::AddPropertyItem::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  contact::AddPropertyObject add_property;
  contact::ContactID contact_id;

  if (!GetIdAsInt64(params->property_to_add.contact_id, &contact_id)) {
    return RespondNow(Error("Error. Invalid contact id"));
  }

  base::string16 property_value =
      base::UTF8ToUTF16(params->property_to_add.property_value);
  add_property.contact_id = contact_id;
  add_property.value = property_value;
  add_property.property_name =
      APIAddpropertyTypeToInternal(params->property_to_add.property_name);

  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->AddProperty(
      add_property,
      base::Bind(&ContactsAddPropertyItemFunction::AddPropertyComplete, this),
      &task_tracker_);
  return RespondLater();
}

void ContactsAddPropertyItemFunction::AddPropertyComplete(
    std::shared_ptr<contact::ContactResults> results) {
  if (!results->success) {
    Respond(Error("Error adding property value"));
  } else {
    extensions::vivaldi::contacts::Contact ev = GetContact(results->contact);
    Respond(ArgumentList(
        extensions::vivaldi::contacts::AddPropertyItem::Results::Create(ev)));
  }
}

ExtensionFunction::ResponseAction ContactsUpdatePropertyItemFunction::Run() {
  std::unique_ptr<vivaldi::contacts::UpdatePropertyItem::Params> params(
      vivaldi::contacts::UpdatePropertyItem::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  contact::UpdatePropertyObject update_property;
  contact::ContactID contact_id;

  if (!GetIdAsInt64(params->property_to_update.contact_id, &contact_id)) {
    return RespondNow(Error("Error. Invalid contact id"));
  }

  update_property.contact_id = contact_id;
  update_property.property_name =
      APIAddpropertyTypeToInternal(params->property_to_update.property_name);

  update_property.value =
      base::UTF8ToUTF16(params->property_to_update.property_value);

  contact::PropertyID property_id;

  if (!GetIdAsInt64(params->property_to_update.property_id, &property_id)) {
    return RespondNow(Error("Error. Invalid property id"));
  }

  update_property.property_id = property_id;

  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->UpdateProperty(
      update_property,
      base::Bind(&ContactsUpdatePropertyItemFunction::UpdatePropertyComplete,
                 this),
      &task_tracker_);
  return RespondLater();
}

void ContactsUpdatePropertyItemFunction::UpdatePropertyComplete(
    std::shared_ptr<contact::ContactResults> results) {
  if (!results->success) {
    Respond(
        Error("Error updating property value or the property does not exist"));
  } else {
    extensions::vivaldi::contacts::Contact contact =
        GetContact(results->contact);
    Respond(ArgumentList(
        extensions::vivaldi::contacts::UpdatePropertyItem::Results::Create(
            contact)));
  }
}

ExtensionFunction::ResponseAction ContactsRemovePropertyItemFunction::Run() {
  std::unique_ptr<vivaldi::contacts::RemovePropertyItem::Params> params(
      vivaldi::contacts::RemovePropertyItem::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  contact::RemovePropertyObject remove_property;

  contact::ContactID contact_id;

  if (!GetIdAsInt64(params->property_to_remove.contact_id, &contact_id)) {
    return RespondNow(Error("Error. Invalid contact id"));
  }

  remove_property.contact_id = contact_id;
  remove_property.property_name =
      APIAddpropertyTypeToInternal(params->property_to_remove.property_name);

  int64_t id;

  if (!GetIdAsInt64(params->property_to_remove.property_id, &id)) {
    return RespondNow(Error("Error. Invalid property id"));
  }

  remove_property.property_id = id;

  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->RemoveProperty(
      remove_property,
      base::Bind(&ContactsRemovePropertyItemFunction::RemovePropertyComplete,
                 this),
      &task_tracker_);
  return RespondLater();
}

void ContactsRemovePropertyItemFunction::RemovePropertyComplete(
    std::shared_ptr<contact::ContactResults> results) {
  if (!results->success) {
    Respond(Error("Error removing property value"));
  } else {
    extensions::vivaldi::contacts::Contact contact =
        GetContact(results->contact);
    Respond(ArgumentList(
        extensions::vivaldi::contacts::RemovePropertyItem::Results::Create(
            contact)));
  }
}

ExtensionFunction::ResponseAction ContactsAddEmailAddressFunction::Run() {
  std::unique_ptr<vivaldi::contacts::AddEmailAddress::Params> params(
      vivaldi::contacts::AddEmailAddress::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  contact::EmailAddressRow add_email;
  contact::ContactID contact_id;

  if (!GetIdAsInt64(params->email_to_add.contact_id, &contact_id)) {
    return RespondNow(Error("Error. Invalid contact id"));
  }

  add_email.set_contact_id(contact_id);

  if (params->email_to_add.email_address.get()) {
    base::string16 email_address;
    email_address = base::UTF8ToUTF16(*params->email_to_add.email_address);
    add_email.set_email_address(email_address);
  }

  if (params->email_to_add.is_default.get()) {
    bool defaultEmailAddress = false;
    defaultEmailAddress = (*params->email_to_add.is_default);
    add_email.set_is_default(defaultEmailAddress);
  }

  if (params->email_to_add.trusted.get()) {
    bool isTrusted = false;
    isTrusted = (*params->email_to_add.trusted);
    add_email.set_trusted(isTrusted);
  }

  if (params->email_to_add.obsolete.get()) {
    bool isObsolete = false;
    isObsolete = (*params->email_to_add.obsolete);
    add_email.set_obsolete(isObsolete);
  }

  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->AddEmailAddress(
      add_email,
      base::Bind(&ContactsAddEmailAddressFunction::AddEmailAddressComplete,
                 this),
      &task_tracker_);
  return RespondLater();
}

void ContactsAddEmailAddressFunction::AddEmailAddressComplete(
    std::shared_ptr<contact::ContactResults> results) {
  if (!results->success) {
    Respond(Error("Error adding email address"));
  } else {
    extensions::vivaldi::contacts::Contact ev = GetContact(results->contact);
    Respond(ArgumentList(
        extensions::vivaldi::contacts::AddEmailAddress::Results::Create(ev)));
  }
}

ExtensionFunction::ResponseAction ContactsUpdateEmailAddressFunction::Run() {
  std::unique_ptr<vivaldi::contacts::UpdateEmailAddress::Params> params(
      vivaldi::contacts::UpdateEmailAddress::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  contact::EmailAddressRow updated_email;
  contact::ContactID contact_id;

  if (!GetIdAsInt64(params->email_to_update.contact_id, &contact_id)) {
    return RespondNow(Error("Error. Invalid contact id"));
  }

  updated_email.set_contact_id(contact_id);

  contact::EmailAddressID email_address_id;
  if (!GetIdAsInt64(params->email_address_id, &email_address_id)) {
    return RespondNow(Error("Error. Invalid email address id"));
  }

  updated_email.set_email_address_id(email_address_id);

  base::string16 email_address;
  email_address = base::UTF8ToUTF16(*params->email_to_update.email_address);
  updated_email.set_email_address(email_address);

  if (params->email_to_update.is_default.get()) {
    bool defaultEmailAddress = false;
    defaultEmailAddress = (*params->email_to_update.is_default);
    updated_email.set_is_default(defaultEmailAddress);
  }

  if (params->email_to_update.trusted.get()) {
    bool isTrusted = false;
    isTrusted = (*params->email_to_update.trusted);
    updated_email.set_trusted(isTrusted);
  }

  if (params->email_to_update.obsolete.get()) {
    bool isObsolete = false;
    isObsolete = (*params->email_to_update.obsolete);
    updated_email.set_obsolete(isObsolete);
  }

  ContactService* model = ContactServiceFactory::GetForProfile(GetProfile());

  model->UpdateEmailAddress(
      updated_email,
      base::Bind(
          &ContactsUpdateEmailAddressFunction::UpdateEmailAddressComplete,
          this),
      &task_tracker_);
  return RespondLater();
}

void ContactsUpdateEmailAddressFunction::UpdateEmailAddressComplete(
    std::shared_ptr<contact::ContactResults> results) {
  if (!results->success) {
    Respond(Error(
        "Error updating email address or the email address does not exist"));
  } else {
    extensions::vivaldi::contacts::Contact contact =
        GetContact(results->contact);
    Respond(ArgumentList(
        extensions::vivaldi::contacts::UpdateEmailAddress::Results::Create(
            contact)));
  }
}

}  //  namespace extensions
