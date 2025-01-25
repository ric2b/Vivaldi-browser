/* Copyright 2024 The OpenXLA Authors.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "xla/backends/cpu/runtime/thunk_executor.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "xla/backends/cpu/runtime/buffer_allocations.h"
#include "xla/backends/cpu/runtime/resource_use.h"
#include "xla/backends/cpu/runtime/thunk.h"
#include "xla/runtime/buffer_use.h"
#include "xla/service/buffer_assignment.h"
#include "xla/service/maybe_owning_device_memory.h"
#include "xla/stream_executor/device_memory.h"
#include "xla/tsl/concurrency/async_value_ref.h"
#include "tsl/platform/env.h"
#include "tsl/platform/errors.h"
#include "tsl/platform/logging.h"
#include "tsl/platform/statusor.h"
#include "tsl/platform/test.h"
#include "tsl/platform/test_benchmark.h"
#include "tsl/platform/threadpool.h"

#define EIGEN_USE_THREADS

#include "unsupported/Eigen/CXX11/Tensor"

namespace xla::cpu {
namespace {

using ::testing::ElementsAre;

// We use a global static variable to simulate a shared resource. We check that
// thunk executor correctly orders access to this resource by running the test
// with a thread sanitizer and checking that there are no data races.
static int64_t shared_resource;

// A test-only thunk for verifying thunk executor implementation:
//
//   dst += src (for all srcs and dsts slices)
//
// We generate random thunk sequences reading and writing different slices of
// the same buffer, and check that at run time it does not lead to any data
// races and produces expected result.
class AddI32Thunk final : public Thunk {
 public:
  AddI32Thunk(std::string name, std::vector<BufferAllocation::Slice> srcs,
              std::vector<BufferAllocation::Slice> dsts,
              std::vector<std::string>* trace, bool use_shared_resource,
              bool inject_error);

  static std::unique_ptr<Thunk> Create(
      std::string name, std::vector<BufferAllocation::Slice> srcs,
      std::vector<BufferAllocation::Slice> dsts,
      std::vector<std::string>* trace = nullptr,
      bool use_shared_resource = false, bool inject_error = false);

  static std::vector<MaybeOwningDeviceMemory> AsDeviceMemory(
      absl::Span<std::vector<int32_t>* const> data);

  // Executes `dst += src` for a single src/dst pair.
  static absl::Status Execute(const BufferAllocations* allocations,
                              BufferAllocation::Slice src_slice,
                              BufferAllocation::Slice dst_slice);

  tsl::AsyncValueRef<ExecuteEvent> Execute(const ExecuteParams&) final;

  BufferUses buffer_uses() const final;
  ResourceUses resource_uses() const final;

 private:
  std::vector<BufferAllocation::Slice> srcs_;
  std::vector<BufferAllocation::Slice> dsts_;
  std::vector<std::string>* trace_;
  bool use_shared_resource_;
  bool inject_error_;
};

std::unique_ptr<Thunk> AddI32Thunk::Create(
    std::string name, std::vector<BufferAllocation::Slice> srcs,
    std::vector<BufferAllocation::Slice> dsts, std::vector<std::string>* trace,
    bool use_shared_resource, bool inject_error) {
  return std::make_unique<AddI32Thunk>(std::move(name), std::move(srcs),
                                       std::move(dsts), trace,
                                       use_shared_resource, inject_error);
}

std::vector<MaybeOwningDeviceMemory> AddI32Thunk::AsDeviceMemory(
    absl::Span<std::vector<int32_t>* const> data) {
  std::vector<MaybeOwningDeviceMemory> buffers;
  for (auto& vec : data) {
    buffers.emplace_back(
        se::DeviceMemoryBase(vec->data(), vec->size() * sizeof(int32_t)));
  }
  return buffers;
}

AddI32Thunk::AddI32Thunk(std::string name,
                         std::vector<BufferAllocation::Slice> srcs,
                         std::vector<BufferAllocation::Slice> dsts,
                         std::vector<std::string>* trace,
                         bool use_shared_resource, bool inject_error)
    : Thunk(Kind::kKernel, Info{name}),
      srcs_(std::move(srcs)),
      dsts_(std::move(dsts)),
      trace_(trace),
      use_shared_resource_(use_shared_resource),
      inject_error_(inject_error) {}

absl::Status AddI32Thunk::Execute(const BufferAllocations* allocations,
                                  BufferAllocation::Slice src_slice,
                                  BufferAllocation::Slice dst_slice) {
  TF_ASSIGN_OR_RETURN(se::DeviceMemoryBase src,
                      allocations->GetDeviceAddress(src_slice));

  TF_ASSIGN_OR_RETURN(se::DeviceMemoryBase dst,
                      allocations->GetDeviceAddress(dst_slice));

  CHECK_EQ(src.size() % sizeof(int32_t), 0);
  CHECK_EQ(dst.size() % sizeof(int32_t), 0);

  int32_t* src_ptr = static_cast<int32_t*>(src.opaque());
  int32_t* dst_ptr = static_cast<int32_t*>(dst.opaque());
  size_t len = std::min(src.size(), dst.size()) / sizeof(int32_t);

  for (int j = 0; j < len; ++j) dst_ptr[j] += src_ptr[j];

  return absl::OkStatus();
}

tsl::AsyncValueRef<Thunk::ExecuteEvent> AddI32Thunk::Execute(
    const ExecuteParams& params) {
  if (trace_) trace_->push_back(info().op_name);

  auto execute = [&]() -> absl::Status {
    CHECK_EQ(srcs_.size(), dsts_.size());
    for (int i = 0; i < srcs_.size(); ++i) {
      TF_RETURN_IF_ERROR(
          Execute(params.buffer_allocations, srcs_.at(i), dsts_.at(i)));
    }
    return absl::OkStatus();
  };

  // Offload the execution to the intra-op thread pool.
  if (params.intra_op_threadpool) {
    auto event = tsl::MakeConstructedAsyncValueRef<ExecuteEvent>();
    params.intra_op_threadpool->getPool()->Schedule([&, event, execute] {
      if (use_shared_resource_) {
        shared_resource++;
      }

      if (inject_error_) {
        event.SetError(absl::InternalError("Injected error"));
      } else {
        CHECK_OK(execute());
        event.SetStateConcrete();
      }
    });
    return event;
  }

  if (use_shared_resource_) {
    shared_resource++;
  }

  if (inject_error_) {
    return tsl::MakeErrorAsyncValueRef(absl::InternalError("Injected error"));
  }

  TF_RETURN_IF_ERROR(execute());
  return Thunk::OkExecuteEvent();
}

AddI32Thunk::BufferUses AddI32Thunk::buffer_uses() const {
  BufferUses buffer_uses;
  for (const auto& src : srcs_) buffer_uses.push_back(BufferUse::Read(src));
  for (const auto& dst : dsts_) buffer_uses.push_back(BufferUse::Write(dst));
  return buffer_uses;
}

AddI32Thunk::ResourceUses AddI32Thunk::resource_uses() const {
  static std::shared_ptr<Resource>* shared_resource =
      new std::shared_ptr<Resource>(Resource::Create(Resource::kToken));

  return use_shared_resource_
             ? ResourceUses{ResourceUse::Write(*shared_resource)}
             : ResourceUses{};
}

static ThunkExecutor::Options OptionsForTest() {
  // Override small buffers threshold to make sure that we test all execution
  // paths, because in test we always use small buffers below the default
  // threshold of `512`.
  return ThunkExecutor::Options{/*execute_sequential_buffer_threshold=*/0};
}

TEST(ThunkExecutorTest, FifoReadyQueueTest) {
  ThunkExecutor::FifoReadyQueue queue({});

  // Check basic queue properties.
  EXPECT_TRUE(queue.Empty());
  EXPECT_EQ(queue.Size(), 0);

  queue.Push(1);
  queue.Push(2);
  queue.Push(3);

  EXPECT_EQ(queue.Size(), 3);

  EXPECT_EQ(queue.Pop(), 1);
  EXPECT_EQ(queue.Pop(), 2);
  EXPECT_EQ(queue.Pop(), 3);

  EXPECT_TRUE(queue.Empty());
  EXPECT_EQ(queue.Size(), 0);

  // Prepare queue for PopHalf test case.
  queue.Push(1);
  queue.Push(2);
  queue.Push(3);

  // Pop half of the queue.
  ThunkExecutor::FifoReadyQueue half0 = queue.PopHalf();
  EXPECT_EQ(half0.Size(), 2);
  EXPECT_EQ(half0.Pop(), 2);
  EXPECT_EQ(half0.Pop(), 3);

  // Check that the rest is still in the queue.
  EXPECT_EQ(queue.Size(), 1);

  // Pop the rest of the queue.
  ThunkExecutor::FifoReadyQueue half1 = queue.PopHalf();
  EXPECT_EQ(half1.Size(), 1);

  // Check that all nodes were returned from PopHalf.
  EXPECT_EQ(queue.Size(), 0);

  // Add 5 elements to test Pop followed by PopHalf.
  queue.Push(1);
  queue.Push(2);
  queue.Push(3);
  queue.Push(4);
  queue.Push(5);

  EXPECT_EQ(queue.Pop(), 1);

  // Check that PopHalf returns 2 last nodes.
  ThunkExecutor::FifoReadyQueue half2 = queue.PopHalf();
  EXPECT_EQ(half2.Size(), 2);
  EXPECT_EQ(half2.Pop(), 4);
  EXPECT_EQ(half2.Pop(), 5);
}

TEST(ThunkExecutorTest, PriorityReadyQueueTest) {
  std::vector<ThunkExecutor::NodeDef> nodes_defs(16);
  for (size_t i = 0; i < nodes_defs.size(); ++i) {
    nodes_defs[i].priority = i;
  }

  ThunkExecutor::PriorityReadyQueue queue(nodes_defs, {});
  // Check basic queue properties.
  EXPECT_TRUE(queue.Empty());
  EXPECT_EQ(queue.Size(), 0);

  queue.Push(1);
  queue.Push(3);
  queue.Push(2);

  EXPECT_EQ(queue.Pop(), 3);
  EXPECT_EQ(queue.Pop(), 2);
  EXPECT_EQ(queue.Pop(), 1);

  EXPECT_TRUE(queue.Empty());
  EXPECT_EQ(queue.Size(), 0);

  // Prepare queue for PopHalf test case.
  queue.Push(2);
  queue.Push(1);
  queue.Push(3);

  // Pop half of the queue.
  ThunkExecutor::PriorityReadyQueue half0 = queue.PopHalf();
  EXPECT_EQ(half0.Size(), 2);
  EXPECT_EQ(half0.Pop(), 2);
  EXPECT_EQ(half0.Pop(), 1);

  // Check that the rest is still in the queue.
  EXPECT_EQ(queue.Size(), 1);

  // Pop the rest of the queue.
  ThunkExecutor::PriorityReadyQueue half1 = queue.PopHalf();
  EXPECT_EQ(half1.Size(), 1);
  EXPECT_EQ(half1.Pop(), 3);

  // Check that all nodes were returned from PopHalf.
  EXPECT_EQ(queue.Size(), 0);

  // Add 5 elements to test Pop followed by PopHalf.
  queue.Push(4);
  queue.Push(3);
  queue.Push(5);
  queue.Push(1);
  queue.Push(2);

  EXPECT_EQ(queue.Pop(), 5);

  // Check that PopHalf returns 2 last nodes.
  ThunkExecutor::PriorityReadyQueue half2 = queue.PopHalf();
  EXPECT_EQ(half2.Size(), 2);
  EXPECT_EQ(half2.Pop(), 2);
  EXPECT_EQ(half2.Pop(), 1);
}

TEST(ThunkExecutorTest, DependencyOrdering) {
  BufferAllocation alloc(/*index=*/0, /*size=*/80, /*color=*/0);

  BufferAllocation::Slice slice0(&alloc, /*offset=*/0, /*size=*/40);
  BufferAllocation::Slice slice1(&alloc, /*offset=*/40, /*size=*/40);
  BufferAllocation::Slice slice2(&alloc, /*offset=*/20, /*size=*/40);

  ThunkSequence sequence;
  sequence.push_back(AddI32Thunk::Create("a", {slice0}, {slice0}));
  sequence.push_back(AddI32Thunk::Create("b", {slice1}, {slice1}));
  sequence.push_back(AddI32Thunk::Create("c", {slice2}, {slice2}));

  TF_ASSERT_OK_AND_ASSIGN(
      ThunkExecutor executor,
      ThunkExecutor::Create(std::move(sequence), OptionsForTest()));

  EXPECT_FALSE(executor.is_sequential());
  EXPECT_THAT(executor.source(), ElementsAre(0, 1));
  EXPECT_THAT(executor.sink(), ElementsAre(2));

  EXPECT_EQ(executor.node_def(0).priority, 1);
  EXPECT_EQ(executor.node_def(1).priority, 1);
  EXPECT_EQ(executor.node_def(2).priority, 0);
}

TEST(ThunkExecutorTest, SequentialOrdering) {
  BufferAllocation alloc(/*index=*/0, /*size=*/80, /*color=*/0);
  BufferAllocation::Slice slice(&alloc, /*offset=*/0, /*size=*/40);

  ThunkSequence sequence;
  sequence.push_back(AddI32Thunk::Create("a", {slice}, {slice}));
  sequence.push_back(AddI32Thunk::Create("b", {slice}, {slice}));
  sequence.push_back(AddI32Thunk::Create("c", {slice}, {slice}));

  TF_ASSERT_OK_AND_ASSIGN(
      ThunkExecutor executor,
      ThunkExecutor::Create(std::move(sequence), OptionsForTest()));

  EXPECT_TRUE(executor.is_sequential());
  EXPECT_THAT(executor.source(), ElementsAre(0));
  EXPECT_THAT(executor.sink(), ElementsAre(2));

  EXPECT_EQ(executor.node_def(0).priority, 2);
  EXPECT_EQ(executor.node_def(1).priority, 1);
  EXPECT_EQ(executor.node_def(2).priority, 0);
}

TEST(ThunkExecutorTest, ResourceOrdering) {
  BufferAllocation alloc(/*index=*/0, /*size=*/80, /*color=*/0);

  BufferAllocation::Slice slice0(&alloc, /*offset=*/0, /*size=*/40);
  BufferAllocation::Slice slice1(&alloc, /*offset=*/40, /*size=*/40);

  ThunkSequence sequence;
  sequence.push_back(AddI32Thunk::Create("a", {slice0}, {slice0},
                                         /*trace=*/nullptr,
                                         /*use_shared_resource=*/true));
  sequence.push_back(AddI32Thunk::Create("b", {slice1}, {slice1},
                                         /*trace=*/nullptr,
                                         /*use_shared_resource=*/true));

  TF_ASSERT_OK_AND_ASSIGN(
      ThunkExecutor executor,
      ThunkExecutor::Create(std::move(sequence), OptionsForTest()));

  EXPECT_TRUE(executor.is_sequential());
  EXPECT_THAT(executor.source(), ElementsAre(0));
  EXPECT_THAT(executor.sink(), ElementsAre(1));

  EXPECT_EQ(executor.node_def(0).priority, 1);
  EXPECT_EQ(executor.node_def(1).priority, 0);
}

TEST(ThunkExecutorTest, TransitiveReduction) {
  BufferAllocation alloc(/*index=*/0, /*size=*/80, /*color=*/0);
  BufferAllocation::Slice slice(&alloc, /*offset=*/0, /*size=*/40);

  ThunkSequence sequence;
  sequence.push_back(AddI32Thunk::Create("a", {slice}, {slice}));
  sequence.push_back(AddI32Thunk::Create("b", {slice}, {slice}));
  sequence.push_back(AddI32Thunk::Create("c", {slice}, {slice}));

  TF_ASSERT_OK_AND_ASSIGN(
      ThunkExecutor executor,
      ThunkExecutor::Create(std::move(sequence), OptionsForTest()));

  EXPECT_THAT(executor.source(), ElementsAre(0));
  EXPECT_THAT(executor.sink(), ElementsAre(2));

  EXPECT_THAT(executor.node_def(0).out_edges, ElementsAre(1));
  EXPECT_THAT(executor.node_def(1).in_edges, ElementsAre(0));
  EXPECT_THAT(executor.node_def(1).out_edges, ElementsAre(2));
  EXPECT_THAT(executor.node_def(2).in_edges, ElementsAre(1));

  EXPECT_EQ(executor.node_def(0).priority, 2);
  EXPECT_EQ(executor.node_def(1).priority, 1);
  EXPECT_EQ(executor.node_def(2).priority, 0);
}

TEST(ThunkExecutorTest, Execute) {
  BufferAllocation alloc(/*index=*/0, /*size=*/80, /*color=*/0);

  BufferAllocation::Slice slice0(&alloc, /*offset=*/0, /*size=*/40);
  BufferAllocation::Slice slice1(&alloc, /*offset=*/40, /*size=*/40);
  BufferAllocation::Slice slice2(&alloc, /*offset=*/20, /*size=*/40);

  std::vector<std::string> trace;

  ThunkSequence sequence;
  sequence.push_back(AddI32Thunk::Create("a", {slice0}, {slice0}, &trace));
  sequence.push_back(AddI32Thunk::Create("b", {slice1}, {slice1}, &trace));
  sequence.push_back(AddI32Thunk::Create("c", {slice2}, {slice2}, &trace));

  TF_ASSERT_OK_AND_ASSIGN(
      ThunkExecutor executor,
      ThunkExecutor::Create(std::move(sequence), OptionsForTest()));

  std::vector<int32_t> data(20, 1);  // shared src and dst allocation

  auto buffers = AddI32Thunk::AsDeviceMemory({&data});
  BufferAllocations allocations(buffers);

  Thunk::TaskRunner task_runner = [&](Thunk::Task task) {
    trace.push_back("<TaskRunner>");
    task();
  };

  Thunk::ExecuteParams params = {nullptr, &allocations};
  params.task_runner = &task_runner;
  params.session =
      Thunk::ExecuteSession(/*max_workers=*/8, /*split_threshold=*/0);

  auto execute_event = executor.Execute(params);

  tsl::BlockUntilReady(execute_event);
  ASSERT_TRUE(execute_event.IsConcrete());

  EXPECT_THAT(trace, ElementsAre("<TaskRunner>", "b", "a", "c"));
  EXPECT_THAT(data, ElementsAre(2, 2, 2, 2, 2,                 // slice0
                                4, 4, 4, 4, 4, 4, 4, 4, 4, 4,  // slice2
                                2, 2, 2, 2, 2));               // slice1
}

//===----------------------------------------------------------------------===//
// ThunkExecutor stress testing
//===----------------------------------------------------------------------===//

// We generate random thunk sequences that may or may not use a shared resource.
enum class SharedResourceUse { kNo, kAll, kRandom };

struct GeneratedThunkSequence {
  BufferAllocation src_alloc;
  BufferAllocation dst_alloc;

  std::vector<int32_t> src;
  std::vector<int32_t> dst;
  std::vector<int32_t> expected;

  int32_t expected_shared_resource_value;

  std::vector<MaybeOwningDeviceMemory> expected_buffers;
  std::vector<MaybeOwningDeviceMemory> buffers;

  ThunkSequence sequence;
};

static absl::StatusOr<std::unique_ptr<GeneratedThunkSequence>>
GenerateThunkSequence(size_t num_elements, size_t num_thunks,
                      SharedResourceUse shared_resource_use,
                      bool inject_errors) {
  auto g = std::make_unique<GeneratedThunkSequence>(GeneratedThunkSequence{
      BufferAllocation(/*index=*/0, num_elements * sizeof(int32_t), 0),
      BufferAllocation(/*index=*/1, num_elements * sizeof(int32_t), 0),
      /*src=*/std::vector<int32_t>(num_elements, 1),
      /*dst=*/std::vector<int32_t>(num_elements, 0),
      /*expected=*/std::vector<int32_t>(num_elements, 0),
      /*expected_shared_resource_value=*/0,
  });

  g->sequence.reserve(num_thunks);
  g->expected_buffers = AddI32Thunk::AsDeviceMemory({&g->src, &g->expected});
  g->buffers = AddI32Thunk::AsDeviceMemory({&g->src, &g->dst});

  std::minstd_rand0 engine;

  std::uniform_int_distribution<size_t> offset_dist(0, num_elements - 1);
  std::uniform_int_distribution<size_t> size_dist(32, 64);
  std::uniform_int_distribution<size_t> use_resource_dist(0, num_thunks / 10);
  std::uniform_int_distribution<size_t> inject_error_dist(0, num_thunks / 10);

  // Returns a random slice of the allocation.
  auto random_slice = [&](BufferAllocation* alloc) {
    size_t start = offset_dist(engine);
    size_t size = std::min(num_elements - start, size_dist(engine));
    return BufferAllocation::Slice(alloc, start * sizeof(int32_t),
                                   size * sizeof(int32_t));
  };

  for (int i = 0; i < num_thunks; ++i) {
    BufferAllocation::Slice src = random_slice(&g->src_alloc);
    BufferAllocation::Slice dst = random_slice(&g->dst_alloc);

    // Pre-compute expected result while building the thunk sequence.
    BufferAllocations allocations(g->expected_buffers);
    TF_RETURN_IF_ERROR(AddI32Thunk::Execute(&allocations, src, dst));

    bool use_resource = [&] {
      switch (shared_resource_use) {
        case SharedResourceUse::kNo:
          return false;
        case SharedResourceUse::kAll:
          return true;
        case SharedResourceUse::kRandom:
          return use_resource_dist(engine) == 0;
      }
    }();
    if (use_resource) g->expected_shared_resource_value++;

    bool inject_error = inject_errors && inject_error_dist(engine) == 0;
    g->sequence.push_back(AddI32Thunk::Create(absl::StrCat(i), {src}, {dst},
                                              /*trace=*/nullptr, use_resource,
                                              inject_error));
  }

  return g;
}

// Parameterized thunk executor stress tests that builds a random thunk sequence
// and optionally uses a thread pool to execute thunk executor tasks.
class ThunkExecutorStressTest
    : public testing::TestWithParam<
          std::tuple<int32_t, bool, bool, SharedResourceUse, bool, bool>> {
 public:
  void SetUp() override {
    auto& [num_thunks, use_task_runner, use_device, shared_resource_use,
           inject_errors, use_priority_ready_queue] = GetParam();

    use_task_runner_ = use_task_runner;
    use_device_ = use_device;

    // Both the task runner and the intra-op device share the same underlying
    // thread pool, and we test that they do not deadlock each other and
    // everything works via chaining together asynchronous events. It is a
    // common source of deadlocks to wait for the completion of tasks scheduled
    // into the same thread pool where awaiting thread is executing.
    if (use_task_runner_ || use_device_) {
      thread_pool_.emplace(tsl::Env::Default(), "thunk-executor", 8);
      device_.emplace(thread_pool_->AsEigenThreadPool(),
                      thread_pool_->NumThreads());
      task_runner_.emplace([this](Thunk::Task task) {
        thread_pool_->Schedule(std::move(task));
      });
    }
  }

  Thunk::TaskRunner* task_runner() {
    if (!use_task_runner_) return nullptr;
    return &*task_runner_;
  }

  Eigen::ThreadPoolDevice* device() {
    if (!use_device_) return nullptr;
    return &*device_;
  }

 private:
  bool use_task_runner_;
  bool use_device_;
  std::optional<tsl::thread::ThreadPool> thread_pool_;
  std::optional<Eigen::ThreadPoolDevice> device_;
  std::optional<Thunk::TaskRunner> task_runner_;
};

TEST_P(ThunkExecutorStressTest, Execute) {
  auto [num_thunks, use_task_runner, use_device, shared_resource_use,
        inject_errors, use_priority_ready_queue] = GetParam();

  TF_ASSERT_OK_AND_ASSIGN(
      std::unique_ptr<GeneratedThunkSequence> g,
      GenerateThunkSequence(/*num_elements=*/1024, num_thunks,
                            shared_resource_use, inject_errors));

  ThunkExecutor::Options executor_options = {
      /*execute_sequential_buffer_threshold=*/0,
      /*use_priority_ready_queue=*/use_priority_ready_queue,
  };

  TF_ASSERT_OK_AND_ASSIGN(
      ThunkExecutor executor,
      ThunkExecutor::Create(std::move(g->sequence), executor_options));

  BufferAllocations allocations(g->buffers);
  Thunk::ExecuteParams params = {nullptr, &allocations, nullptr, device(),
                                 task_runner()};

  shared_resource = 0;

  auto execute_event = executor.Execute(params);
  tsl::BlockUntilReady(execute_event);

  if (inject_errors) {
    ASSERT_TRUE(execute_event.IsError());
    EXPECT_EQ(execute_event.GetError(), absl::InternalError("Injected error"));
  } else {
    ASSERT_TRUE(execute_event.IsConcrete());
    EXPECT_EQ(shared_resource, g->expected_shared_resource_value);
    EXPECT_EQ(g->dst, g->expected);
  }
}

INSTANTIATE_TEST_SUITE_P(
    ThunkExecutor, ThunkExecutorStressTest,
    testing::Combine(/*num_thunks=*/testing::ValuesIn({10, 100, 1000}),
                     /*use_task_runner=*/testing::Bool(),
                     /*use_device=*/testing::Bool(),
                     /*shared_resource_use=*/
                     testing::Values(SharedResourceUse::kNo,
                                     SharedResourceUse::kAll,
                                     SharedResourceUse::kRandom),
                     /*inject_errors=*/testing::Bool(),
                     /*use_priority_ready_queue=*/testing::Bool()));

//===----------------------------------------------------------------------===//
// Performance benchmarks below
//===----------------------------------------------------------------------===//

static void BM_FifoReadyQueuePushPop(benchmark::State& state) {
  ThunkExecutor::FifoReadyQueue queue({});
  const size_t num_push_pop = state.range(0);

  for (auto _ : state) {
    for (int i = 0; i < num_push_pop; ++i) {
      queue.Push(i);
    }
    for (int i = 0; i < num_push_pop; ++i) {
      benchmark::DoNotOptimize(queue.Pop());
    }
  }
}

static void BM_FifoReadyQueuePushPopHalf(benchmark::State& state) {
  ThunkExecutor::FifoReadyQueue queue({});
  const size_t num_push_pop = state.range(0);

  for (auto _ : state) {
    for (int i = 0; i < num_push_pop; ++i) {
      queue.Push(i);
    }
    benchmark::DoNotOptimize(queue.PopHalf());
  }
}

static void BM_PriorityReadyQueuePushPop(benchmark::State& state) {
  std::vector<ThunkExecutor::NodeDef> nodes_defs(16);
  for (size_t i = 0; i < nodes_defs.size(); ++i) {
    nodes_defs[i].priority = i;
  }

  std::default_random_engine rng;
  absl::c_shuffle(nodes_defs, rng);

  ThunkExecutor::PriorityReadyQueue queue(nodes_defs, {});
  const size_t num_push_pop = state.range(0);

  for (auto _ : state) {
    for (int i = 0; i < num_push_pop; ++i) {
      queue.Push(i);
    }
    for (int i = 0; i < num_push_pop; ++i) {
      benchmark::DoNotOptimize(queue.Pop());
    }
  }
}

static void BM_PriorityReadyQueuePushPopHalf(benchmark::State& state) {
  std::vector<ThunkExecutor::NodeDef> nodes_defs(16);
  for (size_t i = 0; i < nodes_defs.size(); ++i) {
    nodes_defs[i].priority = i;
  }

  std::default_random_engine rng;
  absl::c_shuffle(nodes_defs, rng);

  ThunkExecutor::PriorityReadyQueue queue(nodes_defs, {});
  const size_t num_push_pop = state.range(0);

  for (auto _ : state) {
    for (int i = 0; i < num_push_pop; ++i) {
      queue.Push(i);
    }
    benchmark::DoNotOptimize(queue.PopHalf());
  }
}

#define BENCHMARK_READY_QUEUE(name) \
  BENCHMARK(name)                   \
      ->MeasureProcessCPUTime()     \
      ->Arg(1)                      \
      ->Arg(2)                      \
      ->Arg(4)                      \
      ->Arg(8)                      \
      ->Arg(16)

BENCHMARK_READY_QUEUE(BM_FifoReadyQueuePushPop);
BENCHMARK_READY_QUEUE(BM_FifoReadyQueuePushPopHalf);
BENCHMARK_READY_QUEUE(BM_PriorityReadyQueuePushPop);
BENCHMARK_READY_QUEUE(BM_PriorityReadyQueuePushPopHalf);

static void BM_CreateThunkExecutor(benchmark::State& state) {
  const size_t num_thunks = state.range(0);

  for (auto _ : state) {
    auto g = GenerateThunkSequence(/*num_elements=*/1024, num_thunks,
                                   SharedResourceUse::kNo, false);
    CHECK_OK(ThunkExecutor::Create(std::move((*g)->sequence), OptionsForTest())
                 .status());
  }
}

static void BM_SequentialThunkExecutor(benchmark::State& state) {
  const size_t num_thunks = state.range(0);

  auto g =
      GenerateThunkSequence(/*num_elements=*/1024, num_thunks,
                            /*shared_resource_use=*/SharedResourceUse::kAll,
                            /*inject_errors=*/false)
          .value();
  auto e =
      ThunkExecutor::Create(std::move(g->sequence), OptionsForTest()).value();

  BufferAllocations allocations(g->buffers);
  Thunk::ExecuteParams params = {nullptr, &allocations};

  for (auto _ : state) {
    auto execute_event = e.Execute(params);
    tsl::BlockUntilReady(execute_event);
    CHECK(execute_event.IsConcrete());
  }
}

static void BM_SyncThunkExecutor(benchmark::State& state) {
  const size_t num_thunks = state.range(0);

  auto g = GenerateThunkSequence(/*num_elements=*/1024, num_thunks,
                                 /*shared_resource_use=*/SharedResourceUse::kNo,
                                 /*inject_errors=*/false)
               .value();
  auto e =
      ThunkExecutor::Create(std::move(g->sequence), OptionsForTest()).value();

  BufferAllocations allocations(g->buffers);
  Thunk::ExecuteParams params = {nullptr, &allocations};

  for (auto _ : state) {
    auto execute_event = e.Execute(params);
    tsl::BlockUntilReady(execute_event);
    CHECK(execute_event.IsConcrete());
  }
}

static void BM_AsyncThunkExecutor(benchmark::State& state) {
  const size_t num_thunks = state.range(0);

  tsl::thread::ThreadPool thread_pool(tsl::Env::Default(), "thunk-executor", 8);
  Eigen::ThreadPoolDevice device(thread_pool.AsEigenThreadPool(),
                                 thread_pool.NumThreads());

  auto g = GenerateThunkSequence(/*num_elements=*/1024, num_thunks,
                                 /*shared_resource_use=*/SharedResourceUse::kNo,
                                 /*inject_errors=*/false)
               .value();
  auto e =
      ThunkExecutor::Create(std::move(g->sequence), OptionsForTest()).value();

  BufferAllocations allocations(g->buffers);

  Thunk::TaskRunner task_runner = [&](Thunk::Task task) {
    thread_pool.Schedule(std::move(task));
  };

  Thunk::ExecuteParams params = {nullptr, &allocations, nullptr, &device,
                                 &task_runner};

  for (auto _ : state) {
    auto execute_event = e.Execute(params);
    tsl::BlockUntilReady(execute_event);
    CHECK(execute_event.IsConcrete());
  }
}

#define BENCHMARK_THUNK_EXECUTOR(name) \
  BENCHMARK(name)                      \
      ->MeasureProcessCPUTime()        \
      ->Arg(1)                         \
      ->Arg(2)                         \
      ->Arg(4)                         \
      ->Arg(8)                         \
      ->Arg(16)                        \
      ->Arg(32)                        \
      ->Arg(64)                        \
      ->Arg(128)                       \
      ->Arg(256)                       \
      ->Arg(512)

BENCHMARK_THUNK_EXECUTOR(BM_CreateThunkExecutor);
BENCHMARK_THUNK_EXECUTOR(BM_SequentialThunkExecutor);
BENCHMARK_THUNK_EXECUTOR(BM_SyncThunkExecutor);
BENCHMARK_THUNK_EXECUTOR(BM_AsyncThunkExecutor);

}  // namespace
}  // namespace xla::cpu
