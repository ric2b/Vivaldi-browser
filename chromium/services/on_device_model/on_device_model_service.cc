// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/on_device_model/on_device_model_service.h"

#include <queue>
#include <vector>

#include "base/metrics/histogram_functions.h"
#include "base/not_fatal_until.h"
#include "base/timer/elapsed_timer.h"
#include "base/uuid.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/on_device_model/public/cpp/on_device_model.h"

#if defined(ENABLE_ML_INTERNAL)
#include "services/on_device_model/ml/on_device_model_internal.h"  //nogncheck
#else
#include "services/on_device_model/on_device_model_fake.h"  //nogncheck
#endif

namespace on_device_model {
namespace {

class ModelWrapper;

class SessionWrapper final : public mojom::Session {
 public:
  SessionWrapper(base::WeakPtr<ModelWrapper> model,
                 mojo::PendingReceiver<mojom::Session> receiver,
                 std::unique_ptr<OnDeviceModel::Session> session)
      : model_(model),
        receiver_(this, std::move(receiver)),
        session_(std::move(session)) {}
  ~SessionWrapper() override = default;

  SessionWrapper(const SessionWrapper&) = delete;
  SessionWrapper& operator=(const SessionWrapper&) = delete;

  void AddContext(mojom::InputOptionsPtr input,
                  mojo::PendingRemote<mojom::ContextClient> client) override;
  void Execute(
      mojom::InputOptionsPtr input,
      mojo::PendingRemote<mojom::StreamingResponder> response) override;
  void GetSizeInTokens(const std::string& text,
                       GetSizeInTokensCallback callback) override;
  void Score(const std::string& text, ScoreCallback callback) override;
  void Clone(mojo::PendingReceiver<mojom::Session> session) override;

  mojo::Receiver<mojom::Session>& receiver() { return receiver_; }

  void ReplayPreviousContext();

  void AddPreviousContext(mojom::InputOptionsPtr input) {
    previous_contexts_.push_back(std::move(input));
  }

 private:
  void AddContextInternal(mojom::InputOptionsPtr input,
                          mojo::PendingRemote<mojom::ContextClient> client,
                          base::OnceClosure on_complete) {
    session_->AddContext(std::move(input), std::move(client),
                         std::move(on_complete));
  }

  void ExecuteInternal(mojom::InputOptionsPtr input,
                       mojo::PendingRemote<mojom::StreamingResponder> response,
                       base::OnceClosure on_complete) {
    session_->Execute(std::move(input), std::move(response),
                      std::move(on_complete));
  }

  void GetSizeInTokensInternal(const std::string& text,
                               GetSizeInTokensCallback callback,
                               base::OnceClosure on_complete) {
    session_->SizeInTokens(text,
                           std::move(callback).Then(std::move(on_complete)));
  }

  void ScoreInternal(const std::string& text,
                     ScoreCallback callback,
                     base::OnceClosure on_complete) {
    session_->Score(text, std::move(callback).Then(std::move(on_complete)));
  }

  void CloneInternal(mojo::PendingReceiver<mojom::Session> session);

  base::WeakPtr<ModelWrapper> model_;
  mojo::Receiver<mojom::Session> receiver_;
  std::unique_ptr<OnDeviceModel::Session> session_;
  std::vector<mojom::InputOptionsPtr> previous_contexts_;
  base::WeakPtrFactory<SessionWrapper> weak_ptr_factory_{this};
};

class ModelWrapper final : public mojom::OnDeviceModel {
 public:
  explicit ModelWrapper(
      bool support_multiple_sessions,
      std::unique_ptr<on_device_model::OnDeviceModel> model,
      mojo::PendingReceiver<mojom::OnDeviceModel> receiver,
      base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete)
      : support_multiple_sessions_(support_multiple_sessions),
        model_(std::move(model)),
        on_delete_(std::move(on_delete)) {
    receivers_.Add(this, std::move(receiver), std::nullopt);
    receivers_.set_disconnect_handler(base::BindRepeating(
        &ModelWrapper::ModelDisconnected, weak_ptr_factory_.GetWeakPtr()));
  }
  ~ModelWrapper() override = default;

  ModelWrapper(const ModelWrapper&) = delete;
  ModelWrapper& operator=(const ModelWrapper&) = delete;

  bool support_multiple_sessions() const { return support_multiple_sessions_; }

  void AddAndRunPendingTask(
      base::OnceCallback<void(base::OnceClosure finish_callback)> task,
      base::WeakPtr<SessionWrapper> session = nullptr) {
    if (support_multiple_sessions_) {
      base::ScopedClosureRunner task_finished(base::BindOnce(
          &ModelWrapper::TaskFinished, weak_ptr_factory_.GetWeakPtr()));
      pending_tasks_.push(PendingTask{
          .session = session,
          .task = base::BindOnce(
              std::move(task), base::BindOnce([](base::ScopedClosureRunner) {},
                                              std::move(task_finished))),
      });
      RunTaskIfPossible();
      return;
    }

    std::move(task).Run(base::DoNothing());
  }

  void StartSession(mojo::PendingReceiver<mojom::Session> session) override {
    AddSession(std::move(session),
               model_->CreateSession(receivers_.current_context()), {});
  }

  void ClassifyTextSafety(const std::string& text,
                          ClassifyTextSafetyCallback callback) override {
    std::move(callback).Run(model_->ClassifyTextSafety(text));
  }

  void DetectLanguage(const std::string& text,
                      DetectLanguageCallback callback) override {
    std::move(callback).Run(model_->DetectLanguage(text));
  }

  void LoadAdaptation(mojom::LoadAdaptationParamsPtr params,
                      mojo::PendingReceiver<mojom::OnDeviceModel> model,
                      LoadAdaptationCallback callback) override {
    if (!support_multiple_sessions_) {
      sessions_.clear();
    }

    auto load_adaptation = base::BindOnce(
        &ModelWrapper::LoadAdaptationInternal, weak_ptr_factory_.GetWeakPtr(),
        std::move(params), std::move(model), std::move(callback));
    AddAndRunPendingTask(
        base::IgnoreArgs<base::OnceClosure>(std::move(load_adaptation)));
  }

  void AddSession(
      mojo::PendingReceiver<mojom::Session> receiver,
      std::unique_ptr<on_device_model::OnDeviceModel::Session> session,
      const std::vector<mojom::InputOptionsPtr>& previous_contexts) {
    auto current_session = std::make_unique<SessionWrapper>(
        weak_ptr_factory_.GetWeakPtr(), std::move(receiver),
        std::move(session));
    for (const auto& context : previous_contexts) {
      current_session->AddPreviousContext(context.Clone());
    }
    SessionWrapper* current_session_ptr = current_session.get();

    if (!support_multiple_sessions_) {
      sessions_.clear();
    }

    sessions_.insert(std::move(current_session));
    current_session_ptr->receiver().set_disconnect_handler(
        base::BindOnce(&ModelWrapper::SessionDisconnected,
                       base::Unretained(this), current_session_ptr));
  }

 private:
  struct PendingTask {
    base::WeakPtr<SessionWrapper> session;
    base::OnceClosure task;
  };

  void SessionDisconnected(SessionWrapper* ptr) {
    auto it = sessions_.find(ptr);
    if (it != sessions_.end()) {
      sessions_.erase(it);
    }
  }

  void ModelDisconnected() {
    if (receivers_.empty()) {
      std::move(on_delete_).Run(weak_ptr_factory_.GetWeakPtr());
    }
  }

  void LoadAdaptationInternal(mojom::LoadAdaptationParamsPtr params,
                              mojo::PendingReceiver<mojom::OnDeviceModel> model,
                              LoadAdaptationCallback callback) {
    auto start = base::TimeTicks::Now();
    auto result = model_->LoadAdaptation(
        std::move(params),
        base::BindOnce(
            [](base::TimeTicks start) {
              base::UmaHistogramMediumTimes(
                  "OnDeviceModel.LoadAdaptationModelDuration",
                  base::TimeTicks::Now() - start);
            },
            start));
    if (!result.has_value()) {
      std::move(callback).Run(result.error());
      return;
    }
    receivers_.Add(this, std::move(model), *result);
    std::move(callback).Run(mojom::LoadModelResult::kSuccess);
  }

  void RunTaskIfPossible() {
    if (!support_multiple_sessions_) {
      return;
    }

    if (is_running_) {
      return;
    }

    if (pending_tasks_.empty()) {
      return;
    }

    PendingTask pending_task = std::move(pending_tasks_.front());
    pending_tasks_.pop();

    is_running_ = true;
    running_session_ = pending_task.session;
    if (running_session_ && running_session_.get() != last_session_.get()) {
      running_session_->ReplayPreviousContext();
    }

    std::move(pending_task.task).Run();
  }

  void TaskFinished() {
    last_session_ = running_session_;
    is_running_ = false;
    RunTaskIfPossible();
  }

  bool support_multiple_sessions_;
  std::set<std::unique_ptr<SessionWrapper>, base::UniquePtrComparator>
      sessions_;
  std::unique_ptr<on_device_model::OnDeviceModel> model_;
  mojo::ReceiverSet<mojom::OnDeviceModel, std::optional<uint32_t>> receivers_;
  base::OnceCallback<void(base::WeakPtr<mojom::OnDeviceModel>)> on_delete_;
  std::queue<PendingTask> pending_tasks_;
  bool is_running_ = false;
  base::WeakPtr<SessionWrapper> running_session_;
  // Last session a task was executed in.
  base::WeakPtr<SessionWrapper> last_session_;
  base::WeakPtrFactory<ModelWrapper> weak_ptr_factory_{this};
};

void SessionWrapper::AddContext(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::ContextClient> client) {
  if (!model_) {
    return;
  }

  base::OnceClosure save_context = base::DoNothing();
  if (model_->support_multiple_sessions()) {
    save_context =
        base::BindOnce(&SessionWrapper::AddPreviousContext,
                       weak_ptr_factory_.GetWeakPtr(), input.Clone());
  }

  auto add_context_internal = base::BindOnce(
      &SessionWrapper::AddContextInternal, weak_ptr_factory_.GetWeakPtr(),
      std::move(input), std::move(client));

  auto add_context = base::BindOnce(
      [](decltype(add_context_internal) add_context_internal,
         base::OnceClosure save_context, base::OnceClosure finish_callback) {
        std::move(add_context_internal)
            .Run(std::move(save_context).Then(std::move(finish_callback)));
      },
      std::move(add_context_internal), std::move(save_context));

  model_->AddAndRunPendingTask(std::move(add_context),
                               weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::Execute(
    mojom::InputOptionsPtr input,
    mojo::PendingRemote<mojom::StreamingResponder> response) {
  if (!model_) {
    return;
  }

  auto execute_internal = base::BindOnce(&SessionWrapper::ExecuteInternal,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(input), std::move(response));

  model_->AddAndRunPendingTask(std::move(execute_internal),
                               weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::GetSizeInTokens(const std::string& text,
                                     GetSizeInTokensCallback callback) {
  if (!model_) {
    return;
  }

  auto size_in_tokens_internal =
      base::BindOnce(&SessionWrapper::GetSizeInTokensInternal,
                     weak_ptr_factory_.GetWeakPtr(), text, std::move(callback));

  model_->AddAndRunPendingTask(std::move(size_in_tokens_internal),
                               weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::Score(const std::string& text, ScoreCallback callback) {
  if (!model_) {
    return;
  }

  model_->AddAndRunPendingTask(
      base::BindOnce(&SessionWrapper::ScoreInternal,
                     weak_ptr_factory_.GetWeakPtr(), text, std::move(callback)),
      weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::Clone(mojo::PendingReceiver<mojom::Session> session) {
  if (!model_) {
    return;
  }

  model_->AddAndRunPendingTask(
      base::IgnoreArgs<base::OnceClosure>(
          base::BindOnce(&SessionWrapper::CloneInternal,
                         weak_ptr_factory_.GetWeakPtr(), std::move(session))),
      weak_ptr_factory_.GetWeakPtr());
}

void SessionWrapper::CloneInternal(
    mojo::PendingReceiver<mojom::Session> session) {
  if (!model_) {
    return;
  }

  model_->AddSession(std::move(session), session_->Clone(), previous_contexts_);
}

void SessionWrapper::ReplayPreviousContext() {
  if (session_->ClearContext()) {
    for (const auto& context : previous_contexts_) {
      AddContextInternal(context.Clone(),
                         mojo::PendingRemote<mojom::ContextClient>(),
                         base::DoNothing());
    }
  }
}

const OnDeviceModelShim* DefaultImpl() {
#if defined(ENABLE_ML_INTERNAL)
  return ml::GetOnDeviceModelInternalImpl();
#else
  return GetOnDeviceModelFakeImpl();
#endif
}

}  // namespace

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver)
    : OnDeviceModelService(std::move(receiver), DefaultImpl()) {}

OnDeviceModelService::OnDeviceModelService(
    mojo::PendingReceiver<mojom::OnDeviceModelService> receiver,
    const OnDeviceModelShim* impl)
    : receiver_(this, std::move(receiver)), impl_(impl) {}

OnDeviceModelService::~OnDeviceModelService() = default;

void OnDeviceModelService::LoadModel(
    mojom::LoadModelParamsPtr params,
    mojo::PendingReceiver<mojom::OnDeviceModel> model,
    LoadModelCallback callback) {
  auto start = base::TimeTicks::Now();
  bool support_multiple_sessions = params->support_multiple_sessions;
  auto model_impl = impl_->CreateModel(
      std::move(params), base::BindOnce(
                             [](base::TimeTicks start) {
                               base::UmaHistogramMediumTimes(
                                   "OnDeviceModel.LoadModelDuration",
                                   base::TimeTicks::Now() - start);
                             },
                             start));
  if (!model_impl.has_value()) {
    std::move(callback).Run(model_impl.error());
    return;
  }

  models_.insert(std::make_unique<ModelWrapper>(
      support_multiple_sessions, std::move(model_impl.value()),
      std::move(model),
      base::BindOnce(&OnDeviceModelService::DeleteModel,
                     base::Unretained(this))));
  std::move(callback).Run(mojom::LoadModelResult::kSuccess);
}

void OnDeviceModelService::GetEstimatedPerformanceClass(
    GetEstimatedPerformanceClassCallback callback) {
  base::ElapsedTimer timer;
  std::move(callback).Run(impl_->GetEstimatedPerformanceClass());
  base::UmaHistogramTimes("OnDeviceModel.BenchmarkDuration", timer.Elapsed());
}

void OnDeviceModelService::DeleteModel(
    base::WeakPtr<mojom::OnDeviceModel> model) {
  if (!model) {
    return;
  }
  auto it = models_.find(model.get());
  CHECK(it != models_.end(), base::NotFatalUntil::M130);
  models_.erase(it);
}

}  // namespace on_device_model
