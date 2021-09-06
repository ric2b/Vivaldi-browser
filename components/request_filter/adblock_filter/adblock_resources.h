// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/values.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace base {
class SequencedTaskRunner;
}

namespace adblock_filter {
class Resources {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    virtual void OnResourcesLoaded() {}
  };

  explicit Resources(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~Resources();

  base::WeakPtr<Resources> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  base::Optional<std::string> GetRedirect(
      const std::string& name,
      flat::ResourceType resource_type) const;

  std::map<std::string, base::StringPiece> GetInjections();

  bool loaded() const {
    return redirectable_resources_.is_dict() && injectable_resources_.is_dict();
  }

 private:
  void OnLoadFinished(base::Value* destination,
                      std::unique_ptr<base::Value> resources);

  base::Value redirectable_resources_;
  base::Value injectable_resources_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<Resources> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Resources);
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_
