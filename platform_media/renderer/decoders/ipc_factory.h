// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_RENDERER_DECODERS_IPC_FACTORY_H_
#define PLATFORM_MEDIA_RENDERER_DECODERS_IPC_FACTORY_H_

#include "platform_media/common/feature_toggles.h"

#include "platform_media/renderer/pipeline/ipc_media_pipeline_host.h"

#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT IPCFactory
{
public:
  static bool IsAvailable();
  static void Preinitialize(
      const IPCMediaPipelineHost::Creator& ipc_media_pipeline_host_creator,
      const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& media_task_runner);

  // This must be called on the main thread.
  static std::unique_ptr<IPCMediaPipelineHost> CreateHostOnMainThread();

  static const scoped_refptr<base::SequencedTaskRunner>& MediaTaskRunner();

  static const scoped_refptr<base::SequencedTaskRunner>& MainTaskRunner();
};

}

#endif // PLATFORM_MEDIA_RENDERER_DECODERS_IPC_FACTORY_H_
