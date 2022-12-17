// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/page_actions/page_actions_service_impl.h"

#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "components/content_injection/content_injection_service.h"
#include "components/content_injection/content_injection_service_factory.h"
#include "components/page_actions/page_actions_tab_helper.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace page_actions {
namespace {
constexpr char kContentInjectionPrefix[] = "page_action:";
constexpr char kJavascriptWorldStableID[] = "page_actions";
constexpr char kJavascriptWorldName[] = "Vivaldi Page Actions";

const base::FilePath::CharType kCSSExtension[] = FILE_PATH_LITERAL(".css");
const base::FilePath::CharType kJSExtension[] = FILE_PATH_LITERAL(".js");

#if BUILDFLAG(IS_ANDROID)
const base::FilePath::CharType kBuiltInPageActionsPath[] =
    FILE_PATH_LITERAL("assets/user_files.json");
#else
const base::FilePath::CharType kBuiltInPageActionsPath[] =
    FILE_PATH_LITERAL("vivaldi/user_files");
#endif

void ChangeStaticInjectionCallback(
    content::WebContents* tab_contents,
    mojo::Remote<content_injection::mojom::FrameHandler> frame_handler,
    bool result) {
  if (result)
    return;

  // Injection failed. Reload.
  tab_contents->GetController().Reload(content::ReloadType::NORMAL, true);
}

void ChangeStaticInjectionForFrame(content::WebContents* tab_contents,
                                   content::RenderFrameHost* frame,
                                   base::FilePath script_path,
                                   bool enable) {
  if (!frame->IsRenderFrameLive()) {
    // Will happen when restoring a tab. Current injections will be kept in
    // TabHelper.
    return;
  }
  mojo::Remote<content_injection::mojom::FrameHandler> frame_handler;
  frame->GetRemoteInterfaces()->GetInterface(
      frame_handler.BindNewPipeAndPassReceiver());
  DCHECK(frame_handler);
  std::string key(kContentInjectionPrefix);
  key.append(script_path.AsUTF8Unsafe());

  if (enable) {
    auto enabled_static_injection =
        content_injection::mojom::EnabledStaticInjection::New();
    enabled_static_injection->key = std::move(key);
    frame_handler->EnableStaticInjection(
        std::move(enabled_static_injection),
        base::BindOnce(&ChangeStaticInjectionCallback,
                       base::Unretained(tab_contents),
                       std::move(frame_handler)));
  } else {
    frame_handler->DisableStaticInjection(
        std::move(key), base::BindOnce(&ChangeStaticInjectionCallback,
                                       base::Unretained(tab_contents),
                                       std::move(frame_handler)));
  }
}
}  // namespace

ServiceImpl::ServiceImpl(content::BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ServiceImpl::~ServiceImpl() = default;

void ServiceImpl::Load() {
  base::FilePath assets_path;
#if BUILDFLAG(IS_ANDROID)
  assets_path = base::FilePath(kBuiltInPageActionsPath);
  directories_.emplace(
      std::make_pair(assets_path.DirName(), ScriptDirectory()));
#endif

  // Can't use make_unique here, because of the custom deleter
  directory_watcher_.reset(
      new DirectoryWatcher(base::BindRepeating(&ServiceImpl::OnFilesUpdated,
                                               weak_factory_.GetWeakPtr()),
                           assets_path));

#if !BUILDFLAG(IS_ANDROID)
  base::FilePath built_in_path;
  base::PathService::Get(chrome::DIR_RESOURCES, &built_in_path);
  AddPath(
      built_in_path.Append(kBuiltInPageActionsPath).NormalizePathSeparators());
#else
  directory_watcher_->AddPaths({});
#endif
}

bool ServiceImpl::IsLoaded() {
  return javascript_world_id_.has_value();
}

bool ServiceImpl::AddPath(const base::FilePath& path) {
  if (!path.IsAbsolute())
    return false;

  auto result = directories_.emplace(std::make_pair(path, ScriptDirectory()));
  if (!result.second)
    return false;
  directory_watcher_->AddPaths({path});
  return true;
}

bool ServiceImpl::UpdatePath(const base::FilePath& old_path,
                             const base::FilePath& new_path) {
  auto script_directory = directories_.find(old_path);
  if (script_directory == directories_.end())
    return false;

  auto emplace_result =
      directories_.emplace(std::make_pair(new_path, ScriptDirectory()));
  if (!emplace_result.second)
    return false;

  std::swap(emplace_result.first->second, script_directory->second);

  directory_watcher_->RemovePath(old_path);
  directory_watcher_->AddPaths({new_path});

  return true;
}

bool ServiceImpl::RemovePath(const base::FilePath& path) {
  bool result = directories_.erase(path) != 0;
  if (result)
    directory_watcher_->RemovePath(path);
  return result;
}

std::vector<base::FilePath> ServiceImpl::GetAllScriptPaths() {
  std::vector<base::FilePath> result;
  for (const auto& script_directory : directories_) {
    if (!script_directory.second.valid)
      continue;
    for (const auto& script_file : script_directory.second.script_files) {
      if (script_file.second.content.empty())
        continue;

      result.push_back(script_file.first);
    }
  }

  return result;
}

bool ServiceImpl::SetScriptOverrideForTab(content::WebContents* tab_contents,
                                          const base::FilePath& script_path,
                                          ScriptOverride script_override) {
  // We allow this to be called early, mainly to allow restoring overrides
  // from sessions, which typically happen before we are fully ready. The
  // requested overrides are temporarily stored until loading is done and then
  // properly applied.
  if (!IsLoaded()) {
    auto inserted = script_overrides_to_restore_.emplace(
        std::piecewise_construct, std::forward_as_tuple(tab_contents),
        std::forward_as_tuple(tab_contents));
    // Insertion might fail, but we end up with the item we want to change here
    // either way.
    inserted.first->second.Add(script_path, script_override);
    return true;
  }
  return DoSetScriptOverrideForTab(tab_contents, script_path, script_override);
}

bool ServiceImpl::DoSetScriptOverrideForTab(content::WebContents* tab_contents,
                                            const base::FilePath& script_path,
                                            ScriptOverride script_override) {
  const auto& script_directory = directories_.find(script_path.DirName());
  if (script_directory == directories_.end() || !script_directory->second.valid)
    return false;
  const ScriptFiles& script_files = script_directory->second.script_files;
  const auto& script_file = script_files.find(script_path);
  if (script_file == script_files.end() || script_file->second.content.empty())
    return false;

  TabHelper::CreateForWebContents(tab_contents);
  TabHelper* tab_helper = TabHelper::FromWebContents(tab_contents);
  const auto& old_override = tab_helper->GetScriptOverrides().find(script_path);
  bool was_enabled = old_override != tab_helper->GetScriptOverrides().end() &&
                     old_override->second;
  if (!was_enabled && script_override == Service::kEnabledOverride) {
    if (script_path.MatchesExtension(kJSExtension)) {
      ChangeStaticInjectionForFrame(
          tab_contents, tab_contents->GetPrimaryMainFrame(),
                                    script_path, true);
    } else {
      tab_contents->ForEachRenderFrameHost(
          [tab_contents, script_path](content::RenderFrameHost* rfh) {
            ChangeStaticInjectionForFrame(tab_contents, rfh, script_path, true);
          });
    }
  } else if (was_enabled && script_override != Service::kEnabledOverride) {
    if (script_path.MatchesExtension(kJSExtension)) {
      // Can't unload injected Javascript.
      tab_contents->GetController().Reload(content::ReloadType::NORMAL, true);
    } else {
      tab_contents->ForEachRenderFrameHost(
          [tab_contents, script_path](content::RenderFrameHost* rfh) {
            ChangeStaticInjectionForFrame(tab_contents, rfh, script_path,
                                          false);
          });
    }
  }

  switch (script_override) {
    case Service::kNoOverride:
      tab_helper->RemoveScriptOverride(script_path);
      break;
    case Service::kEnabledOverride:
      tab_helper->SetScriptOverride(script_path, true);
      break;
    case Service::kDisabledOverride:
      tab_helper->SetScriptOverride(script_path, false);
      break;
  }

  for (Observer& observer : observers_)
    observer.OnScriptOverridesChanged(tab_contents, script_path,
                                      script_override);
  return true;
}

void ServiceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}
void ServiceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ServiceImpl::OnFilesUpdated(
    DirectoryWatcher::UpdatedFileContents updated_contents,
    std::vector<base::FilePath> invalid_paths) {
  for (auto& content_path : updated_contents) {
    auto script_directory = directories_.find(content_path.first);
    DCHECK(script_directory != directories_.end());

    script_directory->second.valid = true;

    for (auto& updated_content : content_path.second) {
      ScriptFiles& script_files = script_directory->second.script_files;
      // Creates a script item if there isn't one there already.
      auto script = script_files.emplace(
          std::make_pair(updated_content.first, ScriptFile()));
      // If a script was removed, we keep all match patterns we had for it, but
      // we make sure to mark it as disabled.
      if (updated_content.second.empty())
        script.first->second.active = false;
      script.first->second.content = std::move(updated_content.second);
    }
  }

  for (const auto& invalid_path : invalid_paths) {
    auto script_directory = directories_.find(invalid_path);
    DCHECK(script_directory != directories_.end());

    // We keep the activation metadata for invalid script directories, in case
    // they've just been moved and will be made valid again with a call to
    // UpdatePath
    script_directory->second.valid = false;
  }

  content_injection::Service* content_injection_service =
      content_injection::ServiceFactory::GetInstance()->GetForBrowserContext(
          browser_context_);
  if (IsLoaded()) {
    RebuildStaticInjections();
    content_injection_service->OnStaticContentChanged();
    for (Observer& observer : observers_)
      observer.OnScriptPathsChanged();
  } else {
    // We delay registering until the first file update, since it that is when
    // script contents gets populated for the first time.
    content_injection::mojom::JavascriptWorldInfoPtr world_info =
        content_injection::mojom::JavascriptWorldInfo::New();
    world_info->stable_id = kJavascriptWorldStableID;
    world_info->name = kJavascriptWorldName;
    javascript_world_id_ =
        content_injection_service->RegisterWorldForJSInjection(
            std::move(world_info));
    // If we don't have a world ID, we won't be able to inject Javascript. So,
    // we abort registration.
    if (!javascript_world_id_)
      return;

    RebuildStaticInjections();
    content_injection_service->AddProvider(this);
    RestoreOverrides();
    for (Observer& observer : observers_)
      observer.OnServiceLoaded(this);
  }
}

void ServiceImpl::RebuildStaticInjections() {
  DCHECK(IsLoaded());
  static_injections_.clear();

  for (const auto& script_directory : directories_) {
    if (!script_directory.second.valid)
      continue;
    for (const auto& script_file : script_directory.second.script_files) {
      if (script_file.second.content.empty())
        continue;

      std::string item_key(kContentInjectionPrefix);
      item_key.append(script_file.first.AsUTF8Unsafe());
      auto emplace_result = static_injections_.emplace(
          std::make_pair(item_key, content_injection::StaticInjectionItem()));
      DCHECK(emplace_result.second);
      content_injection::StaticInjectionItem& item =
          emplace_result.first->second;
      item.content = script_file.second.content;

      if (script_file.first.MatchesExtension(kJSExtension)) {
        item.metadata.type = content_injection::mojom::ItemType::kJS;
        item.metadata.javascript_world_id = javascript_world_id_.value();
        item.metadata.run_time =
            content_injection::mojom::ItemRunTime::kDocumentEnd;
      } else {
        DCHECK(script_file.first.MatchesExtension(kCSSExtension));
        item.metadata.type = content_injection::mojom::ItemType::kCSS;
        item.metadata.stylesheet_origin =
            content_injection::mojom::StylesheetOrigin::kAuthor;
        item.metadata.run_time =
            content_injection::mojom::ItemRunTime::kDocumentStart;
      }
    }
  }
}

content_injection::mojom::InjectionsForFramePtr
ServiceImpl::GetInjectionsForFrame(const GURL& url,
                                   content::RenderFrameHost* frame) {
  auto result = content_injection::mojom::InjectionsForFrame::New();

  TabHelper* tab_helper = TabHelper::FromWebContents(
      content::WebContents::FromRenderFrameHost(frame));
  if (tab_helper) {
    for (const auto& script_override : tab_helper->GetScriptOverrides()) {
      if (!script_override.second)
        continue;

      if (script_override.first.MatchesExtension(kJSExtension) &&
          frame->GetParent())
        continue;

      auto enabled_static_injection =
          content_injection::mojom::EnabledStaticInjection::New();
      enabled_static_injection->key = kContentInjectionPrefix;
      enabled_static_injection->key.append(
          script_override.first.AsUTF8Unsafe());
      result->static_injections.push_back(std::move(enabled_static_injection));
    }
  }

  return result;
}

const std::map<std::string, content_injection::StaticInjectionItem>&
ServiceImpl::GetStaticContent() {
  return static_injections_;
}

void ServiceImpl::RestoreOverrides() {
  for (const auto& restore_info : script_overrides_to_restore_) {
    if (!restore_info.second.web_contents())
      continue;

    for (const auto& script_override : restore_info.second.script_overrides())
      DoSetScriptOverrideForTab(restore_info.first, script_override.first,
                                script_override.second);
  }

  script_overrides_to_restore_.clear();
}

ServiceImpl::RestoreInfo::RestoreInfo(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

ServiceImpl::RestoreInfo::~RestoreInfo() = default;

void ServiceImpl::RestoreInfo::Add(
    const base::FilePath& script_path,
    page_actions::Service::ScriptOverride script_override) {
  DCHECK(web_contents());
  script_overrides_[script_path] = script_override;
}

void ServiceImpl::RestoreInfo::WebContentsDestroyed() {
  script_overrides_.clear();
}

}  // namespace page_actions
