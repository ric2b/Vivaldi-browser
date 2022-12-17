// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "platform_media/ipc_demuxer/test/ipc_pipeline_test_setup.h"

#include "base/no_destructor.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequence_local_storage_slot.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"

#include "platform_media/ipc_demuxer/gpu/pipeline/ipc_media_pipeline.h"
#include "platform_media/ipc_demuxer/renderer/ipc_factory.h"

#if BUILDFLAG(IS_WIN)
#include "platform_media/sandbox/win/platform_media_init.h"
#endif

namespace media {

struct IPCPipelineTestSetup::Fields {
  scoped_refptr<base::SequencedTaskRunner> pipeline_runner;
  scoped_refptr<base::SequencedTaskRunner> host_ipc_runner;
  base::WaitableEvent ipc_finished_event;
};

namespace {

IPCPipelineTestSetup::Fields* g_current_fields = nullptr;

class TestIpcFactory : public IPCFactory {
 public:
  TestIpcFactory() = default;
  ~TestIpcFactory() override = default;

  scoped_refptr<base::SequencedTaskRunner> GetGpuConnectorRunner() override {
    CHECK(g_current_fields);
    return g_current_fields->pipeline_runner;
  }

  scoped_refptr<base::SequencedTaskRunner> GetHostIpcRunner() override {
    CHECK(g_current_fields);
    return g_current_fields->host_ipc_runner;
  }

  void CreateGpuFactory(mojo::GenericPendingReceiver receiver) override {
    CHECK(g_current_fields);
    CHECK_EQ(g_current_fields->pipeline_runner,
             base::SequencedTaskRunnerHandle::Get());
    IPCMediaPipeline::CreateFactory(std::move(receiver));
  }
};

// Helper to observe destruction of the current SequencedTaskRunner.
class RunnerDestructorObserver {
 public:
  RunnerDestructorObserver(base::OnceClosure observer)
      : observer_(std::move(observer)) {}

  ~RunnerDestructorObserver() { std::move(observer_).Run(); }

  // Call the observer closure when the current runner is destructed.
  static void ObserveCurrent(base::OnceClosure observer) {
    static base::SequenceLocalStorageSlot<RunnerDestructorObserver> slot;
    slot.emplace(std::move(observer));
  }

 private:
  base::OnceClosure observer_;
};

}  // namespace

IPCPipelineTestSetup::IPCPipelineTestSetup()
    : fields_(std::make_unique<Fields>()) {
  CHECK(!g_current_fields);
#if BUILDFLAG(IS_MAC)
  fields_->pipeline_runner = CreatePipelineRunner();
#else
  fields_->pipeline_runner = base::ThreadPool::CreateSequencedTaskRunner({});
#endif
  fields_->host_ipc_runner = base::ThreadPool::CreateSequencedTaskRunner({});
  if (!IPCFactory::has_instance()) {
    static base::NoDestructor<TestIpcFactory> factory;
    media::IPCFactory::init_instance(factory.get());
  }
  g_current_fields = fields_.get();
}

IPCPipelineTestSetup::~IPCPipelineTestSetup() {
  CHECK_EQ(g_current_fields, fields_.get());
  fields_->pipeline_runner->PostTask(
      FROM_HERE, base::BindOnce([] {
        // Drop all known references to the pipeline runner and wait until it is
        // destructed.
        IPCFactory::ResetGpuRemoteForTests();
        g_current_fields->pipeline_runner.reset();
        RunnerDestructorObserver::ObserveCurrent(base::BindOnce([] {
          CHECK(g_current_fields);
          g_current_fields->ipc_finished_event.Signal();
        }));
      }));

  fields_->ipc_finished_event.Wait();

  g_current_fields = nullptr;
}

}  // namespace media
