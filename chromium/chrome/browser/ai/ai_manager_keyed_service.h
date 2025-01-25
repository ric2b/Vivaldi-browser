// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_
#define CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_

#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "chrome/browser/ai/ai_summarizer.h"
#include "chrome/browser/ai/ai_text_session.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom-forward.h"

// The browser-side implementation of `blink::mojom::AIManager`. There should
// be one shared AIManagerKeyedService per BrowserContext.
class AIManagerKeyedService : public KeyedService,
                              public blink::mojom::AIManager {
 public:
  explicit AIManagerKeyedService(content::BrowserContext* browser_context);
  AIManagerKeyedService(const AIManagerKeyedService&) = delete;
  AIManagerKeyedService& operator=(const AIManagerKeyedService&) = delete;

  ~AIManagerKeyedService() override;

  void AddReceiver(mojo::PendingReceiver<blink::mojom::AIManager> receiver,
                   AIContextBoundObjectSet::ReceiverContext host);
  void CreateTextSessionForCloning(
      base::PassKey<AITextSession> pass_key,
      mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
      blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
      AIContextBoundObjectSet* context_bound_object_set,
      const AITextSession::Context& context,
      CreateTextSessionCallback callback);

  size_t GetReceiversSizeForTesting() { return receivers_.size(); }

 private:
  FRIEND_TEST_ALL_PREFIXES(AIManagerKeyedServiceTest,
                           NoUAFWithInvalidOnDeviceModelPath);
  FRIEND_TEST_ALL_PREFIXES(AISummarizerUnitTest,
                           CreateSummarizerWithoutService);

  // `blink::mojom::AIManager` implementation.
  void CanCreateTextSession(CanCreateTextSessionCallback callback) override;
  void CreateTextSession(
      mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
      blink::mojom::AITextSessionSamplingParamsPtr sampling_params,
      const std::optional<std::string>& system_prompt,
      std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts,
      CreateTextSessionCallback callback) override;
  void GetTextModelInfo(GetTextModelInfoCallback callback) override;
  void CreateWriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateWriterClient> client,
      blink::mojom::AIWriterCreateOptionsPtr options) override;
  void CanCreateSummarizer(CanCreateSummarizerCallback callback) override;
  void CreateSummarizer(
      mojo::PendingRemote<blink::mojom::AIManagerCreateSummarizerClient> client,
      blink::mojom::AISummarizerCreateOptionsPtr options) override;
  void CreateRewriter(
      mojo::PendingRemote<blink::mojom::AIManagerCreateRewriterClient> client,
      blink::mojom::AIRewriterCreateOptionsPtr options) override;

  void OnModelPathValidationComplete(const std::string& model_path,
                                     bool is_valid_path);

  void CheckModelPathOverrideCanCreateSession(
      const std::string& model_path,
      optimization_guide::ModelBasedCapabilityKey capability);
  void CanOptimizationGuideKeyedServiceCreateGenericSession(
      optimization_guide::ModelBasedCapabilityKey capability,
      CanCreateTextSessionCallback callback);

  void RemoveReceiver(mojo::ReceiverId receiver_id);

  // Creates an `AIAssistant`, either as a new session, or as a clone of
  // an existing session with its context copied.
  std::unique_ptr<AITextSession> CreateTextSessionInternal(
      mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
      const blink::mojom::AITextSessionSamplingParamsPtr& sampling_params,
      AIContextBoundObjectSet* context_bound_object_set,
      const std::optional<const AITextSession::Context>& context =
          std::nullopt);

  // A `KeyedService` should never outlive the `BrowserContext`.
  raw_ptr<content::BrowserContext> browser_context_;

  mojo::ReceiverSet<blink::mojom::AIManager,
                    AIContextBoundObjectSet::ReceiverContext>
      receivers_;

  base::WeakPtrFactory<AIManagerKeyedService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_MANAGER_KEYED_SERVICE_H_
