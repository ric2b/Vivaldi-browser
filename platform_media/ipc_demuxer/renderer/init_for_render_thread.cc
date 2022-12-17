// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "platform_media/ipc_demuxer/renderer/init_for_render_thread.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "content/renderer/render_thread_impl.h"

#include "platform_media/ipc_demuxer/renderer/ipc_factory.h"

namespace platform_media {

namespace {

class RenderThreadIpcFactory : public media::IPCFactory {
 public:
  RenderThreadIpcFactory(content::RenderThreadImpl& t)
      : main_thread_runner_(base::ThreadTaskRunnerHandle::Get()),
        host_ipc_runner_(t.GetMediaThreadTaskRunner()) {}

  scoped_refptr<base::SequencedTaskRunner> GetGpuConnectorRunner() override {
    return main_thread_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> GetHostIpcRunner() override {
    return host_ipc_runner_;
  }

  void CreateGpuFactory(mojo::GenericPendingReceiver receiver) override {
    content::RenderThreadImpl* t = content::RenderThreadImpl::current();
    if (!t)
      return;
    scoped_refptr<gpu::GpuChannelHost> gpu_channel_host = t->GetGpuChannel();
    if (!gpu_channel_host) {
      gpu_channel_host = t->EstablishGpuChannelSync();
      if (gpu_channel_host) {
        LOG(INFO) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                  << " Using newly established GpuChannelHost";
      } else {
        LOG(ERROR) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
                   << " Establishing a GpuChannelHost failed, not able to "
                      "decode proprietary media";
        return;
      }
    }
    gpu_channel_host->GetGpuChannel().VivaldiCreateMediaPipelineFactory(
        std::move(receiver));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> main_thread_runner_;
  scoped_refptr<base::SequencedTaskRunner> host_ipc_runner_;
};

}  // namespace

void InitForRenderThread(content::RenderThreadImpl& t) {
  static base::NoDestructor<RenderThreadIpcFactory> factory{t};
  media::IPCFactory::init_instance(factory.get());
}

}  // namespace platform_media