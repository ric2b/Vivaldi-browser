// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#include "components/content_injection/renderer/content_injection_frame_handler.h"

#include <algorithm>

#include "app/vivaldi_apptools.h"
#include "base/strings/string_util.h"
#include "components/content_injection/renderer/content_injection_manager.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"

namespace content_injection {
namespace {
std::string ReplacePlaceholders(std::string_view format_string,
                                std::vector<std::string> replacements) {
  DCHECK_LT(replacements.size(), 10U);

  if (replacements.size() == 0)
    return std::string(format_string);

  size_t replacement_length = 0;
  for (const auto& replacement : replacements)
    replacement_length += replacement.length();

  std::string result;
  result.reserve(format_string.length() + replacement_length);

  const int kPlaceHolderLength = 5;

  for (auto c = format_string.begin(); c != format_string.end(); ++c) {
    if (*c == '{' && format_string.end() - c >= kPlaceHolderLength) {
      if (*(c + 1) == '{' && base::IsAsciiDigit(*(c + 2)) && *(c + 3) == '}' &&
          *(c + 4) == '}') {
        size_t replacement_index = *(c + 2) - '0';
        if (replacement_index < replacements.size())
          result.append(replacements[*(c + 2) - '0']);
        c += 5;
      }
    }

    result.push_back(*c);
  }

  return result;
}

std::optional<mojom::ItemRunTime> GetNextRunTime(mojom::ItemRunTime current) {
  switch (current) {
    case mojom::ItemRunTime::kDocumentStart:
      return mojom::ItemRunTime::kDocumentEnd;
    case mojom::ItemRunTime::kDocumentEnd:
      return mojom::ItemRunTime::kDocumentIdle;
    case mojom::ItemRunTime::kDocumentIdle:
      return std::nullopt;
  }
}
}  // namespace

FrameHandler::FrameHandler(content::RenderFrame* render_frame,
                           service_manager::BinderRegistry* registry)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<FrameHandler>(render_frame) {
  DCHECK(render_frame);

  if (vivaldi::IsVivaldiRunning() || vivaldi::ForcedVivaldiRunning()) {
    render_frame->GetBrowserInterfaceBroker().GetInterface(
        injection_helper_.BindNewPipeAndPassReceiver());
  }
  registry->AddInterface(base::BindRepeating(
      &FrameHandler::BindFrameHandlerReceiver, base::Unretained(this)));
}

FrameHandler::~FrameHandler() = default;

void FrameHandler::BindFrameHandlerReceiver(
    mojo::PendingReceiver<mojom::FrameHandler> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FrameHandler::OnDestruct() {
  delete this;
}

void FrameHandler::EnableStaticInjection(
    mojom::EnabledStaticInjectionPtr injection,
    EnableStaticInjectionCallback callback) {
  std::move(callback).Run(AddStaticInjection(std::move(injection)));
}
void FrameHandler::DisableStaticInjection(
    const std::string& key,
    DisableStaticInjectionCallback callback) {
  std::move(callback).Run(RemoveStaticInjection(key));
}

void FrameHandler::DidCreateNewDocument() {
  // Unretained is safe because cosmetic_filter_ cancels all callbacks when
  // destroyed.
  if (injection_helper_) {
    injection_helper_->GetInjections(
        render_frame()->GetWebFrame()->GetDocument().Url(),
        base::BindOnce(&FrameHandler::OnInjectionsReceived,
                       base::Unretained(this)));
  }

  last_run_time_.reset();
  pending_injections_.reset();
  injected_static_scripts_.clear();
}

void FrameHandler::OnInjectionsReceived(
    mojom::InjectionsForFramePtr injections) {
  pending_injections_ = std::move(injections);
  InjectPendingScripts();
}

bool FrameHandler::AddStaticInjection(
    mojom::EnabledStaticInjectionPtr injection) {
  const StaticInjectionItem* injection_item =
      Manager::GetInstance().GetInjectionItem(injection->key);
  if (!injection_item)
    return false;

  if (pending_injections_) {
    std::vector<mojom::EnabledStaticInjectionPtr>& static_injections =
        pending_injections_->static_injections;
    if (!last_run_time_ ||
        injection_item->metadata.run_time > last_run_time_.value()) {
      if (std::find_if(
              static_injections.begin(), static_injections.end(),
              [&injection](
                  const mojom::EnabledStaticInjectionPtr& existing_injection) {
                return injection->key == existing_injection->key;
              }) != static_injections.end())
        return false;

      static_injections.push_back(std::move(injection));
      return true;
    }
  }

  return InjectScript(injection->key,
                      ReplacePlaceholders(injection_item->content,
                                          injection->placeholder_replacements),
                      injection_item->metadata);
}

bool FrameHandler::RemoveStaticInjection(const std::string& key) {
  if (injected_static_scripts_.count(key)) {
    const StaticInjectionItem* injection_item =
        Manager::GetInstance().GetInjectionItem(key);
    // We can't undo JS injection
    if (injection_item->metadata.type != mojom::ItemType::kCSS)
      return false;
    if (!render_frame())
      return false;
    RemoveInjectedCSS(key, injection_item->metadata.stylesheet_origin);
    injected_static_scripts_.erase(key);
    return true;
  }

  if (!pending_injections_)
    return false;

  std::vector<mojom::EnabledStaticInjectionPtr>& static_injections =
      pending_injections_->static_injections;

  auto existing_pending =
      std::find_if(static_injections.begin(), static_injections.end(),
                   [&key](const mojom::EnabledStaticInjectionPtr& injection) {
                     return injection->key == key;
                   });
  if (existing_pending == static_injections.end())
    return false;
  static_injections.erase(existing_pending);
  return true;
}

void FrameHandler::InjectScriptsForRunTime(mojom::ItemRunTime run_time) {
  // Certain run location signals (like DidCreateDocumentElement) can happen
  // multiple times. Ignore the subsequent signals.
  if (last_run_time_ && run_time <= last_run_time_.value()) {
    return;
  }

  // We also don't execute if we detect that the run time is somehow out of
  // order. This can happen if:
  // - The first run time reported for the frame isn't kDocumentStart, or
  // - The run time reported doesn't immediately follow the previous
  //   reported run time.
  // We don't want to run injected scripts because they may have requirements
  // that the scripts for an earlier run time have run. Better to just not run.
  std::optional<mojom::ItemRunTime> next_run_time =
      last_run_time_ ? GetNextRunTime(last_run_time_.value())
                     : mojom::ItemRunTime::kDocumentStart;
  if (!next_run_time || run_time != next_run_time.value()) {
    pending_injections_.reset();
    last_run_time_.reset();
    return;
  }

  last_run_time_ = run_time;
  InjectPendingScripts();
}

bool FrameHandler::InjectScript(const std::string& key,
                                const std::string& content,
                                const mojom::InjectionItemMetadata& metadata) {
  if (!render_frame())
    return false;

  // Attempting to inject the same static script twice is probably a mistake.
  if (!key.empty() && injected_static_scripts_.count(key) != 0)
    return false;

  switch (metadata.type) {
    case mojom::ItemType::kCSS:
      InjectCSS(key, content, metadata.stylesheet_origin);
      break;
    case mojom::ItemType::kJS:
      InjectJS(key, content, metadata.javascript_world_id);
      break;
  }

  if (!key.empty())
    injected_static_scripts_.insert(key);
  return true;
}

void FrameHandler::InjectCSS(const std::string& key,
                             const std::string& content,
                             const mojom::StylesheetOrigin origin) {
  blink::WebStyleSheetKey style_sheet_key = blink::WebString::FromASCII(key);
  auto blink_css_origin = origin == mojom::StylesheetOrigin::kAuthor
                              ? blink::WebCssOrigin::kAuthor
                              : blink::WebCssOrigin::kUser;

  render_frame()->GetWebFrame()->GetDocument().InsertStyleSheet(
      blink::WebString::FromUTF8(content),
      style_sheet_key.IsEmpty() ? nullptr : &style_sheet_key, blink_css_origin);
}
void FrameHandler::RemoveInjectedCSS(const std::string& key,
                                     const mojom::StylesheetOrigin origin) {
  blink::WebStyleSheetKey style_sheet_key = blink::WebString::FromASCII(key);
  auto blink_css_origin = origin == mojom::StylesheetOrigin::kAuthor
                              ? blink::WebCssOrigin::kAuthor
                              : blink::WebCssOrigin::kUser;

  render_frame()->GetWebFrame()->GetDocument().RemoveInsertedStyleSheet(
      style_sheet_key, blink_css_origin);
}

void FrameHandler::InjectJS(const std::string& key, const std::string& content, int world_id) {
  std::vector<blink::WebScriptSource> sources(
      1, blink::WebScriptSource(blink::WebString::FromUTF8(content), GURL(key)));

  render_frame()->GetWebFrame()->RequestExecuteScript(
      world_id, sources, blink::mojom::UserActivationOption::kDoNotActivate,
      blink::mojom::EvaluationTiming::kSynchronous,
      blink::mojom::LoadEventBlockingOption::kBlock, {},
      blink::BackForwardCacheAware::kPossiblyDisallow,
      blink::mojom::WantResultOption::kNoResult,
      blink::mojom::PromiseResultOption::kDoNotWait);
}

void FrameHandler::InjectPendingScripts() {
  if (!pending_injections_ || !last_run_time_ || !render_frame())
    return;

  std::vector<mojom::EnabledStaticInjectionPtr>& static_injections =
      pending_injections_->static_injections;
  static_injections.erase(
      std::remove_if(
          static_injections.begin(), static_injections.end(),
          [this](const mojom::EnabledStaticInjectionPtr& static_injection) {
            const StaticInjectionItem* injection_item =
                Manager::GetInstance().GetInjectionItem(static_injection->key);
            if (!injection_item)
              return true;
            if (injection_item->metadata.run_time > last_run_time_.value())
              return false;
            InjectScript(
                static_injection->key,
                ReplacePlaceholders(injection_item->content,
                                    static_injection->placeholder_replacements),
                injection_item->metadata);
            return true;
          }),
      static_injections.end());

  std::vector<mojom::DynamicInjectionItemPtr>& dynamic_injections =
      pending_injections_->dynamic_injections;
  dynamic_injections.erase(
      std::remove_if(
          dynamic_injections.begin(), dynamic_injections.end(),
          [this](const mojom::DynamicInjectionItemPtr& dynamic_injection) {
            if (dynamic_injection->metadata->run_time > last_run_time_.value())
              return false;
            InjectScript(std::string(), dynamic_injection->content,
                         *dynamic_injection->metadata);
            return true;
          }),
      dynamic_injections.end());
}

}  // namespace content_injection