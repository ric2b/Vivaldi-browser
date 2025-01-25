// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "components/translate/renderer/vivaldi_translate_agent.h"

#include "base/json/string_escape.h"
#include "components/translate/core/common/translate_constants.h"
#include "components/translate/core/common/translate_errors.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_script_source.h"

namespace {
// Language name passed to the Translate element for it to detect the language.
const char kAutoDetectionLanguage[] = "auto";
}  // namespace

using blink::WebDocumentLoader;
using blink::WebLocalFrame;
using blink::WebScriptSource;
using blink::WebString;
using blink::WebURL;

namespace vivaldi {

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent, public:
VivaldiTranslateAgent::VivaldiTranslateAgent(
    content::RenderFrame* render_frame,
    int world_id)
    : translate::TranslateAgent(render_frame, world_id),
      world_id_(world_id) {}

VivaldiTranslateAgent::~VivaldiTranslateAgent() {}

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent overrides
bool VivaldiTranslateAgent::IsTranslateLibAvailable() {
  // We currently don't load any JS from an external server, so it's always
  // available after injection.
  return script_injected_;
}

bool VivaldiTranslateAgent::IsTranslateLibReady() {
  // We currently don't load any JS from an external server, so it's always
  // available after injection.
  return script_injected_;
}

bool VivaldiTranslateAgent::HasTranslationFinished() {
  return ExecuteScriptAndGetBoolResult("window.vivaldiTranslate.isTranslated",
                                       true);
}

bool VivaldiTranslateAgent::HasTranslationFailed() {
  return ExecuteScriptAndGetBoolResult("window.vivaldiTranslate.error", true);
}

int64_t VivaldiTranslateAgent::GetErrorCode() {
  int64_t error_code =
      ExecuteScriptAndGetIntegerResult("window.vivaldiTranslate.errorCode");
  DCHECK_LT(error_code,
            static_cast<int>(translate::TranslateErrors::TRANSLATE_ERROR_MAX));
  return error_code;
}

std::string VivaldiTranslateAgent::GetPageSourceLanguage() {
  return ExecuteScriptAndGetStringResult("window.vivaldiTranslate.sourceLang");
}

void VivaldiTranslateAgent::RevertTranslation() {
  if (!IsTranslateLibAvailable()) {
    NOTREACHED();
    //return;
  }

  CancelPendingTranslation();

  ExecuteScript("window.vivaldiTranslate.revert()");
}

void VivaldiTranslateAgent::ExecuteScript(const std::string& script) {
  WebLocalFrame* main_frame = render_frame()->GetWebFrame();
  if (!main_frame)
    return;

  WebScriptSource source = WebScriptSource(
      WebString::FromUTF8(script), WebURL(GURL("vivaldi://translate.js")));
  main_frame->ExecuteScriptInIsolatedWorld(
      world_id_, source, blink::BackForwardCacheAware::kAllow);

  script_injected_ = true;
}

void VivaldiTranslateAgent::ReadyToCommitNavigation(
    WebDocumentLoader* document_loader) {
  // Make sure the script is re-injected on a new page.
  script_injected_ = false;
}

// NOTE(pettern): The timeouts for the task is wonky, and sometimes delays much
// longer for no reason I can see, hence we reduce it a lot to mitigate it.
const int kVivaldiTranslateStatusCheckDelayMs = 50;

base::TimeDelta VivaldiTranslateAgent::AdjustDelay(int delay_in_milliseconds) {
  return base::Milliseconds(kVivaldiTranslateStatusCheckDelayMs);
}

////////////////////////////////////////////////////////////////////////////////
// TranslateAgent, private:

/* static */
std::string VivaldiTranslateAgent::BuildTranslationScript(
    const std::string& source_lang,
    const std::string& target_lang) {
  std::string src_lang = source_lang;
  if (source_lang == kAutoDetectionLanguage) {
    src_lang = "";
  }
  return "window.vivaldiTranslate.startTranslate(" +
         base::GetQuotedJSONString(src_lang) + "," +
         base::GetQuotedJSONString(target_lang) + ")";
}

}  // namespace vivaldi
