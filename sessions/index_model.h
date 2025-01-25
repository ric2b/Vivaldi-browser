// Copyright (c) 2023 Vivaldi Technologies AS. All rights reserved

#ifndef SESSIONS_INDEX_MODEL_H_
#define SESSIONS_INDEX_MODEL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "sessions/index_model_observer.h"
#include "sessions/index_node.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace sessions {

class IndexLoadDetails;
class IndexStorage;

class Index_Model : public KeyedService {
 public:
  explicit Index_Model(content::BrowserContext* context);
  ~Index_Model() override;
  Index_Model(const Index_Model&) = delete;
  Index_Model& operator=(const Index_Model&) = delete;

  void Load();
  bool Save();
  void LoadFinished(std::unique_ptr<IndexLoadDetails> details);

  bool Move(const Index_Node* node, const Index_Node* parent, size_t index);
  Index_Node* Add(std::unique_ptr<Index_Node> node, Index_Node* parent,
                 size_t index, std::string owner = "");
  bool SetTitle(Index_Node* node, const std::u16string& title);
  bool Change(Index_Node* node, Index_Node* from);
  bool Swap(Index_Node* node_a, Index_Node* node_b);
  bool Remove(Index_Node* node);

  bool loaded() const { return loaded_; }
  bool loadingFailed() const { return loading_failed_; }
  bool IsValidIndex(const Index_Node* parent, size_t index);
  bool IsTrashed(Index_Node* node);

  void AddObserver(IndexModelObserver* observer);
  void RemoveObserver(IndexModelObserver* observer);

  // Returns the fixed node that is the ancestor of all other.
  Index_Node* root_node() { return &root_; }
  // Returns the fixed node that is the ancestor regular sessions.
  Index_Node* items_node() { return items_node_; }
  // Returns the fixed node that holds the timed session backup. Note: The
  // returned object must never be saved for later use. Index_Model.Remove() can
  // invalidate it any any time.
  Index_Node* backup_node() { return backup_node_; }
  // Returns the fixed node that holds saved persistent tabs.
  Index_Node* persistent_node() { return persistent_node_; }

  content::BrowserContext* browser_context() { return context_; }

 private:
  std::unique_ptr<IndexLoadDetails> CreateLoadDetails();
  bool loaded_ = false;
  bool loading_failed_ = false;
  base::ObserverList<IndexModelObserver> observers_;
  const raw_ptr<content::BrowserContext> context_;
  std::unique_ptr<IndexStorage> store_;
  Index_Node root_;
  // Managed by the root node. Provides easy access.
  raw_ptr<Index_Node> items_node_ = nullptr;
  // Managed by the root node. Provides easy access.
  // DisableDanglingPtrDetection is needed because of backup_node().
  raw_ptr<Index_Node, DisableDanglingPtrDetection> backup_node_ = nullptr;
  // Managed by the root node. Provides easy access.
  raw_ptr<Index_Node> persistent_node_ = nullptr;
};

}  // namespace sessions

#endif  // SESSIONS_SESSION_INDEX_H_
