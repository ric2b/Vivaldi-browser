// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is based on original work developed by Opera Software ASA.

#include "platform_media/renderer/decoders/ipc_factory.h"

#include "platform_media/common/platform_mime_util.h"

#include "base/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"

#if defined(OS_MACOSX)
#include "base/mac/mac_util.h"
#endif

namespace media {

bool IPCFactory::g_enabled = true;

namespace {

static IPCMediaPipelineHost::Creator
    g_ipc_media_pipeline_host_creator;
static scoped_refptr<base::SequencedTaskRunner>
    g_main_task_runner;
static scoped_refptr<base::SequencedTaskRunner>
    g_media_task_runner;

void RunAndSignal(const base::Closure& task, base::WaitableEvent* done) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  task.Run();
  done->Signal();
}

}  // namespace

IPCFactory::ScopedDisableForTesting::ScopedDisableForTesting() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  IPCFactory::g_enabled = false;
}

IPCFactory::ScopedDisableForTesting::~ScopedDisableForTesting() {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  IPCFactory::g_enabled = true;
}

// static
bool IPCFactory::IsAvailable() {
  if (!g_enabled) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << ": No, disabled";
    return false;
  }

#if defined(OS_MACOSX)
  if (!base::mac::IsAtLeastOS10_10()) {
    // The pre-10.10 PlatformMediaPipeline implementation decodes media by
    // playing them at the regular playback rate.  This is unacceptable for Web
    // Audio API.
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << ": No";
    return false;
  }
#endif

  if (IsPlatformMediaPipelineAvailable(PlatformMediaCheckType::BASIC)) {
    VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << ": Yes";
    return true;
  }

  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__ << ": No";
  return false;
}

// static
void IPCFactory::Preinitialize(
    const IPCMediaPipelineHost::Creator& ipc_media_pipeline_host_creator,
    const scoped_refptr<base::SequencedTaskRunner>& main_task_runner,
    const scoped_refptr<base::SequencedTaskRunner>& media_task_runner) {
  DCHECK(IsAvailable());
  DCHECK(!ipc_media_pipeline_host_creator.is_null());
  DCHECK(main_task_runner);
  DCHECK(media_task_runner);
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  g_ipc_media_pipeline_host_creator = ipc_media_pipeline_host_creator;
  g_main_task_runner = main_task_runner;
  g_media_task_runner = media_task_runner;
}

void IPCFactory::RunCreatorOnMainThread(
    DataSource* data_source,
    std::unique_ptr<IPCMediaPipelineHost>* ipc_media_pipeline_host) {
  DCHECK(g_media_task_runner);
  DCHECK(ipc_media_pipeline_host);
  DCHECK(!ipc_media_pipeline_host->get());
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  *ipc_media_pipeline_host =
            g_ipc_media_pipeline_host_creator.Run(g_media_task_runner,
                                                  data_source);
}

void IPCFactory::CreatePipeline(
    std::unique_ptr<IPCMediaPipelineHost>* ipc_media_pipeline_host) {
  DCHECK(g_main_task_runner);
  DCHECK(ipc_media_pipeline_host);
  DCHECK(!ipc_media_pipeline_host->get());
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  PostTaskAndWait(g_main_task_runner, FROM_HERE,
                  base::Bind(&IPCFactory::RunCreatorOnMainThread,
                             base::Unretained(this),
                             nullptr,
                             ipc_media_pipeline_host));
}

void IPCFactory::ReleasePipeline(
    std::unique_ptr<IPCMediaPipelineHost>* ipc_media_pipeline_host) {
  DCHECK(g_media_task_runner);
  DCHECK(ipc_media_pipeline_host);
  DCHECK(ipc_media_pipeline_host->get());
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;

  base::ThreadRestrictions::ScopedAllowWait scoped_wait;

  PostTaskAndWait(g_media_task_runner, FROM_HERE,
                  base::Bind(&IPCMediaPipelineHost::Stop,
                             base::Unretained(ipc_media_pipeline_host->get())));

  g_media_task_runner->DeleteSoon(FROM_HERE,
                                  ipc_media_pipeline_host->release());
}

void IPCFactory::PostTaskAndWait(
    const scoped_refptr<base::SequencedTaskRunner>& task_runner,
    const base::Location& from_here,
    const base::Closure& task) {
  VLOG(1) << " PROPMEDIA(RENDERER) : " << __FUNCTION__;
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  task_runner->PostTask(from_here, base::Bind(&RunAndSignal, task, &done));
  done.Wait();
}

const scoped_refptr<base::SequencedTaskRunner>& IPCFactory::MediaTaskRunner() {
  DCHECK(IPCFactory::IsAvailable());
  DCHECK(g_media_task_runner);
  return g_media_task_runner;
}

const scoped_refptr<base::SequencedTaskRunner>& IPCFactory::MainTaskRunner() {
  DCHECK(g_main_task_runner);
  return g_main_task_runner;
}

}  // namespace media
