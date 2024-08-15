// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved
#include "translate_history/th_model.h"

#include "content/public/browser/browser_context.h"
#include "translate_history/th_model_loader.h"
#include "translate_history/th_model_observer.h"
#include "translate_history/th_storage.h"

namespace translate_history {

TH_Model::TH_Model(content::BrowserContext* context, bool session_only)
    : context_(context), session_only_(session_only) {
  if (session_only_) {
    Load();  // Sets up an empty list and saves it.
  }
}

TH_Model::~TH_Model() {
  for (auto& observer : observers_)
    observer.TH_ModelBeingDeleted(this);
  if (store_) {
    store_->OnModelWillBeDeleted();
  }
}

std::unique_ptr<TH_LoadDetails> TH_Model::CreateLoadDetails() {
  NodeList* list = new NodeList;
  return base::WrapUnique(new TH_LoadDetails(list));
}

void TH_Model::Load() {
  store_.reset(new TH_Storage(context_, this));
  if (session_only_) {
    // Make an empty list and save it so that file content is wiped out.
    list_.reset(new NodeList);
    loaded_ = true;
    store_->SaveNow();
  } else {
    // ModelLoader schedules the load on a backend task runner.
    TH_ModelLoader::Create(
        context_->GetPath(), CreateLoadDetails(),
        base::BindOnce(&TH_Model::LoadFinished, AsWeakPtr()));
  }
}

bool TH_Model::Save() {
  if (store_ && !session_only_) {
    store_->ScheduleSave();
    return true;
  }
  return false;
}

void TH_Model::LoadFinished(std::unique_ptr<TH_LoadDetails> details) {
  DCHECK(details);

  std::unique_ptr<NodeList> list(details->release_list());
  list_ = std::move(list);
  loaded_ = true;

  for (auto& observer : observers_) {
    observer.TH_ModelLoaded(this);
  }
}

void TH_Model::AddObserver(TH_ModelObserver* observer) {
  observers_.AddObserver(observer);
}

void TH_Model::RemoveObserver(TH_ModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

const TH_Node* TH_Model::GetByContent(TH_Node* candidate) {
  NodeList& list = *list_;
  for (TH_Node* node : list) {
    if (node->src().code == candidate->src().code &&
        node->src().text == candidate->src().text &&
        node->translated().code == candidate->translated().code &&
        node->translated().text == candidate->translated().text) {
      return node;
    }
  }
  return nullptr;
}

int TH_Model::GetIndex(const std::string& id) {
  int index = 0;
  NodeList& list = *list_;
  for (TH_Node* node : list) {
    if (node->id() == id) {
      return index;
    }
    index++;
  }
  return -1;
}

bool TH_Model::Add(std::unique_ptr<TH_Node> node, int index) {
  if (index == -1) {
    index = list_->size();
  }
  if (index < 0 || static_cast<unsigned long>(index) > list_->size()) {
    return false;
  }

  list_->insert(list_->begin() + index, node.release());

  Save();

  for (auto& observer : observers_) {
    observer.TH_ModelElementAdded(this, index);
  }

  return true;
}

bool TH_Model::Move(const std::string& id, int index) {
  if (index == -1) {
    index = list_->size();
  }
  if (index < 0 || static_cast<size_t>(index) > list_->size()) {
    return false;
  }
  int from = GetIndex(id);
  if (from < 0 || static_cast<size_t>(from) > list_->size()) {
    return false;
  }

  if (from == index) {
    return true;
  }

  const TH_Node* node = list_->at(from);
  std::unique_ptr<TH_Node> copy = std::make_unique<TH_Node>(node->id());
  copy->src() = node->src();
  copy->translated() = node->translated();

  if (from < index) {
    index--;
  }
  list_->erase(list_->begin() + from);
  list_->insert(list_->begin() + index, copy.release());

  Save();

  for (auto& observer : observers_) {
    observer.TH_ModelElementMoved(this, index);
  }

  return true;
}

bool TH_Model::Remove(const std::vector<std::string>& ids) {
  unsigned long num_match = 0;

  NodeList& list = *list_;
  for (const std::string& id : ids) {
    for (unsigned long i = 0; i < list.size(); i++) {
      if (list[i]->id() == id) {
        num_match++;
        break;
      }
    }
  }
  if (num_match != ids.size()) {
    return false;
  }

  for (const std::string& id : ids) {
    for (unsigned long i = 0; i < list.size(); i++) {
      if (list[i]->id() == id) {
        list.erase(list.begin() + i);
        break;
      }
    }
  }

  Save();

  for (auto& observer : observers_) {
    observer.TH_ModelElementsRemoved(this, ids);
  }
  return true;
}

void TH_Model::Reset(double ms_since_epoch) {
  base::Time remove_since =
      base::Time::FromMillisecondsSinceUnixEpoch(ms_since_epoch);

  if (ms_since_epoch == 0) {
    if (list_->size() > 0) {
      list_->clear();

      Save();

      for (auto& observer : observers_) {
        observer.TH_ModelElementsRemoved(this, std::vector<std::string>());
      }
    }
  } else {
    std::vector<std::string> ids;
    NodeList& list = *list_;
    for (unsigned long i = 0; i < list.size(); i++) {
      if (list[i]->date_added() >= remove_since) {
        ids.push_back(list[i]->id());
      }
    }
    if (ids.size() > 0) {
      if (ids.size() == list.size()) {
        Reset(0);  // A bit more efficient.
      } else {
        Remove(ids);
      }
    }
  }
}

}  // namespace translate_history
