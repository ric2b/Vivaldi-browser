// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/webnn/public/mojom/webnn_graph.mojom-mojolpm.h"
#include "services/webnn/public/mojom/webnn_graph.mojom.h"
#include "services/webnn/tflite/context_impl_tflite.h"
#include "services/webnn/tflite/graph_builder_tflite.h"
#include "services/webnn/webnn_context_impl.h"
#include "services/webnn/webnn_graph_impl.h"
#include "services/webnn/webnn_graph_mojolpm_fuzzer.pb.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

#if BUILDFLAG(IS_POSIX)
#include "services/webnn/coreml/graph_builder_coreml.h"
#endif

namespace {
struct InitGlobals {
  InitGlobals() {
    mojo::core::Init();
    bool success = base::CommandLine::Init(0, nullptr);
    CHECK(success);

    TestTimeouts::Initialize();

    task_environment = std::make_unique<base::test::TaskEnvironment>(
        base::test::TaskEnvironment::MainThreadType::DEFAULT,
        base::test::TaskEnvironment::TimeSource::MOCK_TIME);
  }

  std::unique_ptr<base::test::TaskEnvironment> task_environment;
};

InitGlobals* init_globals = new InitGlobals();

base::test::TaskEnvironment& GetEnvironment() {
  return *init_globals->task_environment;
}

scoped_refptr<base::SingleThreadTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().GetMainThreadTaskRunner();
}

class WebnnGraphLPMFuzzer {
 public:
  explicit WebnnGraphLPMFuzzer(
      const services::fuzzing::webnn_graph::proto::Testcase& testcase)
      : testcase_(testcase) {}

  void NextAction() {
    const auto& action = testcase_->actions(action_index_);
    const auto& create_graph = action.create_graph();

    auto graph_info_ptr = webnn::mojom::GraphInfo::New();
    mojolpm::FromProto(create_graph.graph_info(), graph_info_ptr);

#if BUILDFLAG(IS_POSIX)
    auto coreml_properties =
        webnn::WebNNContextImpl::IntersectWithBaseProperties(
            webnn::coreml::GraphBuilderCoreml::GetContextProperties());
    if (webnn::WebNNGraphImpl::ValidateGraph(coreml_properties, *graph_info_ptr)
            .has_value()) {
      // Test the Core ML graph builder.
      base::ScopedTempDir temp_dir;
      CHECK(temp_dir.CreateUniqueTempDir());
      auto coreml_graph_builder =
          webnn::coreml::GraphBuilderCoreml::CreateAndBuild(
              *graph_info_ptr, std::move(coreml_properties),
              temp_dir.GetPath());
    }
#endif

    auto tflite_properties =
        webnn::WebNNContextImpl::IntersectWithBaseProperties(
            webnn::tflite::GraphBuilderTflite::GetContextProperties());
    if (webnn::WebNNGraphImpl::ValidateGraph(tflite_properties, *graph_info_ptr)
            .has_value()) {
      // Test the TFLite graph builder.
      auto flatbuffer =
          webnn::tflite::GraphBuilderTflite::CreateAndBuild(*graph_info_ptr);
    }

    ++action_index_;
  }

  bool IsFinished() { return action_index_ >= testcase_->actions_size(); }

 private:
  const raw_ref<const services::fuzzing::webnn_graph::proto::Testcase>
      testcase_;
  int action_index_ = 0;
};

void NextAction(WebnnGraphLPMFuzzer* testcase,
                base::OnceClosure fuzzer_run_loop) {
  if (!testcase->IsFinished()) {
    testcase->NextAction();
    GetFuzzerTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                  std::move(fuzzer_run_loop)));
  } else {
    std::move(fuzzer_run_loop).Run();
  }
}

void RunTestcase(WebnnGraphLPMFuzzer* testcase) {
  base::RunLoop fuzzer_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                fuzzer_run_loop.QuitClosure()));
  // Make sure that all callbacks have completed.
  constexpr base::TimeDelta kTimeout = base::Seconds(5);
  GetEnvironment().FastForwardBy(kTimeout);
  fuzzer_run_loop.Run();
}

DEFINE_BINARY_PROTO_FUZZER(
    const services::fuzzing::webnn_graph::proto::Testcase& testcase) {
  if (!testcase.actions_size()) {
    return;
  }

  WebnnGraphLPMFuzzer webnn_graph_fuzzer_instance(testcase);
  base::RunLoop main_run_loop;

  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(RunTestcase,
                     base::Unretained(&webnn_graph_fuzzer_instance)),
      main_run_loop.QuitClosure());
  main_run_loop.Run();
}

}  // namespace
