// Copyright (c) 2019 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_
#define COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "vivaldi/components/request_filter/adblock_filter/flat/adblock_rules_list_generated.h"

namespace base {
class SequencedTaskRunner;
}

namespace adblock_filter {
class Resources {
 public:
  explicit Resources(scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~Resources();

  base::WeakPtr<Resources> AsWeakPtr() { return weak_factory_.GetWeakPtr(); }

  base::Optional<std::string> Get(const std::string& name,
                                  flat::ResourceType resource_type) const;

 private:
  void OnLoadFinished(std::unique_ptr<base::Value> resources);
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::unique_ptr<base::Value> resources_;

  base::WeakPtrFactory<Resources> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(Resources);
};
}  // namespace adblock_filter

#endif  // COMPONENTS_REQUEST_FILTER_ADBLOCK_FILTER_ADBLOCK_RESOURCES_H_
