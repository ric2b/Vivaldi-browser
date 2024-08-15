// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_

#include <map>
#include <memory>
#include <string>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/values.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_IOS)
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"
#endif  // !IS_IOS

namespace base {
class SequencedTaskRunner;
}

namespace adblock_filter {
class Resources {
 public:
  struct InjectableResource {
    const std::string_view code;
    const bool use_main_world;
  };

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnResourcesLoaded() {}

   protected:
    Observer() = default;
  };

  explicit Resources(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~Resources();
  Resources(const Resources&) = delete;
  Resources& operator=(const Resources&) = delete;

  base::WeakPtr<Resources> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

#if !BUILDFLAG(IS_IOS)
  std::optional<std::string> GetRedirect(
      const std::string& name,
      flat::ResourceType resource_type) const;
#endif  // !IS_IOS

  std::map<std::string, InjectableResource> GetInjections();

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
};

}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_
