// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "platform_media/test/ipc_pipeline_test_setup.h"

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/sequence_token.h"
#include "base/task/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/threading/sequence_local_storage_map.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "platform_media/gpu/decoders/mac/data_request_handler.h"

namespace media {

namespace {

// The runner that uses dispatch_queue_t to run its tasks.
class DispatchQueueRunner final : public base::SequencedTaskRunner {
 public:
  DispatchQueueRunner()
      : queue_(dispatch_queue_create(NewDispatchQueueName().c_str(),
                                     DISPATCH_QUEUE_SERIAL)) {
    dispatch_queue_set_specific(queue_, g_dispatch_queue_key, this, nullptr);
    VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
  }

  static DispatchQueueRunner* GetFromCurrentQueue() {
    DispatchQueueRunner* runner = static_cast<DispatchQueueRunner*>(
        dispatch_get_specific(g_dispatch_queue_key));
    CHECK(runner);
    return runner;
  }

  dispatch_queue_t GetQueue() { return queue_; }

  void EnterRunner() {
    DCHECK(dispatch_queue_get_label(queue_) ==
           dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
    DCHECK(!scoped_runner_);
    scoped_runner_.emplace(*this);
  }

  void ExitRunner() {
    DCHECK(dispatch_queue_get_label(queue_) ==
           dispatch_queue_get_label(DISPATCH_CURRENT_QUEUE_LABEL));
    DCHECK(base::SequencedTaskRunnerHandle::IsSet());
    DCHECK_EQ(base::SequencedTaskRunnerHandle::Get(), this);
    DCHECK(scoped_runner_);
    scoped_runner_.reset();
  }

 private:
  // Scopes to associate when entering the current thread.
  struct ScopedRunner {
    ScopedRunner(DispatchQueueRunner& runner)
        : token(runner.token_), handle(&runner), storage(&runner.storage_) {}

    base::ScopedSetSequenceTokenForCurrentThread token;
    base::SequencedTaskRunnerHandle handle;
    base::internal::ScopedSetSequenceLocalStorageMapForCurrentThread storage;
  };

  ~DispatchQueueRunner() override {
    VLOG(2) << " PROPMEDIA(GPU) : " << __FUNCTION__ << " this=" << this;
    dispatch_queue_set_specific(queue_, g_dispatch_queue_key, nullptr, nullptr);
    dispatch_release(queue_);
  }

  static std::string NewDispatchQueueName() {
    static base::AtomicSequenceNumber name_number;
    return base::StringPrintf("com.vivaldi.dispatch_queue_runner-%d",
                              name_number.GetNext());
  }

  // base::SequencedTaskRunner

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    // Keep this alive until the block exit.
    AddRef();
    __block base::OnceClosure block_task = std::move(task);
    dispatch_block_t block = ^{
      EnterRunner();
      std::move(block_task).Run();
      ExitRunner();
      Release();
    };
    if (delay.is_zero()) {
      dispatch_async(queue_, block);
    } else {
      dispatch_after(dispatch_time(DISPATCH_TIME_NOW, delay.InNanoseconds()),
                     queue_, block);
    }
    return true;
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    return PostDelayedTask(from_here, std::move(task), delay);
  }

  bool RunsTasksInCurrentSequence() const override {
    return base::SequenceToken::GetForCurrentThread() == token_;
  }

  static const char g_dispatch_queue_key[1];

  dispatch_queue_t queue_;
  base::SequenceToken token_{base::SequenceToken::Create()};
  base::internal::SequenceLocalStorageMap storage_;

  // optional to allow manual constructor and destructor calls.
  absl::optional<ScopedRunner> scoped_runner_;
};

const char DispatchQueueRunner::g_dispatch_queue_key[1] = {0};

class IPCPipelineTestQueue : public DispatchQueueRunnerProxy {
 public:
  IPCPipelineTestQueue() = default;
  ~IPCPipelineTestQueue() override = default;

  static void Enable() {
    static base::NoDestructor<IPCPipelineTestQueue> storage;
    if (enabled()) {
        CHECK_EQ(instance(), storage.get());
      } else {
      init_instance(storage.get());
    }
  }

 private:
  // DispatchQueueRunnerProxy

  dispatch_queue_t GetQueue() override {
    return DispatchQueueRunner::GetFromCurrentQueue()->GetQueue();
  }

  void EnterRunner() override {
    DispatchQueueRunner::GetFromCurrentQueue()->EnterRunner();
  }

  void ExitRunner() override {
    DispatchQueueRunner::GetFromCurrentQueue()->ExitRunner();
  }
};

}  // namespace

/* static */
scoped_refptr<base::SequencedTaskRunner>
IPCPipelineTestSetup::CreatePipelineRunner() {
  IPCPipelineTestQueue::Enable();
  return base::MakeRefCounted<DispatchQueueRunner>();
}

}  // namespace media
