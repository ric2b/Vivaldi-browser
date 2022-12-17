// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef PLATFORM_MEDIA_TEST_IPC_PIPELINE_TEST_SETUP_H_
#define PLATFORM_MEDIA_TEST_IPC_PIPELINE_TEST_SETUP_H_

#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"

namespace media {

class IPCPipelineTestSetup {
 public:
  struct Fields;

  IPCPipelineTestSetup();

  ~IPCPipelineTestSetup();

#if BUILDFLAG(IS_MAC)
  static scoped_refptr<base::SequencedTaskRunner> CreatePipelineRunner();
#endif

 private:
  std::unique_ptr<Fields> fields_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_TEST_IPC_PIPELINE_TEST_SETUP_H_
