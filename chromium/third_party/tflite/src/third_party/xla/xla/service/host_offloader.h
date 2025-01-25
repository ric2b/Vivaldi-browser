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
#ifndef XLA_SERVICE_HOST_OFFLOADER_H_
#define XLA_SERVICE_HOST_OFFLOADER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/service/hlo_alias_analysis.h"
#include "xla/service/hlo_buffer.h"
#include "xla/service/hlo_pass_interface.h"

namespace xla {

class HloCostAnalysis;

struct InstructionAndShapeIndex {
  explicit InstructionAndShapeIndex(HloInstruction* instruction)
      : instruction(instruction) {}
  InstructionAndShapeIndex(HloInstruction* instruction, ShapeIndex shape_index)
      : instruction(instruction), shape_index(shape_index) {}
  HloInstruction* instruction;
  ShapeIndex shape_index;
  std::string ToString() const;

  template <typename H>
  static H Hash(H h, const InstructionAndShapeIndex& i) {
    h = H::combine(std::move(h), i.instruction);
    h = H::combine(std::move(h), i.shape_index);
    return std::move(h);
  }

  template <typename H>
  friend H AbslHashValue(H h, const InstructionAndShapeIndex& i) {
    return InstructionAndShapeIndex::Hash(std::move(h), i);
  }
};

bool operator==(const InstructionAndShapeIndex& lhs,
                const InstructionAndShapeIndex& rhs);

// This pass does "host memory offloading". If a tensor is annotated to be moved
// to or from the host, this pass will remove the annotations and update each
// tensor's layout with host memory spaces and insert copies if necessary. This
// pass checks to make sure that no compute is done on the tensors annotated for
// host memory offload; if there is compute, it is considered a user error and
// an error will be returned.
// The pass will "walk down" the Hlo graph starting from either MoveToHost
// custom calls or from parameters with host memory space in their layout. All
// tensors along each path have their memory space set as host memory space. If
// a MoveToHost custom call is paired with a DynamicUpdateSlice, the
// DynamicUpdateSlice will write into host memory space. Otherwise, a copy from
// device to host will be inserted. All MoveToHost and MoveToDevice custom calls
// are removed by the end of this pass.
class HostOffloader : public HloModulePass {
 public:
  explicit HostOffloader(int64_t host_memory_space_color)
      : kHostMemorySpaceColor(host_memory_space_color) {}
  ~HostOffloader() override = default;

  absl::string_view name() const override { return "host-offloader"; }

  using HloPassInterface::Run;
  absl::StatusOr<bool> Run(
      HloModule* module,
      const absl::flat_hash_set<absl::string_view>& execution_threads) override;

 private:
  const int64_t kHostMemorySpaceColor;
  absl::flat_hash_set<HloInstruction*>
      already_visited_move_to_host_custom_calls_;
  absl::flat_hash_set<HloInstruction*> dynamic_update_slices_already_allocated_;
  absl::flat_hash_set<HloInstruction*> validated_slices_;
  absl::flat_hash_map<HloInstruction*, HloInstruction*> copies_created_after_;
  absl::flat_hash_set<HloInstruction*> move_to_device_custom_calls_to_remove_;
  absl::flat_hash_set<InstructionAndShapeIndex> already_inserted_copy_before_;

  // Sometimes previous transformations turn a DynamicSlice into a Slice. Since
  // we're doing a DMA between the host and device, we need to turn the Slice
  // back into a DynamicSlice.
  absl::StatusOr<HloInstruction*> DynamifySlice(HloInstruction* slice);

  // Returns true if the instruction is allowed to be in the
  // middle of a pure memory offload path.
  bool IsValidDuringPureMemoryOffload(const HloInstruction* instruction) const;

  // Returns true if the instruction is allowed to be in the
  // middle of a path between a MoveToHost custom-call annotation and a
  // DynamicUpdateSlice. Ideally the custom-call should be immediately followed
  // by the DynamicUpdateSlice, but this is not always the case.
  bool InstructionIsAllowedBetweenMoveToHostAndDus(
      const HloInstruction* instruction) const;

  // Returns true if the instruction is allowed to be in the
  // middle of a path between a DynamicSlice and a MoveToDevice custom-call
  // annotation. Ideally the DynamicSlice should be immediately followed by the
  // custom-call, but this is not always the case.
  bool InstructionIsAllowedBetweenDsAndMoveToDevice(
      const HloInstruction* instruction) const;

  // Walks down the graph and does "host memory offloading" starting from every
  // host memory parameter in the entry computation.
  absl::StatusOr<bool> HandleInputStreaming(HloComputation* entry_computation);

  // Walks down the graph and does "host memory offloading" starting from every
  // MoveToHost custom call.
  absl::StatusOr<bool> HandleMoveToHostCustomCall(
      HloInstruction* custom_call_instruction);

  // Since we always walk the graph from the top down, this function only needs
  // to remove these lingering custom calls. This function should only be called
  // once all host memory offloading is done because multiple paths might lead
  // to the same MoveToDevice custom call. Removing it too early will confuse
  // subsequent walkings of the graph.
  absl::StatusOr<bool> HandleMoveToDeviceCustomCall(
      HloInstruction* custom_call_instruction);

  // DynamicUpdateSlices which write into host memory must have their
  // destination buffer allocated on the host. This function creates the
  // allocation and updates all positions to have host memory space.
  absl::Status CreateAllocateBufferForDynamicUpdateSlice(
      HloInstruction* dynamic_update_slice);

  // Returns an error if something unallowed exists between the
  // Slice/DynamicSlice and the MoveToDevice custom call.
  absl::Status ValidateSliceLeadsToMoveToDeviceCustomCall(
      HloInstruction* slice);

  // Common function for doing the actual walking of the graph. Host memory
  // spaces are set and copies are inserted in here.
  absl::StatusOr<bool> WalkDownHostMemoryOffloadPaths(
      const InstructionAndShapeIndex& starting_instruction_and_index,
      bool insert_copy_before);

  // Given a custom call, this returns the first instruction and shape index to
  // start the host memory offload path from for each use of the custom call.
  absl::StatusOr<std::vector<InstructionAndShapeIndex>> GetStartingInstructions(
      HloInstruction* custom_call_instruction);

  // When a MoveToHost custom call is not paired with a DynamicUpdateSlice, a
  // copy from device to host must be inserted.
  absl::StatusOr<bool> InsertCopyBetween(
      const InstructionAndShapeIndex& before_instruction_and_index,
      const InstructionAndShapeIndex& after_instruction_and_index);

  // This is a fix for scheduling. Add copies to inputs of dynamic-update-slice
  // if the inserted value is directly a parameter of a computation. This is to
  // avoid cases in while loop where parameter/output aliasing can stop
  // scheduling because control-dependencies are added.
  absl::StatusOr<bool> ApplySchedulingFix(
      HloModule* module,
      const absl::flat_hash_set<absl::string_view>& execution_threads);
};

}  // namespace xla

#endif  // XLA_SERVICE_HOST_OFFLOADER_H_
