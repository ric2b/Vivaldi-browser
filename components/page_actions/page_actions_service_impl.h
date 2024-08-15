// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_IMPL_H_
#define COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_IMPL_H_

#include <map>
#include <string>

#include "base/memory/weak_ptr.h"
#include "components/content_injection/content_injection_provider.h"
#include "components/page_actions/page_actions_directory_watcher.h"
#include "components/page_actions/page_actions_service.h"
#include "components/page_actions/page_actions_types.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace page_actions {
class ServiceImpl : public Service, public content_injection::Provider {
 public:
  ServiceImpl(content::BrowserContext* browser_context);
  ~ServiceImpl() override;
  ServiceImpl(const ServiceImpl&) = delete;
  ServiceImpl& operator=(const ServiceImpl&) = delete;

  void Load();

  // Implementing Service
  bool IsLoaded() override;
  bool AddPath(const base::FilePath& path) override;
  bool UpdatePath(const base::FilePath& old_path,
                  const base::FilePath& new_path) override;
  bool RemovePath(const base::FilePath& path) override;
  std::vector<base::FilePath> GetAllScriptPaths() override;
  bool SetScriptOverrideForTab(content::WebContents* tab_contents,
                               const base::FilePath& script_path,
                               ScriptOverride script_override) override;
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  // Implementing content_injection::Provider
  content_injection::mojom::InjectionsForFramePtr GetInjectionsForFrame(
      const GURL& url,
      content::RenderFrameHost* frame) override;
  const std::map<std::string, content_injection::StaticInjectionItem>&
  GetStaticContent() override;

 private:
  class RestoreInfo : public content::WebContentsObserver {
   public:
    RestoreInfo(content::WebContents* web_contents);
    ~RestoreInfo() override;
    RestoreInfo(const RestoreInfo& other) = delete;
    RestoreInfo& operator=(const RestoreInfo& other) = delete;

    void Add(const base::FilePath& script_path,
             page_actions::Service::ScriptOverride script_override);
    const std::map<base::FilePath, page_actions::Service::ScriptOverride>&
    script_overrides() const {
      return script_overrides_;
    }

    // Implementing content::WebContentsObserver;
    void WebContentsDestroyed() override;

   private:
    std::map<base::FilePath, page_actions::Service::ScriptOverride>
        script_overrides_;
  };

  void OnFilesUpdated(DirectoryWatcher::UpdatedFileContents updated_contents,
                      std::vector<base::FilePath> invalid_paths);

  void RestoreOverrides();
  bool DoSetScriptOverrideForTab(content::WebContents* tab_contents,
                                 const base::FilePath& script_path,
                                 ScriptOverride script_override);

  void RebuildStaticInjections();

  const raw_ptr<content::BrowserContext> browser_context_;
  std::map<base::FilePath, ScriptDirectory> directories_;

  std::map<content::WebContents*, RestoreInfo> script_overrides_to_restore_;

  std::unique_ptr<DirectoryWatcher, DirectoryWatcher::Deleter>
      directory_watcher_;
  std::map<std::string, content_injection::StaticInjectionItem>
      static_injections_;
  std::optional<int> javascript_world_id_;

  base::ObserverList<Observer> observers_;

  base::WeakPtrFactory<ServiceImpl> weak_factory_{this};
};
}  // namespace page_actions

#endif  // COMPONENTS_PAGE_ACTIONS_PAGE_ACTIONS_SERVICE_IMPL_H_
