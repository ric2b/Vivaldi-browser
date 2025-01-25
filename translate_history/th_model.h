// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved

#ifndef TRANSLATE_HISTORY_TH_MODEL_H_
#define TRANSLATE_HISTORY_TH_MODEL_H_

#include <memory>
#include <set>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"
#include "translate_history/th_node.h"

namespace content {
class BrowserContext;
}

namespace translate_history {
class TH_LoadDetails;
class TH_ModelObserver;
class TH_Storage;
}  // namespace translate_history

namespace translate_history {

class TH_Model : public KeyedService {
 public:
  TH_Model(content::BrowserContext* context, bool session_only);
  ~TH_Model() override;
  TH_Model(const TH_Model&) = delete;
  TH_Model& operator=(const TH_Model&) = delete;

  void Load();
  bool Save();
  void LoadFinished(std::unique_ptr<TH_LoadDetails> details);

  NodeList* list() { return list_.get(); }
  bool loaded() const { return loaded_; }
  bool session_only() const { return session_only_; }
  base::WeakPtr<TH_Model> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  void AddObserver(TH_ModelObserver* observer);
  void RemoveObserver(TH_ModelObserver* observer);

  const TH_Node* GetByContent(TH_Node* candidate);
  int GetIndex(const std::string& id);

  bool Add(std::unique_ptr<TH_Node> node, int index);
  bool Move(const std::string& id, int index);
  bool Remove(const std::vector<std::string>& ids);
  void Reset(double ms_since_epoch);

 private:
  std::unique_ptr<TH_LoadDetails> CreateLoadDetails();

  std::unique_ptr<NodeList> list_;
  const raw_ptr<content::BrowserContext> context_;
  std::unique_ptr<TH_Storage> store_;
  base::ObserverList<TH_ModelObserver> observers_;
  bool loaded_ = false;
  bool session_only_;
  base::WeakPtrFactory<TH_Model> weak_factory_{this};
};

}  // namespace translate_history

#endif  //  TRANSLATE_HISTORY_TH_MODEL_H_