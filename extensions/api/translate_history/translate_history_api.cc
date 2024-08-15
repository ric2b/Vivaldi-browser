//
// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.
//
#include "extensions/api/translate_history/translate_history_api.h"

#include "base/lazy_instance.h"
#include "base/uuid.h"
#include "extensions/tools/vivaldi_tools.h"
#include "translate_history/th_model.h"
#include "translate_history/th_service_factory.h"

namespace extensions {

using content::BrowserContext;
using translate_history::NodeList;
using translate_history::TH_Model;
using translate_history::TH_Node;
using translate_history::TH_ServiceFactory;
using vivaldi::translate_history::HistoryItem;

static HistoryItem MakeHistoryApiItem(TH_Node* node) {
  HistoryItem item;
  item.id = node->id();
  item.src_item.code = node->src().code;
  item.src_item.text = node->src().text;
  item.translated_item.code = node->translated().code;
  item.translated_item.text = node->translated().text;
  return item;
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<TranslateHistoryAPI>>::
    DestructorAtExit g_translate_history = LAZY_INSTANCE_INITIALIZER;
// static
BrowserContextKeyedAPIFactory<TranslateHistoryAPI>*
TranslateHistoryAPI::GetFactoryInstance() {
  return g_translate_history.Pointer();
}

TranslateHistoryAPI::TranslateHistoryAPI(BrowserContext* browser_context)
    : browser_context_(browser_context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(
      this, vivaldi::translate_history::OnAdded::kEventName);
  event_router->RegisterObserver(
      this, vivaldi::translate_history::OnRemoved::kEventName);
}

void TranslateHistoryAPI::Shutdown() {
  if (model_) {
    model_->RemoveObserver(this);
    model_ = nullptr;
  }
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void TranslateHistoryAPI::OnListenerAdded(const EventListenerInfo& details) {
  DCHECK(!model_);
  model_ = TH_ServiceFactory::GetForBrowserContext(browser_context_);
  model_->AddObserver(this);

  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

void TranslateHistoryAPI::TH_ModelElementAdded(TH_Model* model, int index) {
  TH_Node* node = (*(model->list()))[index];

  std::vector<HistoryItem> list;
  list.push_back(MakeHistoryApiItem(node));

  ::vivaldi::BroadcastEvent(
      vivaldi::translate_history::OnAdded::kEventName,
      vivaldi::translate_history::OnAdded::Create(list, index),
      browser_context_);
}

void TranslateHistoryAPI::TH_ModelElementMoved(TH_Model* model, int index) {
  TH_Node* node = (*(model->list()))[index];
  ::vivaldi::BroadcastEvent(
      vivaldi::translate_history::OnMoved::kEventName,
      vivaldi::translate_history::OnMoved::Create(node->id(), index),
      browser_context_);
}

void TranslateHistoryAPI::TH_ModelElementsRemoved(
    TH_Model* model,
    const std::vector<std::string>& ids) {
  ::vivaldi::BroadcastEvent(vivaldi::translate_history::OnRemoved::kEventName,
                            vivaldi::translate_history::OnRemoved::Create(ids),
                            browser_context_);
}

ExtensionFunction::ResponseAction TranslateHistoryFunction::Run() {
  TH_Model* model = TH_ServiceFactory::GetForBrowserContext(browser_context());
  if (!model) {
    Respond(Error("Failed to create model"));
  } else if (model->loaded()) {
    return RespondNow(RunWithModel(model));
  } else {
    AddRef();  // Balanced in MenuModelLoaded().
    model->AddObserver(this);
    model->Load();
    return RespondLater();
  }
  return AlreadyResponded();
}

void TranslateHistoryFunction::TH_ModelLoaded(TH_Model* model) {
  model->RemoveObserver(this);

  if (!model->loaded()) {
    Respond(Error("Failed to load model"));
  } else {
    Respond(RunWithModel(model));
  }

  Release();  // Balanced in Run().
}

ExtensionFunction::ResponseValue TranslateHistoryGetFunction::RunWithModel(
    translate_history::TH_Model* model) {
  std::vector<HistoryItem> entries;

  NodeList& list = *model->list();
  for (TH_Node* node : list) {
    entries.push_back(MakeHistoryApiItem(node));
  }

  return ArgumentList(
      vivaldi::translate_history::Get::Results::Create(entries));
}

ExtensionFunction::ResponseValue TranslateHistoryAddFunction::RunWithModel(
    translate_history::TH_Model* model) {
  std::optional<vivaldi::translate_history::Add::Params> params(
      vivaldi::translate_history::Add::Params::Create(args()));
  HistoryItem& item = params->item;

  // Assign id to parameter item as that is returned in the call.
  item.id = base::Uuid::GenerateRandomV4().AsLowercaseString();
  std::unique_ptr<TH_Node> node = std::make_unique<TH_Node>(item.id);
  node->src().code = item.src_item.code;
  node->src().text = item.src_item.text;
  node->translated().code = item.translated_item.code;
  node->translated().text = item.translated_item.text;

  const TH_Node* existingNode = model->GetByContent(node.get());
  if (existingNode) {
    // Prevent duplicate. Just return with the existing node's id.
    item.id = existingNode->id();
    // And move the node.
    if (model->Move(existingNode->id(), params->index)) {
      return ArgumentList(
          vivaldi::translate_history::Add::Results::Create(item));
    } else {
      return Error("Item not added. Failed to move existing element");
    }
  } else {
    if (model->Add(std::move(node), params->index)) {
      return ArgumentList(
          vivaldi::translate_history::Add::Results::Create(item));
    } else {
      return Error("Item not added. Index out of bounds");
    }
  }
}

ExtensionFunction::ResponseAction TranslateHistoryRemoveFunction::Run() {
  TH_Model* model = TH_ServiceFactory::GetForBrowserContext(browser_context());
  if (model && model->loaded()) {
    std::optional<vivaldi::translate_history::Remove::Params> params(
        vivaldi::translate_history::Remove::Params::Create(args()));

    if (params->ids.size() == 0) {
      Respond(ArgumentList(
          vivaldi::translate_history::Remove::Results::Create(false)));
    }
    if (model->Remove(params->ids)) {
      Respond(ArgumentList(
          vivaldi::translate_history::Remove::Results::Create(true)));
    } else {
      Respond(Error("Item(s) not removed. Unknown id(s)"));
    }
  } else {
    Respond(Error("Model is missing"));
  }
  return AlreadyResponded();
}

ExtensionFunction::ResponseAction TranslateHistoryResetFunction::Run() {
  TH_Model* model = TH_ServiceFactory::GetForBrowserContext(browser_context());
  if (model && model->loaded()) {
    std::optional<vivaldi::translate_history::Reset::Params> params(
        vivaldi::translate_history::Reset::Params::Create(args()));
    model->Reset(params->since);
    Respond(
        ArgumentList(vivaldi::translate_history::Reset::Results::Create(true)));
  } else {
    Respond(Error("Model is missing"));
  }
  return AlreadyResponded();
}

}  // namespace extensions