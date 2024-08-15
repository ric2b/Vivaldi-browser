// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#include "extensions/api/reading_list/reading_list_api.h"

#include "chrome/browser/profiles/profile.h"

#include "extensions/schema/reading_list_private.h"
#include "extensions/tools/vivaldi_tools.h"

namespace extensions {
// static
ReadingListPrivateAPI* ReadingListPrivateAPI::FromBrowserContext(
    content::BrowserContext* browser_context) {
  ReadingListPrivateAPI* api = GetFactoryInstance()->Get(browser_context);
  DCHECK(api);
  return api;
}

// static
BrowserContextKeyedAPIFactory<ReadingListPrivateAPI>*
ReadingListPrivateAPI::GetFactoryInstance() {
  static base::NoDestructor<BrowserContextKeyedAPIFactory<ReadingListPrivateAPI>>
      instance;
  return instance.get();
}

ReadingListPrivateAPI::ReadingListPrivateAPI(content::BrowserContext* context)
    : profile_(Profile::FromBrowserContext(context)) {

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(profile_);
  reading_list_model_scoped_observation_.Observe(model);
}

ReadingListPrivateAPI::~ReadingListPrivateAPI() {}


// ReadingListModelObserver
void ReadingListPrivateAPI::ReadingListModelLoaded(
    const ReadingListModel* model) {
}

void ReadingListPrivateAPI::ReadingListDidApplyChanges(
    ReadingListModel* model) {
  ::vivaldi::BroadcastEvent(
      vivaldi::reading_list_private::OnModelChanged::kEventName,
      vivaldi::reading_list_private::OnModelChanged::Create(), profile_);
}

ExtensionFunction::ResponseAction ReadingListPrivateAddFunction::Run() {
  using vivaldi::reading_list_private::Add::Params;
  namespace Results = vivaldi::reading_list_private::Add::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url(params->url);

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser_context());

  DCHECK(model->IsUrlSupported(url));
  bool success = false;
  if (model->IsUrlSupported(url) && model->loaded()) {
    model->AddOrReplaceEntry(url, params->title,
        reading_list::ADDED_VIA_CURRENT_APP, {});
    success = true;
  }
  return RespondNow(ArgumentList(Results::Create(success)));
}

ExtensionFunction::ResponseAction ReadingListPrivateRemoveFunction::Run() {
  using vivaldi::reading_list_private::Remove::Params;
  namespace Results = vivaldi::reading_list_private::Remove::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url(params->url);

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser_context());

  DCHECK(model->IsUrlSupported(url));
  bool success = false;
  if (model->IsUrlSupported(url) && model->loaded()) {
    scoped_refptr<const ReadingListEntry> entry = model->GetEntryByURL(url);
    DCHECK(entry);
    success = entry != nullptr;
    if (entry) {
      model->RemoveEntryByURL(url, FROM_HERE);
    }
  }
  return RespondNow(ArgumentList(Results::Create(success)));
}

namespace {
vivaldi::reading_list_private::ReadingListEntry GetEntryData(
    scoped_refptr<const ReadingListEntry> entry) {
  vivaldi::reading_list_private::ReadingListEntry entry_data;

  entry_data.title = entry->Title();
  entry_data.url = entry->URL().spec();
  entry_data.last_update = entry->UpdateTime();
  entry_data.read = entry->IsRead();

  return entry_data;
}
}  // namespace


ExtensionFunction::ResponseAction ReadingListPrivateGetAllFunction::Run() {
  namespace Results = vivaldi::reading_list_private::GetAll::Results;

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser_context());

  std::vector<vivaldi::reading_list_private::ReadingListEntry> entries;

  for (const auto& url : model->GetKeys()) {
    scoped_refptr<const ReadingListEntry> entry = model->GetEntryByURL(url);
    DCHECK(entry);
    entries.push_back(GetEntryData(entry));
  }
  return RespondNow(ArgumentList(Results::Create(entries)));
}

ExtensionFunction::ResponseAction ReadingListPrivateSetReadStatusFunction::Run() {
  using vivaldi::reading_list_private::SetReadStatus::Params;
  namespace Results = vivaldi::reading_list_private::SetReadStatus::Results;

  std::optional<Params> params = Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  GURL url(params->url);

  ReadingListModel* model =
      ReadingListModelFactory::GetForBrowserContext(browser_context());

  DCHECK(model->IsUrlSupported(url));

  bool success = false;
  if (model->IsUrlSupported(url) && model->loaded()) {
    model->SetReadStatusIfExists(url, params->read);
    success = true;
  }
  return RespondNow(ArgumentList(Results::Create(success)));
}

}  // namespace extensions
