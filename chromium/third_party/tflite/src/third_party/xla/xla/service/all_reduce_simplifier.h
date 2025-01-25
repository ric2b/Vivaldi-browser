/* Copyright 2019 The OpenXLA Authors.

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

#ifndef XLA_SERVICE_ALL_REDUCE_SIMPLIFIER_H_
#define XLA_SERVICE_ALL_REDUCE_SIMPLIFIER_H_

#include "absl/status/statusor.h"
#include "xla/hlo/ir/hlo_module.h"
#include "xla/service/hlo_pass_interface.h"

namespace xla {

// A pass that detects all-reduces whose inputs are already the same across
// replicas using the replication analysis, then replaces those all-reduces with
// local computations. E.g., a sum all-reduce on replicated input will be
// replaced by a multiply with the replica count.
class AllReduceSimplifier : public HloModulePass {
 public:
  explicit AllReduceSimplifier(int64_t replica_count)
      : replica_count_(replica_count) {}
  ~AllReduceSimplifier() override = default;
  absl::string_view name() const override { return "all-reduce-simp"; }

  // Run all-reduce simplification on the given computation. Returns whether the
  // computation was changed.
  using HloPassInterface::Run;
  absl::StatusOr<bool> Run(
      HloModule* module,
      const absl::flat_hash_set<absl::string_view>& execution_threads) override;

 private:
  int64_t replica_count_;
};

}  // namespace xla

#endif  // XLA_SERVICE_ALL_REDUCE_SIMPLIFIER_H_
