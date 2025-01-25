/* Copyright 2021 The OpenXLA Authors.

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

#include "xla/service/async_collective_creator.h"

#include <cstdint>
#include <iterator>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/log/log.h"
#include "xla/frontend_attributes.h"
#include "xla/hlo/ir/hlo_casting_utils.h"
#include "xla/hlo/ir/hlo_computation.h"
#include "xla/hlo/ir/hlo_instruction.h"
#include "xla/hlo/ir/hlo_instructions.h"
#include "xla/hlo/ir/hlo_opcode.h"
#include "xla/hlo/ir/hlo_schedule.h"
#include "xla/service/shape_inference.h"
#include "xla/util.h"
#include "tsl/platform/errors.h"

namespace xla {
namespace {

struct ReplacedAsync {
  HloInstruction* start;
  HloInstruction* done;
};

absl::StatusOr<ReplacedAsync> CreateAsyncAllReduce(
    HloInstruction* instruction) {
  HloComputation* computation = instruction->parent();
  auto* ar = Cast<HloAllReduceInstruction>(instruction);
  HloInstruction* start =
      computation->AddInstruction(HloInstruction::CreateAllReduceStart(
          ar->shape(), ar->operands(), ar->to_apply(), ar->device_list(),
          ar->constrain_layout(), ar->channel_id(),
          ar->use_global_device_ids()));
  HloInstruction* done =
      computation->AddInstruction(HloInstruction::CreateUnary(
          ar->shape(), HloOpcode::kAllReduceDone, start));
  return ReplacedAsync{start, done};
}

absl::StatusOr<ReplacedAsync> CreateAsyncAllGather(
    HloInstruction* instruction) {
  HloComputation* computation = instruction->parent();
  auto* ag = Cast<HloAllGatherInstruction>(instruction);
  std::vector<const Shape*> operand_shapes;
  operand_shapes.reserve(ag->operand_count());
  for (const HloInstruction* op : ag->operands()) {
    operand_shapes.push_back(&op->shape());
  }
  Shape shape = ShapeUtil::MakeTupleShape(
      {ag->operand_count() > 1
           ? ShapeUtil::MakeTupleShapeWithPtrs(operand_shapes)
           : *operand_shapes[0],
       ag->shape()});
  HloInstruction* start =
      computation->AddInstruction(HloInstruction::CreateAllGatherStart(
          shape, ag->operands(), ag->all_gather_dimension(), ag->device_list(),
          ag->constrain_layout(), ag->channel_id(),
          ag->use_global_device_ids()));
  HloInstruction* done =
      computation->AddInstruction(HloInstruction::CreateUnary(
          ag->shape(), HloOpcode::kAllGatherDone, start));
  return ReplacedAsync{start, done};
}

absl::StatusOr<ReplacedAsync> CreateAsyncCollectivePermute(
    HloInstruction* instruction, absl::Span<const Shape> context_shapes) {
  HloComputation* computation = instruction->parent();
  auto* cp = Cast<HloCollectivePermuteInstruction>(instruction);
  HloInstruction* start;
  HloInstruction* operand = cp->mutable_operand(0);
  if (cp->operand_count() == 1) {
    start = computation->AddInstruction(
        HloInstruction::CreateCollectivePermuteStart(
            ShapeInference::InferCollectivePermuteStartShape(
                {&operand->shape()}, context_shapes)
                .value(),
            operand, cp->source_target_pairs(), cp->channel_id()));
  } else {
    CHECK_EQ(cp->operand_count(), 4);
    std::vector<const Shape*> operand_shapes;
    absl::c_transform(
        cp->operands(), std::back_inserter(operand_shapes),
        [](const HloInstruction* operand) { return &(operand->shape()); });
    start = computation->AddInstruction(
        HloInstruction::CreateCollectivePermuteStart(
            ShapeInference::InferCollectivePermuteStartShape(operand_shapes,
                                                             context_shapes)
                .value(),
            operand, cp->mutable_operand(1), cp->mutable_operand(2),
            cp->mutable_operand(3), cp->source_target_pairs(),
            cp->dynamic_slice_sizes_list(), cp->channel_id()));
    if (HasDisjointReadWriteRegionsAttr(cp)) {
      SetDisjointReadWriteRegionsAttr(start);
    }
  }
  HloInstruction* done =
      computation->AddInstruction(HloInstruction::CreateUnary(
          cp->shape(), HloOpcode::kCollectivePermuteDone, start));
  return ReplacedAsync{start, done};
}

absl::StatusOr<ReplacedAsync> CreateAsyncStartDone(
    HloInstruction* instruction, absl::Span<const Shape> context_shapes) {
  HloComputation* computation = instruction->parent();
  TF_ASSIGN_OR_RETURN(
      HloInstruction * done,
      computation->CreateAsyncInstructions(instruction, context_shapes,
                                           HloInstruction::kMainExecutionThread,
                                           /*replace=*/false));
  HloInstruction* start = done->mutable_operand(0);
  return ReplacedAsync{start, done};
}

}  // namespace

// Find all supported collective ops first as we can't modify the instructions
// while iterating through them.
std::vector<HloInstruction*> AsyncCollectiveCreator::MatchCollectives(
    HloComputation* computation) {
  std::vector<HloInstruction*> supported_collectives;
  for (HloInstruction* instruction : computation->instructions()) {
    const HloOpcode op = instruction->opcode();
    if ((op == HloOpcode::kAllReduce &&
         config_.convert_all_reduce(instruction)) ||
        (op == HloOpcode::kAllGather &&
         config_.convert_all_gather(instruction)) ||
        (op == HloOpcode::kCollectiveBroadcast &&
         config_.convert_collective_broadcast(instruction)) ||
        (op == HloOpcode::kCollectivePermute &&
         config_.convert_collective_permute(instruction)) ||
        (op == HloOpcode::kAllToAll &&
         config_.convert_all_to_all(instruction)) ||
        (op == HloOpcode::kReduceScatter &&
         config_.convert_reduce_scatter(instruction))) {
      supported_collectives.push_back(instruction);
    }
  }
  return supported_collectives;
}

absl::StatusOr<bool> AsyncCollectiveCreator::ReplaceCollectives(
    HloComputation* computation,
    std::vector<HloInstruction*>& supported_collectives) {
  bool changed = false;
  HloModule* module = computation->parent();
  absl::flat_hash_map<HloInstruction*, ReplacedAsync> replaced_pairs;
  const bool should_update_schedule =
      module->has_schedule() &&
      module->schedule().is_computation_scheduled(computation);
  for (HloInstruction* instruction : supported_collectives) {
    absl::StatusOr<ReplacedAsync> async_pair;
    switch (instruction->opcode()) {
      case HloOpcode::kAllReduce:
        async_pair = CreateAsyncAllReduce(instruction);
        break;
      case HloOpcode::kAllGather:
        async_pair = CreateAsyncAllGather(instruction);
        break;
      case HloOpcode::kCollectivePermute:
        async_pair = CreateAsyncCollectivePermute(
            instruction, config_.get_context_shapes(instruction));
        break;
      case HloOpcode::kCollectiveBroadcast:
      case HloOpcode::kAllToAll:
      case HloOpcode::kReduceScatter:
        async_pair = CreateAsyncStartDone(
            instruction, config_.get_context_shapes(instruction));
        break;
      default:
        return Internal("Unexpected opcode %s",
                        HloOpcodeString(instruction->opcode()));
    }
    TF_RETURN_IF_ERROR(async_pair.status());
    async_pair->start->set_metadata(instruction->metadata());
    async_pair->start->CopyBackendConfigFrom(instruction);
    if (should_update_schedule) {
      replaced_pairs[instruction] = *async_pair;
    }

    // Update control dependencies if present.
    TF_RETURN_IF_ERROR(
        instruction->CopyAllControlDepsTo(async_pair->start, async_pair->done));
    TF_RETURN_IF_ERROR(instruction->DropAllControlDeps());

    TF_RETURN_WITH_CONTEXT_IF_ERROR(
        computation->ReplaceInstruction(instruction, async_pair->done),
        "replacing ", instruction->ToShortString());
    changed = true;
  }
  if (should_update_schedule) {
    std::vector<HloInstruction*> new_sequence;
    const HloInstructionSequence& sequence =
        module->schedule().sequence(computation);
    new_sequence.reserve(sequence.size() + replaced_pairs.size());
    for (HloInstruction* instr : sequence.instructions()) {
      auto it = replaced_pairs.find(instr);
      if (it != replaced_pairs.end()) {
        new_sequence.push_back(it->second.start);
        new_sequence.push_back(it->second.done);
        continue;
      }
      new_sequence.push_back(instr);
    }
    module->schedule().set_sequence(computation, new_sequence);
  }
  return changed;
}

absl::StatusOr<bool> AsyncCollectiveCreator::Run(
    HloModule* module,
    const absl::flat_hash_set<absl::string_view>& execution_threads) {
  bool changed = false;
  int64_t collectives_replaced = 0;
  for (HloComputation* computation :
       module->MakeNonfusionComputations(execution_threads)) {
    std::vector<HloInstruction*> supported_collectives =
        MatchCollectives(computation);
    if (supported_collectives.empty()) {
      continue;
    }
    TF_ASSIGN_OR_RETURN(bool comp_changed,
                        ReplaceCollectives(computation, supported_collectives));
    collectives_replaced += supported_collectives.size();
    changed |= comp_changed;
  }
  VLOG(1) << "Replaced " << collectives_replaced
          << " sync collectives with async versions.";
  return changed;
}

}  // namespace xla
