// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef COMPONENTS_TRANSLATE_RENDERER_VIVALDI_TRANSLATE_AGENT_H_
#define COMPONENTS_TRANSLATE_RENDERER_VIVALDI_TRANSLATE_AGENT_H_

#include "base/time/time.h"
#include "components/translate/content/common/translate.mojom.h"
#include "components/translate/content/renderer/translate_agent.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace vivaldi {

// This class deals with page translation.
// There is one VivaldiTranslateAgent per RenderView.
class VivaldiTranslateAgent : public translate::TranslateAgent {
 public:
  VivaldiTranslateAgent(content::RenderFrame* render_frame,
                        int world_id);
  ~VivaldiTranslateAgent() override;
  VivaldiTranslateAgent(const VivaldiTranslateAgent&) = delete;
  VivaldiTranslateAgent& operator=(const VivaldiTranslateAgent&) = delete;

  // mojom::TranslateAgent implementation.
  void RevertTranslation() override;

  // content::RenderFrameObserver overrides
  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override;

 protected:
  // TranslateAgent overrides
  bool IsTranslateLibAvailable() override;
  bool IsTranslateLibReady() override;
  bool HasTranslationFinished() override;
  bool HasTranslationFailed() override;
  int64_t GetErrorCode() override;
  std::string GetPageSourceLanguage() override;
  void ExecuteScript(const std::string& script) override;
  base::TimeDelta AdjustDelay(int delay_in_milliseconds) override;

 private:
  friend TranslateAgent;

  bool script_injected_ = false;

  // The world ID to use for script execution.
  int world_id_;

  // Builds the translation JS used to translate from source_lang to
  // target_lang.
  static std::string BuildTranslationScript(const std::string& source_lang,
                                            const std::string& target_lang);
};

}  // namespace vivaldi

#endif  // COMPONENTS_TRANSLATE_RENDERER_VIVALDI_TRANSLATE_AGENT_H_
