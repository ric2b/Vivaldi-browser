// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_IPC_DEMUXER_RENDERER_IPC_FACTORY_H_
#define PLATFORM_MEDIA_IPC_DEMUXER_RENDERER_IPC_FACTORY_H_

#include "media/base/media_export.h"
#include "mojo/public/cpp/bindings/generic_pending_receiver.h"

#include "platform_media/ipc_demuxer/platform_media.mojom-forward.h"

namespace media {

// TODO(igor@vivaldi.com): Figure out how to add MEDIA_EXPORT to generated mojom
// classes and move all relevant functionality to subclasses.
class MEDIA_EXPORT IPCFactory {
 public:
  IPCFactory();
  virtual ~IPCFactory();

  static bool has_instance() { return g_factory != nullptr; }

  static IPCFactory* instance() {
    DCHECK(g_factory);
    return g_factory;
  }

  static void init_instance(IPCFactory* factory) {
    DCHECK(!g_factory);
    DCHECK(factory);
    g_factory = factory;
  }

  // This can be called on any thread.
  virtual scoped_refptr<base::SequencedTaskRunner> GetHostIpcRunner() = 0;

  using GetPipelineFactoryResult =
      base::OnceCallback<void(platform_media::mojom::PipelineFactory& factory)>;

  // This can be called on any thread. The callback will be called on a runner
  // suitable for doing PipelineFactory calls.
  static void GetPipelienFactory(GetPipelineFactoryResult callback);

  // This must be called on Gpu connector runner.
  static void ResetGpuRemoteForTests();

 protected:
  // This must be called on Gpu connector runner.
  virtual void CreateGpuFactory(mojo::GenericPendingReceiver) = 0;

  // This can be called on any thread.
  virtual scoped_refptr<base::SequencedTaskRunner> GetGpuConnectorRunner() = 0;

 private:
  static void GetPipelienFactoryImpl(GetPipelineFactoryResult callback);

  static IPCFactory* g_factory;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_IPC_DEMUXER_RENDERER_IPC_FACTORY_H_
