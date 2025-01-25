// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/command_buffer_helper.h"

#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "build/build_config.h"
#include "gpu/command_buffer/common/scheduling_priority.h"
#include "gpu/command_buffer/service/scheduler.h"
#include "gpu/command_buffer/service/shared_image/shared_image_backing.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "gpu/command_buffer/service/sync_point_manager.h"
#include "gpu/ipc/service/command_buffer_stub.h"
#include "gpu/ipc/service/gpu_channel.h"
#include "gpu/ipc/service/gpu_channel_manager.h"
#include "ui/gl/gl_context.h"

#if BUILDFLAG(IS_WIN)
#include "gpu/command_buffer/service/dxgi_shared_handle_manager.h"
#endif

#if !BUILDFLAG(IS_ANDROID)
#include "media/gpu/gles2_decoder_helper.h"
#endif

namespace media {

class CommandBufferHelperImpl
    : public CommandBufferHelper,
      public gpu::CommandBufferStub::DestructionObserver {
 public:
  explicit CommandBufferHelperImpl(gpu::CommandBufferStub* stub)
      : CommandBufferHelper(stub->channel()->task_runner()),
        stub_(stub),
        memory_tracker_(this),
        memory_type_tracker_(&memory_tracker_) {
    DVLOG(1) << __func__;
    DCHECK(stub_->channel()->task_runner()->BelongsToCurrentThread());

    stub_->AddDestructionObserver(this);
    wait_sequence_id_ = stub_->channel()->scheduler()->CreateSequence(
        gpu::SchedulingPriority::kNormal, stub_->channel()->task_runner());
#if !BUILDFLAG(IS_ANDROID)
    decoder_helper_ = GLES2DecoderHelper::Create(stub_->decoder_context());
#endif
  }

  CommandBufferHelperImpl(const CommandBufferHelperImpl&) = delete;
  CommandBufferHelperImpl& operator=(const CommandBufferHelperImpl&) = delete;

  void WaitForSyncToken(gpu::SyncToken sync_token,
                        base::OnceClosure done_cb) override {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (!stub_) {
      return;
    }

    // TODO(sandersd): Do we need to keep a ref to |this| while there are
    // pending waits? If we destruct while they are pending, they will never
    // run.
    stub_->channel()->scheduler()->ScheduleTask(
        gpu::Scheduler::Task(wait_sequence_id_, std::move(done_cb),
                             std::vector<gpu::SyncToken>({sync_token})));
  }

  // Const variant of GetSharedImageStub() for internal callers.
  gpu::SharedImageStub* shared_image_stub() const {
    if (!stub_) {
      return nullptr;
    }
    return stub_->channel()->shared_image_stub();
  }

#if !BUILDFLAG(IS_ANDROID)
  gpu::SharedImageStub* GetSharedImageStub() override {
    return shared_image_stub();
  }

  gpu::MemoryTypeTracker* GetMemoryTypeTracker() override {
    return &memory_type_tracker_;
  }

  gpu::SharedImageManager* GetSharedImageManager() override {
    if (!stub_) {
      return nullptr;
    }
    return stub_->channel()->gpu_channel_manager()->shared_image_manager();
  }

#if BUILDFLAG(IS_WIN)
  gpu::DXGISharedHandleManager* GetDXGISharedHandleManager() override {
    if (!stub_)
      return nullptr;
    return stub_->channel()
        ->gpu_channel_manager()
        ->shared_image_manager()
        ->dxgi_shared_handle_manager()
        .get();
  }
#endif

  bool HasStub() override {
    DVLOG(4) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    return stub_;
  }

  bool MakeContextCurrent() override {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    return decoder_helper_ && decoder_helper_->MakeContextCurrent();
  }

  std::unique_ptr<gpu::SharedImageRepresentationFactoryRef> Register(
      std::unique_ptr<gpu::SharedImageBacking> backing) override {
    DVLOG(2) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    return stub_->channel()
        ->gpu_channel_manager()
        ->shared_image_manager()
        ->Register(std::move(backing), &memory_type_tracker_);
  }

 public:
  void AddWillDestroyStubCB(WillDestroyStubCB callback) override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    will_destroy_stub_callbacks_.push_back(std::move(callback));
  }
#endif

 private:
  // Helper class to forward memory tracking calls to shared image stub.
  // Necessary because the underlying stub and channel can get destroyed before
  // the CommandBufferHelper and its clients.
  class MemoryTrackerImpl : public gpu::MemoryTracker {
   public:
    explicit MemoryTrackerImpl(CommandBufferHelperImpl* helper)
        : helper_(helper) {
      if (auto* stub = helper_->shared_image_stub()) {
        // We assume these don't change after initialization.
        client_id_ = stub->ClientId();
        client_tracing_id_ = stub->ClientTracingId();
        context_group_tracing_id_ = stub->ContextGroupTracingId();
      }
    }
    ~MemoryTrackerImpl() override = default;

    MemoryTrackerImpl(const MemoryTrackerImpl&) = delete;
    MemoryTrackerImpl& operator=(const MemoryTrackerImpl&) = delete;

    void TrackMemoryAllocatedChange(int64_t delta) override {
      if (auto* stub = helper_->shared_image_stub())
        stub->TrackMemoryAllocatedChange(delta);
    }

    uint64_t GetSize() const override {
      if (auto* stub = helper_->shared_image_stub())
        return stub->GetSize();
      return 0;
    }

    int ClientId() const override { return client_id_; }

    uint64_t ClientTracingId() const override { return client_tracing_id_; }

    uint64_t ContextGroupTracingId() const override {
      return context_group_tracing_id_;
    }

   private:
    const raw_ptr<CommandBufferHelperImpl> helper_;
    int client_id_ = 0;
    uint64_t client_tracing_id_ = 0;
    uint64_t context_group_tracing_id_ = 0;
  };

  ~CommandBufferHelperImpl() override {
    DVLOG(1) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    if (stub_)
      DestroyStub();
  }

  void OnWillDestroyStub(bool have_context) override {
    DVLOG(1) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

    // In case any of the |will_destroy_stub_callbacks_| drops the last
    // reference to |this|, use a weak ptr to check if |this| is still alive.
    auto weak_ptr = weak_ptr_factory_.GetWeakPtr();

    // Save callbacks in case |this| gets destroyed while running callbacks.
    auto callbacks = std::move(will_destroy_stub_callbacks_);
    for (auto& callback : callbacks) {
      std::move(callback).Run(have_context);
    }

    if (weak_ptr && weak_ptr->stub_) {
      weak_ptr->DestroyStub();
    }
  }

  void DestroyStub() {
    DVLOG(3) << __func__;
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if !BUILDFLAG(IS_ANDROID)
    decoder_helper_ = nullptr;
#endif

    // If the last reference to |this| is in a |done_cb|, destroying the wait
    // sequence can delete |this|. Clearing |stub_| first prevents DestroyStub()
    // being called twice.
    gpu::CommandBufferStub* stub = stub_;
    stub_ = nullptr;

    stub->RemoveDestructionObserver(this);
    stub->channel()->scheduler()->DestroySequence(wait_sequence_id_);
  }

  raw_ptr<gpu::CommandBufferStub> stub_;
  // Wait tasks are scheduled on our own sequence so that we can't inadvertently
  // block the command buffer.
  gpu::SequenceId wait_sequence_id_;
#if !BUILDFLAG(IS_ANDROID)
  // TODO(sandersd): Merge GLES2DecoderHelper implementation into this class.
  std::unique_ptr<GLES2DecoderHelper> decoder_helper_;
#endif

  std::vector<WillDestroyStubCB> will_destroy_stub_callbacks_;

  MemoryTrackerImpl memory_tracker_;
  gpu::MemoryTypeTracker memory_type_tracker_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<CommandBufferHelperImpl> weak_ptr_factory_{this};
};

CommandBufferHelper::CommandBufferHelper(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : base::RefCountedDeleteOnSequence<CommandBufferHelper>(
          std::move(task_runner)) {}

// static
scoped_refptr<CommandBufferHelper> CommandBufferHelper::Create(
    gpu::CommandBufferStub* stub) {
  return base::MakeRefCounted<CommandBufferHelperImpl>(stub);
}

}  // namespace media
