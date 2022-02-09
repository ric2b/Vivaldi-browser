// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef PLATFORM_MEDIA_TEST_IPC_PIPELINE_TEST_SETUP_H_
#define PLATFORM_MEDIA_TEST_IPC_PIPELINE_TEST_SETUP_H_

#include "base/sequenced_task_runner.h"

//#include "base/threading/sequenced_task_runner_handle.h"
//#include "base/threading/sequenced_task_runner_handle.h"
//#include "base/threading/thread_task_runner_handle.h"

namespace media {

class IPCPipelineTestSetup {
 public:
  struct Fields;

  static void InitStatics();

  IPCPipelineTestSetup();

  ~IPCPipelineTestSetup();

#if defined(OS_MAC)
  static scoped_refptr<base::SequencedTaskRunner> CreatePipelineRunner();
#endif

 private:
  std::unique_ptr<Fields> fields_;

};

}  // namespace media

#endif  // PLATFORM_MEDIA_TEST_IPC_PIPELINE_TEST_SETUP_H_
