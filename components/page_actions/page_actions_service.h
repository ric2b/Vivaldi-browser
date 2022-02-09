// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_H_
#define COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_H_

#include <string>

#include "base/files/file_path.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class WebContents;
}

namespace page_actions {
class Service : public KeyedService {
 public:
  enum ScriptOverride {
    kNoOverride = 0,
    kEnabledOverride = 1,
    kDisabledOverride = 2
  };

  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;
    virtual void OnServiceLoaded(Service* service) {}
    virtual void OnScriptPathsChanged() {}
    virtual void OnScriptOverridesChanged(content::WebContents* tab_contents,
                                          const base::FilePath& script_path,
                                          ScriptOverride script_override) {}
  };

  Service();
  ~Service() override;

  virtual bool IsLoaded() = 0;

  virtual bool AddPath(const base::FilePath& path) = 0;
  virtual bool UpdatePath(const base::FilePath& old_path,
                          const base::FilePath& new_path) = 0;
  virtual bool RemovePath(const base::FilePath& path) = 0;

  // Get all valid page action script paths. Those paths are used as key for
  // manipulating settings related to the scripts themselves
  virtual std::vector<base::FilePath> GetAllScriptPaths() = 0;

  // Forces a script to be always enabled or disabled on agiven tab, regardless
  // of what include/exclude url rules say.
  virtual bool SetScriptOverrideForTab(content::WebContents* tab_contents,
                                       const base::FilePath& script_path,
                                       ScriptOverride script_override) = 0;
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};
}  // namespace page_actions

#endif  // COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_H_
