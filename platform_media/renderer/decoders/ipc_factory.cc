// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/ipc_factory.h"

#include "base/logging.h"
#include "base/no_destructor.h"
#include "mojo/public/cpp/bindings/remote.h"

#include "platform_media/common/platform_media.mojom.h"

namespace media {

IPCFactory* IPCFactory::g_factory = nullptr;

namespace {

struct IPCFactoryGlobals {
  mojo::Remote<platform_media::mojom::PipelineFactory> gpu_remote;
};

IPCFactoryGlobals& GetIPCFactoryGlobals() {
  static base::NoDestructor<IPCFactoryGlobals> globals;
  return *globals;
}

}  // namespace

IPCFactory::IPCFactory() = default;
IPCFactory::~IPCFactory() = default;

/* static */
void IPCFactory::GetPipelienFactory(GetPipelineFactoryResult callback) {
  instance()->GetGpuConnectorRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IPCFactory::GetPipelienFactoryImpl, std::move(callback)));
}

/* static */
void IPCFactory::GetPipelienFactoryImpl(
    IPCFactory::GetPipelineFactoryResult callback) {
  DCHECK(instance()->GetGpuConnectorRunner()->RunsTasksInCurrentSequence());
  IPCFactoryGlobals& globals = GetIPCFactoryGlobals();
  if (!globals.gpu_remote) {
    mojo::PendingReceiver<platform_media::mojom::PipelineFactory> receiver =
        globals.gpu_remote.BindNewPipeAndPassReceiver();

    globals.gpu_remote.set_disconnect_handler(base::BindOnce([]() {
      // This will trigger a new factory creation attempt when accessing the
      // factory next time.
      GetIPCFactoryGlobals().gpu_remote.reset();
    }));

    instance()->CreateGpuFactory(std::move(receiver));
  }
  std::move(callback).Run(*globals.gpu_remote);
}

/* static */
void IPCFactory::ResetGpuRemoteForTests() {
  DCHECK(instance()->GetGpuConnectorRunner()->RunsTasksInCurrentSequence());
  GetIPCFactoryGlobals().gpu_remote.reset();
}

}  // namespace media
