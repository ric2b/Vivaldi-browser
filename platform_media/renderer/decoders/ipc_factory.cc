// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/ipc_factory.h"

#include "platform_media/common/platform_mime_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/no_destructor.h"

#if defined(OS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace media {

namespace {

struct IPCFactoryParams {
  IPCMediaPipelineHost::Creator host_creator;
  scoped_refptr<base::SequencedTaskRunner> main_task_runner;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner;
};

IPCFactoryParams* GetIPCFactoryParams() {
  static base::NoDestructor<IPCFactoryParams> params;
  return params.get();
}

}  // namespace

// static
bool IPCFactory::IsAvailable() {
  if (IsPlatformMediaPipelineAvailable(PlatformMediaCheckType::BASIC)) {
    VLOG(2) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << ": Yes";
    return true;
  }

  VLOG(2) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << ": No";
  return false;
}

// static
void IPCFactory::Preinitialize(
    const IPCMediaPipelineHost::Creator& ipc_media_pipeline_host_creator,
    const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner) {
  DCHECK(IsAvailable());
  DCHECK(ipc_media_pipeline_host_creator);
  DCHECK(main_task_runner);
  DCHECK(media_task_runner);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  // The params may already be initialized when running tests.
  IPCFactoryParams* params = GetIPCFactoryParams();
  params->host_creator = ipc_media_pipeline_host_creator;
  params->main_task_runner = main_task_runner;
  params->media_task_runner = media_task_runner;
}

// static
std::unique_ptr<IPCMediaPipelineHost> IPCFactory::CreateHostOnMainThread() {
  std::unique_ptr<IPCMediaPipelineHost> host;
  IPCFactoryParams* params = GetIPCFactoryParams();
  if (params->host_creator) {
    DCHECK(params->main_task_runner);
    DCHECK(params->main_task_runner->RunsTasksInCurrentSequence());
    host = params->host_creator.Run();
  }
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__
          << " host=" << host.get();
  return host;
}

// static
const scoped_refptr<base::SequencedTaskRunner>& IPCFactory::MediaTaskRunner() {
  IPCFactoryParams* params = GetIPCFactoryParams();
  DCHECK(params->media_task_runner);
  return params->media_task_runner;
}

// static
const scoped_refptr<base::SequencedTaskRunner>& IPCFactory::MainTaskRunner() {
  IPCFactoryParams* params = GetIPCFactoryParams();
  DCHECK(params->main_task_runner);
  return params->main_task_runner;
}

}  // namespace media
