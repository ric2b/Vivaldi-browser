// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
#define CHROME_BROWSER_AI_AI_TEXT_SESSION_H_

#include <deque>
#include <optional>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/ai/ai_context_bound_object.h"
#include "chrome/browser/ai/ai_context_bound_object_set.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/prompt_api.pb.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/blink/public/mojom/ai/ai_manager.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/ai_text_session.mojom.h"
#include "third_party/blink/public/mojom/ai/ai_text_session_info.mojom-forward.h"
#include "third_party/blink/public/mojom/ai/model_streaming_responder.mojom-forward.h"

// The implementation of `blink::mojom::ModelGenericSession`, which exposes the
// single stream-based `Execute()` API for model execution.
class AITextSession : public AIContextBoundObject,
                      public blink::mojom::AITextSession {
 public:
  using PromptApiPrompt = optimization_guide::proto::PromptApiPrompt;
  using PromptApiRequest = optimization_guide::proto::PromptApiRequest;
  using CreateTextSessionCallback =
      base::OnceCallback<void(blink::mojom::AITextSessionInfoPtr)>;

  // The Context class manages the history of prompt input and output, which are
  // used to build the context when performing the next execution. Context is
  // stored in a FIFO and kept below a limited number of tokens.
  class Context {
   public:
    // The structure storing the text in context and the number of tokens in the
    // text.
    struct ContextItem {
      ContextItem();
      ContextItem(const ContextItem&);
      ContextItem(ContextItem&&);
      ~ContextItem();

      google::protobuf::RepeatedPtrField<PromptApiPrompt> prompts;
      uint32_t tokens = 0;
    };

    Context(uint32_t max_tokens,
            ContextItem initial_prompts,
            bool use_prompt_api_proto);
    Context(const Context&);
    ~Context();

    // Insert a new context item, this may evict some oldest items to ensure the
    // total number of tokens in the context is below the limit.
    void AddContextItem(ContextItem context_item);

    // Combines the initial prompts and all current items into a request.
    // The type of request produced is either PromptApiRequest or StringValue,
    // depending on use_prompt_api_proto = true.
    std::unique_ptr<google::protobuf::MessageLite> MakeRequest();

    // Either returns it's argument wrapped in unique_ptr, or converts it to a
    // StringValue depending on whether this Context has
    // use_prompt_api_proto = true.
    std::unique_ptr<google::protobuf::MessageLite> MaybeFormatRequest(
        PromptApiRequest request);

    // Returns true if the system prompt is set or there is at least one context
    // item.
    bool HasContextItem();

    uint32_t max_tokens() const { return max_tokens_; }
    uint32_t current_tokens() const { return current_tokens_; }
    bool use_prompt_api_proto() const { return use_prompt_api_proto_; }

   private:
    uint32_t max_tokens_;
    uint32_t current_tokens_ = 0;
    ContextItem initial_prompts_;
    std::deque<ContextItem> context_items_;
    // Whether this should use PromptApiRequest or StringValue as request type.
    bool use_prompt_api_proto_;
  };

  // The `AITextSession` will be owned by the `AITextSessionSet` which is bound
  // to the `BucketContext`. However, the `deletion_callback` should be set to
  // properly remove the `AITextSession` from `AITextSessionSet` in case the
  // connection is closed before the `BucketContext` is destroyed.

  // The ownership chain of the relevant class is:
  // `BucketContext` (via `SupportsUserData` or `DocumentUserData`) --owns-->
  // `AITextSessionSet` --owns-->
  // `AITextSession` (implements blink::mojom::AITextSession) --owns-->
  // `mojo::Receiver<blink::mojom::AITextSession>`
  AITextSession(
      std::unique_ptr<
          optimization_guide::OptimizationGuideModelExecutor::Session> session,
      base::WeakPtr<content::BrowserContext> browser_context,
      mojo::PendingReceiver<blink::mojom::AITextSession> receiver,
      AIContextBoundObjectSet* session_set,
      const std::optional<const Context>& context = std::nullopt);
  AITextSession(const AITextSession&) = delete;
  AITextSession& operator=(const AITextSession&) = delete;

  ~AITextSession() override;

  // `AIUserData` implementation.
  void SetDeletionCallback(base::OnceClosure deletion_callback) override;

  // `blink::mojom::ModelTextSession` implementation.
  void Prompt(const std::string& input,
              mojo::PendingRemote<blink::mojom::ModelStreamingResponder>
                  pending_responder) override;
  void Fork(mojo::PendingReceiver<blink::mojom::AITextSession> session,
            ForkCallback callback) override;
  void Destroy() override;

  // Format the initial prompts, gets the token count, updates the session, and
  // passes the session information back through the callback.
  void SetInitialPrompts(
      const std::optional<std::string> system_prompt,
      std::vector<blink::mojom::AIAssistantInitialPromptPtr> initial_prompts,
      CreateTextSessionCallback callback);
  blink::mojom::AITextSessionInfoPtr GetTextSessionInfo();

 private:
  void ModelExecutionCallback(
      const PromptApiRequest& input,
      mojo::RemoteSetElementId responder_id,
      optimization_guide::OptimizationGuideModelStreamingExecutionResult
          result);

  void InitializeContextWithInitialPrompts(
      optimization_guide::proto::PromptApiRequest request,
      CreateTextSessionCallback callback,
      uint32_t size);

  // This function is passed as a completion callback to the
  // `GetSizeInTokens()`. It will
  // - Add the item into context, and remove the oldest items to reduce the
  // context size if the number of tokens in the current context exceeds the
  // limit.
  // - Signal the completion of model execution through the `responder` with the
  // new size of the context.
  void AddPromptHistoryAndSendCompletion(
      const PromptApiRequest& history_item,
      blink::mojom::ModelStreamingResponder* responder,
      uint32_t size);

  // The underlying session provided by optimization guide component.
  std::unique_ptr<optimization_guide::OptimizationGuideModelExecutor::Session>
      session_;
  // The `RemoteSet` storing all the responders, each of them corresponds to one
  // `Execute()` call.
  mojo::RemoteSet<blink::mojom::ModelStreamingResponder> responder_set_;
  base::WeakPtr<content::BrowserContext> browser_context_;
  // Holds all the input and output from the previous prompt.
  std::unique_ptr<Context> context_;
  // It's safe to store a `raw_ptr` here since `this` is owned by
  // `context_bound_object_set_`.
  const raw_ptr<AIContextBoundObjectSet> context_bound_object_set_;

  mojo::Receiver<blink::mojom::AITextSession> receiver_;

  base::WeakPtrFactory<AITextSession> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_AI_AI_TEXT_SESSION_H_
