// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

namespace on_device_model {
namespace {

class SessionImpl : public OnDeviceModel::Session {
 public:
  explicit SessionImpl(std::optional<uint32_t> adaptation_id)
      : adaptation_id_(std::move(adaptation_id)) {}
  ~SessionImpl() override = default;

  SessionImpl(const SessionImpl&) = delete;
  SessionImpl& operator=(const SessionImpl&) = delete;

  void AddContext(mojom::InputOptionsPtr input,
                  mojo::PendingRemote<mojom::ContextClient> client,
                  base::OnceClosure on_complete) override {
    std::string text = input->text;
    if (input->token_offset) {
      text.erase(text.begin(), text.begin() + *input->token_offset);
    }
    if (input->max_tokens && *input->max_tokens < text.size()) {
      text.resize(*input->max_tokens);
    }
    context_.push_back(text);
    if (client) {
      mojo::Remote<mojom::ContextClient> remote(std::move(client));
      remote->OnComplete(text.size());
    }
    std::move(on_complete).Run();
  }

  void Execute(mojom::InputOptionsPtr input,
               mojo::PendingRemote<mojom::StreamingResponder> response,
               base::OnceClosure on_complete) override {
    mojo::Remote<mojom::StreamingResponder> remote(std::move(response));
    if (adaptation_id_) {
      auto chunk = mojom::ResponseChunk::New();
      chunk->text =
          "Adaptation: " + base::NumberToString(*adaptation_id_) + "\n";
      remote->OnResponse(std::move(chunk));
    }
    if (!input->ignore_context) {
      for (const std::string& context : context_) {
        auto chunk = mojom::ResponseChunk::New();
        chunk->text = "Context: " + context + "\n";
        remote->OnResponse(std::move(chunk));
      }
    }

    auto chunk = mojom::ResponseChunk::New();
    chunk->text = "Input: " + input->text + "\n";
    remote->OnResponse(std::move(chunk));
    remote->OnComplete(mojom::ResponseSummary::New());
    std::move(on_complete).Run();
  }

  bool ClearContext() override {
    context_.clear();
    return true;
  }

  void SizeInTokens(const std::string& text,
                    base::OnceCallback<void(uint32_t)> callback) override {
    std::move(callback).Run(text.size());
  }

  void Score(const std::string& text,
             base::OnceCallback<void(float)> callback) override {
    // For unit tests, return the value of the first character.
    std::move(callback).Run(static_cast<float>(text[0]));
  }

  std::unique_ptr<Session> Clone() override {
    auto session = std::make_unique<SessionImpl>(adaptation_id_);
    session->context_ = context_;
    return session;
  }

 private:
  std::vector<std::string> context_;
  std::optional<uint32_t> adaptation_id_;
};

class OnDeviceModelImpl : public OnDeviceModel {
 public:
  OnDeviceModelImpl() = default;
  ~OnDeviceModelImpl() override = default;

  OnDeviceModelImpl(const OnDeviceModelImpl&) = delete;
  OnDeviceModelImpl& operator=(const OnDeviceModelImpl&) = delete;

  std::unique_ptr<Session> CreateSession(
      std::optional<uint32_t> adaptation_id) override {
    return std::make_unique<SessionImpl>(std::move(adaptation_id));
  }

  mojom::SafetyInfoPtr ClassifyTextSafety(const std::string& text) override {
    return nullptr;
  }

  mojom::LanguageDetectionResultPtr DetectLanguage(
      const std::string& text) override {
    return nullptr;
  }

  base::expected<uint32_t, mojom::LoadModelResult> LoadAdaptation(
      mojom::LoadAdaptationParamsPtr params,
      base::OnceClosure on_complete) override {
    std::move(on_complete).Run();
    return base::ok(++next_adaptation_id_);
  }

 private:
  uint32_t next_adaptation_id_ = 0;
};

class OnDeviceModelFakeImpl final : public OnDeviceModelShim {
 public:
  OnDeviceModelFakeImpl();
  ~OnDeviceModelFakeImpl() override;

  base::expected<std::unique_ptr<OnDeviceModel>, mojom::LoadModelResult>
  CreateModel(mojom::LoadModelParamsPtr params,
              base::OnceClosure on_complete) const override {
    auto model = base::ok<std::unique_ptr<OnDeviceModel>>(
        std::make_unique<OnDeviceModelImpl>());
    std::move(on_complete).Run();
    return model;
  }

  mojom::PerformanceClass GetEstimatedPerformanceClass() const override {
    return mojom::PerformanceClass::kFailedToLoadLibrary;
  }
};

OnDeviceModelFakeImpl::OnDeviceModelFakeImpl() = default;
OnDeviceModelFakeImpl::~OnDeviceModelFakeImpl() = default;

}  // namespace

COMPONENT_EXPORT(ON_DEVICE_MODEL_FAKE)
const OnDeviceModelShim* GetOnDeviceModelFakeImpl() {
  static const base::NoDestructor<OnDeviceModelFakeImpl> impl;
  return impl.get();
}

}  // namespace on_device_model
