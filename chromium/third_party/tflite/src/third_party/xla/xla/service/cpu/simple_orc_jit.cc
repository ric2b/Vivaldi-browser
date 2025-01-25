/* Copyright 2017 The OpenXLA Authors.

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

#include "xla/service/cpu/simple_orc_jit.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <system_error>  // NOLINT
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/ExecutorProcessControl.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include "llvm/ExecutionEngine/Orc/SymbolStringPool.h"
#include "llvm/ExecutionEngine/Orc/ThreadSafeModule.h"
#include "llvm/ExecutionEngine/RTDyldMemoryManager.h"
#include "llvm/ExecutionEngine/RuntimeDyld.h"
#include "llvm/ExecutionEngine/SectionMemoryManager.h"
#include "llvm/IR/FMF.h"
#include "llvm/IR/Mangler.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/CodeGen.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Memory.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Process.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/TargetParser/Host.h"
#include "mlir/ExecutionEngine/CRunnerUtils.h"
#include "xla/service/cpu/compiler_functor.h"
#include "xla/service/cpu/cpu_runtime.h"
#include "xla/service/cpu/orc_jit_memory_mapper.h"
#include "xla/service/cpu/runtime_conv2d.h"
#include "xla/service/cpu/runtime_conv2d_acl.h"
#include "xla/service/cpu/runtime_conv2d_mkl.h"
#include "xla/service/cpu/runtime_conv3d.h"
#include "xla/service/cpu/runtime_custom_call_status.h"
#include "xla/service/cpu/runtime_fft.h"
#include "xla/service/cpu/runtime_fork_join.h"
#include "xla/service/cpu/runtime_fp16.h"
#include "xla/service/cpu/runtime_handle_ffi_call.h"  // NOLINT
#include "xla/service/cpu/runtime_key_value_sort.h"
#include "xla/service/cpu/runtime_matmul.h"
#include "xla/service/cpu/runtime_matmul_acl.h"
#include "xla/service/cpu/runtime_pow.h"
#include "xla/service/cpu/runtime_single_threaded_conv2d.h"
#include "xla/service/cpu/runtime_single_threaded_conv3d.h"
#include "xla/service/cpu/runtime_single_threaded_fft.h"
#include "xla/service/cpu/runtime_single_threaded_matmul.h"
#include "xla/service/cpu/runtime_topk.h"
#include "xla/service/cpu/windows_compatibility.h"
#include "xla/service/custom_call_target_registry.h"
#include "xla/service/llvm_compiler.h"
#include "xla/util.h"
#include "tsl/platform/cpu_info.h"
#include "tsl/platform/logging.h"

#if defined(INTEL_MKL) && defined(ENABLE_ONEDNN_V3)
#include "xla/service/cpu/onednn_convolution.h"
#include "xla/service/cpu/onednn_layer_norm.h"
#include "xla/service/cpu/onednn_matmul.h"
#include "xla/service/cpu/onednn_softmax.h"
#endif

// Provided by compiler-rt and MLIR.
// Converts an F32 value to a BF16.
extern "C" uint16_t __truncsfbf2(float);
// Converts an F64 value to a BF16.
extern "C" uint16_t __truncdfbf2(double);

namespace xla::cpu {
namespace {

class DefaultMemoryMapper final
    : public llvm::SectionMemoryManager::MemoryMapper {
 public:
  llvm::sys::MemoryBlock allocateMappedMemory(
      llvm::SectionMemoryManager::AllocationPurpose purpose, size_t num_bytes,
      const llvm::sys::MemoryBlock* const near_block, unsigned flags,
      std::error_code& error_code) override {
    return llvm::sys::Memory::allocateMappedMemory(num_bytes, near_block, flags,
                                                   error_code);
  }

  std::error_code protectMappedMemory(const llvm::sys::MemoryBlock& block,
                                      unsigned flags) override {
    return llvm::sys::Memory::protectMappedMemory(block, flags);
  }

  std::error_code releaseMappedMemory(llvm::sys::MemoryBlock& m) override {
    return llvm::sys::Memory::releaseMappedMemory(m);
  }
};

// On Windows, LLVM may emit IMAGE_REL_AMD64_ADDR32NB COFF relocations when
// referring to read-only data, however IMAGE_REL_AMD64_ADDR32NB requires that
// the read-only data section follow within 2GB of the code. Oddly enough,
// the LLVM SectionMemoryManager does nothing to enforce this
// (https://github.com/llvm/llvm-project/issues/55386), leading to crashes on
// Windows when the sections end up in the wrong order. Since none
// of the memory managers in the LLVM tree obey the necessary ordering
// constraints, we need to roll our own.
//
// ContiguousSectionMemoryManager is an alternative to SectionMemoryManager
// that maps one large block of memory and suballocates it
// for each section, in the correct order. This is easy enough to do because of
// the llvm::RuntimeDyld::MemoryManager::reserveAllocationSpace() hook, which
// ensures that LLVM will tell us ahead of time the total sizes of all the
// relevant sections. We also know that XLA isn't going to do any more
// complicated memory management: we will allocate the sections once and we are
// done.
class ContiguousSectionMemoryManager : public llvm::RTDyldMemoryManager {
 public:
  explicit ContiguousSectionMemoryManager(
      llvm::SectionMemoryManager::MemoryMapper* mmapper)
      : mmapper_(mmapper), mmapper_is_owned_(false) {
    if (mmapper_ == nullptr) {
      mmapper_ = new DefaultMemoryMapper();
      mmapper_is_owned_ = true;
    }
  }

  ~ContiguousSectionMemoryManager() override;

  bool needsToReserveAllocationSpace() override { return true; }
  void reserveAllocationSpace(uintptr_t code_size, llvm::Align code_align,
                              uintptr_t ro_data_size, llvm::Align ro_data_align,
                              uintptr_t rw_data_size,
                              llvm::Align rw_data_align) override;

  uint8_t* allocateDataSection(uintptr_t size, unsigned alignment,
                               unsigned section_id,
                               llvm::StringRef section_name,
                               bool is_read_only) override;

  uint8_t* allocateCodeSection(uintptr_t size, unsigned alignment,
                               unsigned section_id,
                               llvm::StringRef section_name) override;

  bool finalizeMemory(std::string* err_msg) override;

 private:
  llvm::SectionMemoryManager::MemoryMapper* mmapper_;
  bool mmapper_is_owned_;

  llvm::sys::MemoryBlock allocation_;

  // Sections must be in the order code < rodata < rwdata.
  llvm::sys::MemoryBlock code_block_;
  llvm::sys::MemoryBlock ro_data_block_;
  llvm::sys::MemoryBlock rw_data_block_;

  llvm::sys::MemoryBlock code_free_;
  llvm::sys::MemoryBlock ro_data_free_;
  llvm::sys::MemoryBlock rw_data_free_;

  uint8_t* Allocate(llvm::sys::MemoryBlock& free_block, std::uintptr_t size,
                    unsigned alignment);
};

ContiguousSectionMemoryManager::~ContiguousSectionMemoryManager() {
  if (allocation_.allocatedSize() != 0) {
    auto ec = mmapper_->releaseMappedMemory(allocation_);
    if (ec) {
      LOG(ERROR) << "releaseMappedMemory failed with error: " << ec.message();
    }
  }
  if (mmapper_is_owned_) {
    delete mmapper_;
  }
}

void ContiguousSectionMemoryManager::reserveAllocationSpace(
    uintptr_t code_size, llvm::Align code_align, uintptr_t ro_data_size,
    llvm::Align ro_data_align, uintptr_t rw_data_size,
    llvm::Align rw_data_align) {
  CHECK_EQ(allocation_.allocatedSize(), 0);

  static const size_t page_size = llvm::sys::Process::getPageSizeEstimate();
  CHECK_LE(code_align.value(), page_size);
  CHECK_LE(ro_data_align.value(), page_size);
  CHECK_LE(rw_data_align.value(), page_size);
  code_size = RoundUpTo<uintptr_t>(code_size + code_align.value(), page_size);
  ro_data_size =
      RoundUpTo<uintptr_t>(ro_data_size + ro_data_align.value(), page_size);
  rw_data_size =
      RoundUpTo<uintptr_t>(rw_data_size + rw_data_align.value(), page_size);
  uintptr_t total_size =
      code_size + ro_data_size + rw_data_size + page_size * 3;

  std::error_code ec;
  allocation_ = mmapper_->allocateMappedMemory(
      llvm::SectionMemoryManager::AllocationPurpose::Code, total_size, nullptr,
      llvm::sys::Memory::MF_READ | llvm::sys::Memory::MF_WRITE, ec);
  if (ec) {
    LOG(ERROR) << "allocateMappedMemory failed with error: " << ec.message();
    return;
  }

  auto base = reinterpret_cast<std::uintptr_t>(allocation_.base());
  code_block_ = code_free_ =
      llvm::sys::MemoryBlock(reinterpret_cast<void*>(base), code_size);
  base += code_size;
  ro_data_block_ = ro_data_free_ =
      llvm::sys::MemoryBlock(reinterpret_cast<void*>(base), ro_data_size);
  base += ro_data_size;
  rw_data_block_ = rw_data_free_ =
      llvm::sys::MemoryBlock(reinterpret_cast<void*>(base), rw_data_size);
}

uint8_t* ContiguousSectionMemoryManager::allocateDataSection(
    uintptr_t size, unsigned alignment, unsigned section_id,
    llvm::StringRef section_name, bool is_read_only) {
  if (is_read_only) {
    return Allocate(ro_data_free_, size, alignment);
  } else {
    return Allocate(rw_data_free_, size, alignment);
  }
}

uint8_t* ContiguousSectionMemoryManager::allocateCodeSection(
    uintptr_t size, unsigned alignment, unsigned section_id,
    llvm::StringRef section_name) {
  return Allocate(code_free_, size, alignment);
}

uint8_t* ContiguousSectionMemoryManager::Allocate(
    llvm::sys::MemoryBlock& free_block, std::uintptr_t size,
    unsigned alignment) {
  auto base = reinterpret_cast<uintptr_t>(free_block.base());
  auto start = RoundUpTo<uintptr_t>(base, alignment);
  uintptr_t padded_size = (start - base) + size;
  if (padded_size > free_block.allocatedSize()) {
    LOG(ERROR) << "Failed to satisfy suballocation request for " << size;
    return nullptr;
  }
  free_block =
      llvm::sys::MemoryBlock(reinterpret_cast<void*>(base + padded_size),
                             free_block.allocatedSize() - padded_size);
  return reinterpret_cast<uint8_t*>(start);
}

bool ContiguousSectionMemoryManager::finalizeMemory(std::string* err_msg) {
  std::error_code ec;

  ec = mmapper_->protectMappedMemory(
      code_block_, llvm::sys::Memory::MF_READ | llvm::sys::Memory::MF_EXEC);
  if (ec) {
    if (err_msg) {
      *err_msg = ec.message();
    }
    return true;
  }
  ec =
      mmapper_->protectMappedMemory(ro_data_block_, llvm::sys::Memory::MF_READ);
  if (ec) {
    if (err_msg) {
      *err_msg = ec.message();
    }
    return true;
  }

  llvm::sys::Memory::InvalidateInstructionCache(code_block_.base(),
                                                code_block_.allocatedSize());
  return false;
}

using tsl::port::CPUFeature;

// Returns the earliest CPU generation that supports the instruction set.
llvm::StringRef CPUTargetFromMaxFeature(CPUFeature max_feature) {
  switch (max_feature) {
    case CPUFeature::SSE4_2:
      return "nehalem";
    case CPUFeature::AVX:
      return "sandybridge";
    case CPUFeature::AVX2:
      return "haswell";
    case CPUFeature::AVX512F:
      return "skylake-avx512";
    case CPUFeature::AVX512_VNNI:
      return "cascadelake";
    case CPUFeature::AVX512_BF16:
      return "cooperlake";
    case CPUFeature::AMX_BF16:
    case CPUFeature::AMX_INT8:
      return "sapphirerapids";
    case CPUFeature::AMX_FP16:
      return "graniterapids";
    default:
      LOG(FATAL) << "Unsupported max feature: " << max_feature;
  }
}

}  // namespace

std::optional<CPUFeature> ISAStringToFeature(
    const absl::string_view feature_string) {
  if (feature_string.empty()) return std::nullopt;

  // Non-exhaustive list of CPU features. (Only the ones we care about.)
  // TODO(penporn): Handle ARM
  static auto* x86 = [] {
    return new absl::flat_hash_map<std::string, CPUFeature>(
        {{"SSE4_2", CPUFeature::SSE4_2},
         {"AVX", CPUFeature::AVX},
         {"AVX2", CPUFeature::AVX2},
         {"AVX512", CPUFeature::AVX512F},
         {"AVX512_VNNI", CPUFeature::AVX512_VNNI},
         {"AVX512_BF16", CPUFeature::AVX512_BF16},
         {"AMX", CPUFeature::AMX_BF16},  // Includes AMX_INT8.
         {"AMX_FP16", CPUFeature::AMX_FP16}});
  }();

  // Assume that `feature_string` always contains all uppercase letters.
  if (auto it = x86->find(feature_string); it != x86->end()) return it->second;
  LOG(WARNING) << "Unknown CPU ISA: " << feature_string;
  return std::nullopt;
}

// Disable any feature that is newer than `max_feature`.
bool ShouldEnableCPUFeature(const llvm::StringRef feature,
                            const CPUFeature& max_feature) {
  // x86 CPUs have backward compatibility so newer CPUs have all features of
  // older CPUs. We go through switch cases from oldest features to newest.
  //   - Each case looks for features that are introduced in the next
  //     generation, i.e., features that should be disabled if `max_feature` is
  //     older or equal to the case's ISA.
  //   - We combine all features that needs to be disabled from all ISAs newer
  //     than `max_feature` by falling through cases.
  //
  // For example, if `max_feature` is AVX2, we start by disabling
  // AVX512-generation features in the AVX2 case, then fall through to the
  // AVX512 case to disable next-gen features (AVX512_VNNI), etc, all the way
  // down to the newest one.
  //
  // TODO(https://github.com/openxla/xla/issues/17758): Figure out if we need to
  // support AVX10 and where to put it.
  switch (max_feature) {
    case CPUFeature::SSE4_2:
      if (feature.starts_with("avx") || feature == "f16c" ||
          feature == "vpclmulqdq" || feature == "vaes") {
        return false;
      }
      [[fallthrough]];
    case CPUFeature::AVX:
      if (feature.starts_with("avx2") || feature.starts_with("fma")) {
        return false;
      }
      [[fallthrough]];
    case CPUFeature::AVX2:
      if (feature.starts_with("avx512") || feature == "evex512") return false;
      [[fallthrough]];
    case CPUFeature::AVX512F:
      if (feature == "avx512vnni") return false;
      [[fallthrough]];
    case CPUFeature::AVX512_VNNI:
      if (feature == "avx512bf16") return false;
      [[fallthrough]];
    case CPUFeature::AVX512_BF16:
      if (feature.starts_with("amx")) return false;
      [[fallthrough]];
    case CPUFeature::AMX_INT8:
    case CPUFeature::AMX_BF16:
      if (feature == "amx-fp16") return false;
      [[fallthrough]];
    default:
      // Leave all other features enabled.
      return true;
  }
}

DetectedMachineAttributes DetectMachineAttributes(
    std::optional<CPUFeature> max_feature) {
  DetectedMachineAttributes result;
  result.features_filtered = false;
  // We only have x86 constraints. Skip the check if we are on non-x86 CPUs.
  bool no_feature_constraint =
      !max_feature.has_value() || !tsl::port::IsX86CPU();
  for (const auto& [feature, enabled] : llvm::sys::getHostCPUFeatures()) {
    bool should_enable =
        enabled && (no_feature_constraint ||
                    ShouldEnableCPUFeature(feature, *max_feature));
    result.features.push_back(
        absl::StrCat(should_enable ? "+" : "-", std::string(feature)));
    result.features_filtered |= (should_enable != enabled);
  }
  std::sort(result.features.begin(), result.features.end());
  return result;
}

std::vector<std::string> DetectMachineAttributes() {
  return DetectMachineAttributes(std::nullopt).features;
}

/*static*/ std::unique_ptr<llvm::TargetMachine>
SimpleOrcJIT::InferTargetMachineForJIT(
    const llvm::TargetOptions& target_options, llvm::CodeGenOptLevel opt_level,
    absl::string_view max_cpu_isa) {
  std::optional<CPUFeature> max_feature = ISAStringToFeature(max_cpu_isa);
  auto result = DetectMachineAttributes(max_feature);
  llvm::SmallVector<std::string, 0> llvm_attrs(result.features.begin(),
                                               result.features.end());
  // If `max_feature` is newer than the host CPU, we should keep the host CPU
  // name, e.g., we don't want to set the target CPU to Skylake when we are on
  // a Broadwell host.
  llvm::StringRef target_cpu = result.features_filtered
                                   ? CPUTargetFromMaxFeature(*max_feature)
                                   : llvm::sys::getHostCPUName();
  std::unique_ptr<llvm::TargetMachine> target_machine(
      llvm::EngineBuilder()
          .setTargetOptions(target_options)
          .setOptLevel(opt_level)
          .selectTarget(
              /*TargetTriple=*/llvm::Triple(), /*MArch=*/"",
              /*MCPU=*/target_cpu,
              /*MAttrs=*/llvm_attrs));
  CHECK(target_machine != nullptr);
  return target_machine;
}

static CompilerFunctor::TargetMachineBuilder CreateTargetMachineBuilder(
    llvm::TargetOptions target_options, llvm::CodeGenOptLevel opt_level,
    absl::string_view max_cpu_isa) {
  return [target_options, opt_level, max_cpu_isa]() {
    return SimpleOrcJIT::InferTargetMachineForJIT(target_options, opt_level,
                                                  max_cpu_isa);
  };
}

SimpleOrcJIT::SimpleOrcJIT(
    std::unique_ptr<llvm::orc::ExecutorProcessControl> target_process_control,
    std::unique_ptr<llvm::orc::ExecutionSession> execution_session,
    const llvm::TargetOptions& target_options, llvm::CodeGenOptLevel opt_level,
    bool optimize_for_size, bool disable_expensive_passes,
    bool disable_slp_vectorizer, llvm::FastMathFlags fast_math_flags,
    LLVMCompiler::ModuleHook pre_optimization_hook,
    LLVMCompiler::ModuleHook post_optimization_hook,
    absl::AnyInvocable<void(const llvm::object::ObjectFile&)> post_codegen_hook,
    size_t num_jit_dylibs, absl::string_view max_cpu_isa)
    : target_machine_builder_(
          CreateTargetMachineBuilder(target_options, opt_level, max_cpu_isa)),
      target_machine_(target_machine_builder_()),
      target_triple_(target_machine_->getTargetTriple()),
      data_layout_(target_machine_->createDataLayout()),
      target_process_control_(std::move(target_process_control)),
      execution_session_(std::move(execution_session)),
      object_layer_(*execution_session_,
                    []() {
                      return std::make_unique<ContiguousSectionMemoryManager>(
                          orc_jit_memory_mapper::GetInstance());
                    }),
      compile_layer_(
          *execution_session_, object_layer_,
          std::make_unique<CompilerFunctor>(
              target_machine_builder_, static_cast<int>(opt_level),
              optimize_for_size, disable_expensive_passes,
              disable_slp_vectorizer, fast_math_flags,
              std::move(pre_optimization_hook),
              std::move(post_optimization_hook), std::move(post_codegen_hook))),
      gdb_jit_event_listener_(
          llvm::JITEventListener::createGDBRegistrationListener()),
      perf_jit_event_listener_(
          llvm::JITEventListener::createPerfJITEventListener()) {
  VLOG(1) << "CPU target: " << target_machine_->getTargetCPU().str()
          << " features: " << target_machine_->getTargetFeatureString().str();

  // Materialize unknown symbols from the runtime symbol table.
  class RuntimeSymbolGenerator : public llvm::orc::DefinitionGenerator {
    SimpleOrcJIT& jit_;

   public:
    explicit RuntimeSymbolGenerator(SimpleOrcJIT& jit) : jit_(jit) {}
    llvm::Error tryToGenerate(
        llvm::orc::LookupState&, llvm::orc::LookupKind,
        llvm::orc::JITDylib& jit_dylib, llvm::orc::JITDylibLookupFlags,
        const llvm::orc::SymbolLookupSet& names) override {
      llvm::orc::SymbolMap new_defs;

      for (const auto& kv : names) {
        const auto& name = kv.first;
        llvm::orc::ExecutorSymbolDef symbol = jit_.ResolveRuntimeSymbol(*name);
        if (symbol.getAddress()) {
          new_defs[name] = symbol;
        }
      }

      cantFail(jit_dylib.define(absoluteSymbols(std::move(new_defs))));
      return llvm::Error::success();
    }
  };

  // Always create at least one dylib.
  num_jit_dylibs = std::max(size_t{1}, num_jit_dylibs);
  jit_dylibs_.resize(num_jit_dylibs);
  for (size_t i = 0; i < num_jit_dylibs; ++i) {
    jit_dylibs_[i] = &execution_session_->createBareJITDylib(
        absl::StrCat("<xla_jit_dylib_", i, ">"));
    jit_dylibs_[i]->addGenerator(
        std::make_unique<RuntimeSymbolGenerator>(*this));
  }

  object_layer_.registerJITEventListener(*this);
  if (perf_jit_event_listener_) {
    object_layer_.registerJITEventListener(*perf_jit_event_listener_);
  }

  // Copied from LLJIT, required to find symbols on Windows.
  if (target_triple_.isOSBinFormatCOFF()) {
    object_layer_.setOverrideObjectFlagsWithResponsibilityFlags(true);
    object_layer_.setAutoClaimResponsibilityForObjectSymbols(true);
  }
}

SimpleOrcJIT::~SimpleOrcJIT() {
  if (auto err = execution_session_->endSession()) {
    execution_session_->reportError(std::move(err));
  }
}

llvm::Expected<std::unique_ptr<SimpleOrcJIT>> SimpleOrcJIT::Create(
    const llvm::TargetOptions& target_options, llvm::CodeGenOptLevel opt_level,
    bool optimize_for_size, bool disable_expensive_passes,
    bool disable_slp_vectorizer, llvm::FastMathFlags fast_math_flags,
    LLVMCompiler::ModuleHook pre_optimization_hook,
    LLVMCompiler::ModuleHook post_optimization_hook,
    absl::AnyInvocable<void(const llvm::object::ObjectFile&)> post_codegen_hook,
    size_t num_jit_dylibs, absl::string_view max_cpu_isa) {
  auto SSP = std::make_shared<llvm::orc::SymbolStringPool>();
  auto target_process_control =
      llvm::orc::SelfExecutorProcessControl::Create(std::move(SSP));
  if (!target_process_control) {
    return target_process_control.takeError();
  }

  auto execution_session = std::make_unique<llvm::orc::ExecutionSession>(
      std::make_unique<llvm::orc::UnsupportedExecutorProcessControl>());
  return std::make_unique<SimpleOrcJIT>(
      std::move(*target_process_control), std::move(execution_session),
      target_options, opt_level, optimize_for_size, disable_expensive_passes,
      disable_slp_vectorizer, fast_math_flags, std::move(pre_optimization_hook),
      std::move(post_optimization_hook), std::move(post_codegen_hook),
      num_jit_dylibs, std::move(max_cpu_isa));
}

llvm::orc::ExecutorSymbolDef SimpleOrcJIT::ResolveRuntimeSymbol(
    llvm::StringRef name) {
  void* func_addr = nullptr;
  if (name.size() > 1 && name.front() == data_layout_.getGlobalPrefix()) {
    // On Mac OS X, 'name' may have a leading underscore prefix, even though the
    // registered name may not.
    std::string stripped_name(name.begin() + 1, name.end());
    func_addr =
        xla::CustomCallTargetRegistry::Global()->Lookup(stripped_name, "Host");
  } else {
    func_addr =
        xla::CustomCallTargetRegistry::Global()->Lookup(name.str(), "Host");
  }

  if (func_addr == nullptr) {
    // If symbol corresponds to a kernel function, then it must be defined in
    // another LLVM module part (another dylib).
    if (!kernel_symbols_.contains(name)) {
      LOG(ERROR)
          << "Unable to resolve runtime symbol: `" << name.str()
          << "'. Hint: if the symbol a custom call target, make sure you've "
             "registered it with the JIT using "
             "XLA_CPU_REGISTER_CUSTOM_CALL_TARGET.";
    }
    return {};
  }
  return {llvm::orc::ExecutorAddr(reinterpret_cast<uint64_t>(func_addr)),
          llvm::JITSymbolFlags::None};
}

void SimpleOrcJIT::notifyObjectLoaded(
    llvm::JITEventListener::ObjectKey key,
    const llvm::object::ObjectFile& object,
    const llvm::RuntimeDyld::LoadedObjectInfo& object_info) {
  gdb_jit_event_listener_->notifyObjectLoaded(key, object, object_info);
  size_of_generated_code_in_bytes_ += object.getData().size();
}

void SimpleOrcJIT::notifyFreeingObject(llvm::JITEventListener::ObjectKey key) {
  gdb_jit_event_listener_->notifyFreeingObject(key);
}

llvm::Error SimpleOrcJIT::AddObjFile(
    std::unique_ptr<llvm::MemoryBuffer> obj_file, size_t dylib_index) {
  return object_layer_.add(*jit_dylibs_[dylib_index], std::move(obj_file));
}

llvm::Error SimpleOrcJIT::AddModule(llvm::orc::ThreadSafeModule module,
                                    size_t dylib_index) {
  return compile_layer_.add(*jit_dylibs_[dylib_index], std::move(module));
}

void SimpleOrcJIT::DoneCompiling() {
  // The target machine takes a non-trivial amount of memory, so once we are
  // done compiling throw it away.
  target_machine_.reset();
}

llvm::Expected<llvm::orc::ExecutorSymbolDef> SimpleOrcJIT::FindCompiledSymbol(
    const std::string& name) {
  return execution_session_->lookup(jit_dylibs_, name);
}

#if defined(PLATFORM_WINDOWS)
// This function is used by compiler-generated code on windows, but it's not
// declared anywhere. The signature does not matter, we just need the address.
extern "C" void __chkstk(size_t);
#endif

namespace {
// Register some known symbols with the CustomCallTargetRegistry.
bool RegisterKnownJITSymbols() {
  xla::CustomCallTargetRegistry* registry =
      xla::CustomCallTargetRegistry::Global();
  registry->Register("printf", reinterpret_cast<void*>(&printf), "Host");
  registry->Register("puts", reinterpret_cast<void*>(&puts), "Host");

#define REGISTER_CPU_RUNTIME_SYMBOL(base_name)                               \
  do {                                                                       \
    auto* function_address =                                                 \
        reinterpret_cast<void*>(__xla_cpu_runtime_##base_name);              \
    registry->Register(xla::cpu::runtime::k##base_name##SymbolName,          \
                       function_address, "Host");                            \
    CHECK_EQ(absl::string_view(xla::cpu::runtime::k##base_name##SymbolName), \
             "__xla_cpu_runtime_" #base_name);                               \
  } while (false)

  REGISTER_CPU_RUNTIME_SYMBOL(AcquireInfeedBufferForDequeue);
  REGISTER_CPU_RUNTIME_SYMBOL(AcquireOutfeedBufferForPopulation);
  REGISTER_CPU_RUNTIME_SYMBOL(AllReduce);
  REGISTER_CPU_RUNTIME_SYMBOL(CollectivePermute);
  REGISTER_CPU_RUNTIME_SYMBOL(AllToAll);
  REGISTER_CPU_RUNTIME_SYMBOL(AllGather);
  REGISTER_CPU_RUNTIME_SYMBOL(ReduceScatter);
  REGISTER_CPU_RUNTIME_SYMBOL(PartitionId);
  REGISTER_CPU_RUNTIME_SYMBOL(ReplicaId);
  REGISTER_CPU_RUNTIME_SYMBOL(MKLConv2DF32);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenConv2DF16);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenConv2DF32);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenConv3DF16);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenConv3DF32);
  REGISTER_CPU_RUNTIME_SYMBOL(DuccFft);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenMatMulF16);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenMatMulF32);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenMatMulF64);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenMatMulC64);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenMatMulC128);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenMatMulS32);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenBatchMatMulF32);
  REGISTER_CPU_RUNTIME_SYMBOL(ACLMatMulF32);
  REGISTER_CPU_RUNTIME_SYMBOL(ACLBatchMatMulF32);
  REGISTER_CPU_RUNTIME_SYMBOL(ACLConv2DF32);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedConv2DF16);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedConv2DF32);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedConv3DF16);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedConv3DF32);
  REGISTER_CPU_RUNTIME_SYMBOL(DuccSingleThreadedFft);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulF8E4M3FN);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulF8E5M2);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulF16);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulF32);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulF64);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulC64);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulC128);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulS32);
  REGISTER_CPU_RUNTIME_SYMBOL(EigenSingleThreadedMatMulU8);
  REGISTER_CPU_RUNTIME_SYMBOL(ParallelForkJoin);
  REGISTER_CPU_RUNTIME_SYMBOL(PrintfToStderr);
  REGISTER_CPU_RUNTIME_SYMBOL(ReleaseInfeedBufferAfterDequeue);
  REGISTER_CPU_RUNTIME_SYMBOL(ReleaseOutfeedBufferAfterPopulation);
  REGISTER_CPU_RUNTIME_SYMBOL(StatusIsSuccess);
  REGISTER_CPU_RUNTIME_SYMBOL(KeyValueSort);
  REGISTER_CPU_RUNTIME_SYMBOL(TopKF32);
  REGISTER_CPU_RUNTIME_SYMBOL(TracingStart);
  REGISTER_CPU_RUNTIME_SYMBOL(TracingEnd);
  REGISTER_CPU_RUNTIME_SYMBOL(HandleFfiCall);
#if defined(INTEL_MKL) && defined(ENABLE_ONEDNN_V3)
  REGISTER_CPU_RUNTIME_SYMBOL(OneDnnMatMul);
  REGISTER_CPU_RUNTIME_SYMBOL(OneDnnSoftmax);
  REGISTER_CPU_RUNTIME_SYMBOL(OneDnnLayerNorm);
  REGISTER_CPU_RUNTIME_SYMBOL(OneDnnConvolution);
  REGISTER_CPU_RUNTIME_SYMBOL(OneDnnMatMulReorder);
#endif  // INTEL_MKL && ENABLE_ONEDNN_V3

  registry->Register("__gnu_f2h_ieee", reinterpret_cast<void*>(__gnu_f2h_ieee),
                     "Host");
  registry->Register("__gnu_h2f_ieee", reinterpret_cast<void*>(__gnu_h2f_ieee),
                     "Host");
  registry->Register("__truncdfhf2", reinterpret_cast<void*>(__truncdfhf2),
                     "Host");
  registry->Register("__truncdfbf2", reinterpret_cast<void*>(__truncdfbf2),
                     "Host");
  registry->Register("__truncsfbf2", reinterpret_cast<void*>(__truncsfbf2),
                     "Host");
  registry->Register("__powisf2", reinterpret_cast<void*>(__powisf2), "Host");
  registry->Register("__powidf2", reinterpret_cast<void*>(__powidf2), "Host");

#undef REGISTER_CPU_RUNTIME_SYMBOL

// Register both the f32 (float) and f64 (double) versions of a libm symbol.
// Unfortunately the double versions are overloaded on some systems, e.g.
// Mac so we need an explicit cast. This requires passing the function signature
// for that case.
#define REGISTER_LIBM_SYMBOL(name, double_sig)                                 \
  do {                                                                         \
    registry->Register(#name "f", reinterpret_cast<void*>(name##f), "Host");   \
    registry->Register(#name,                                                  \
                       reinterpret_cast<void*>(static_cast<double_sig>(name)), \
                       "Host");                                                \
  } while (false)

  REGISTER_LIBM_SYMBOL(acos, double (*)(double));
  REGISTER_LIBM_SYMBOL(acosh, double (*)(double));
  REGISTER_LIBM_SYMBOL(asin, double (*)(double));
  REGISTER_LIBM_SYMBOL(asinh, double (*)(double));
  REGISTER_LIBM_SYMBOL(atan, double (*)(double));
  REGISTER_LIBM_SYMBOL(atan2, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(atanh, double (*)(double));
  REGISTER_LIBM_SYMBOL(cbrt, double (*)(double));
  REGISTER_LIBM_SYMBOL(ceil, double (*)(double));
  REGISTER_LIBM_SYMBOL(copysign, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(cos, double (*)(double));
  REGISTER_LIBM_SYMBOL(cosh, double (*)(double));
  REGISTER_LIBM_SYMBOL(erf, double (*)(double));
  REGISTER_LIBM_SYMBOL(erfc, double (*)(double));
  REGISTER_LIBM_SYMBOL(exp, double (*)(double));
  REGISTER_LIBM_SYMBOL(exp2, double (*)(double));
  REGISTER_LIBM_SYMBOL(expm1, double (*)(double));
  REGISTER_LIBM_SYMBOL(fabs, double (*)(double));
  REGISTER_LIBM_SYMBOL(fdim, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(floor, double (*)(double));
  REGISTER_LIBM_SYMBOL(fma, double (*)(double, double, double));
  REGISTER_LIBM_SYMBOL(fmax, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(fmin, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(fmod, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(frexp, double (*)(double, int*));
  REGISTER_LIBM_SYMBOL(hypot, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(ilogb, int (*)(double));
  REGISTER_LIBM_SYMBOL(ldexp, double (*)(double, int));
  REGISTER_LIBM_SYMBOL(lgamma, double (*)(double));
  REGISTER_LIBM_SYMBOL(llrint, long long (*)(double));   // NOLINT(runtime/int)
  REGISTER_LIBM_SYMBOL(llround, long long (*)(double));  // NOLINT(runtime/int)
  REGISTER_LIBM_SYMBOL(log, double (*)(double));
  REGISTER_LIBM_SYMBOL(log10, double (*)(double));
  REGISTER_LIBM_SYMBOL(log1p, double (*)(double));
  REGISTER_LIBM_SYMBOL(log2, double (*)(double));
  REGISTER_LIBM_SYMBOL(logb, double (*)(double));
  REGISTER_LIBM_SYMBOL(lrint, long (*)(double));   // NOLINT(runtime/int)
  REGISTER_LIBM_SYMBOL(lround, long (*)(double));  // NOLINT(runtime/int)
  REGISTER_LIBM_SYMBOL(modf, double (*)(double, double*));
  REGISTER_LIBM_SYMBOL(nan, double (*)(const char*));
  REGISTER_LIBM_SYMBOL(nearbyint, double (*)(double));
  REGISTER_LIBM_SYMBOL(nextafter, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(nexttoward, double (*)(double, long double));
  REGISTER_LIBM_SYMBOL(pow, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(remainder, double (*)(double, double));
  REGISTER_LIBM_SYMBOL(remquo, double (*)(double, double, int*));
  REGISTER_LIBM_SYMBOL(rint, double (*)(double));
  REGISTER_LIBM_SYMBOL(round, double (*)(double));
  REGISTER_LIBM_SYMBOL(scalbln,
                       double (*)(double, long));  // NOLINT(runtime/int)
  REGISTER_LIBM_SYMBOL(scalbn, double (*)(double, int));
  REGISTER_LIBM_SYMBOL(sin, double (*)(double));
#ifdef __APPLE__
  REGISTER_LIBM_SYMBOL(__sincos, void (*)(double, double*, double*));
  registry->Register("__sincosf_stret",
                     reinterpret_cast<void*>(__sincosf_stret), "Host");
  registry->Register("__sincos_stret", reinterpret_cast<void*>(__sincos_stret),
                     "Host");
#else
  REGISTER_LIBM_SYMBOL(sincos, void (*)(double, double*, double*));
#endif
  REGISTER_LIBM_SYMBOL(sinh, double (*)(double));
  REGISTER_LIBM_SYMBOL(sqrt, double (*)(double));
  REGISTER_LIBM_SYMBOL(tan, double (*)(double));
  REGISTER_LIBM_SYMBOL(tanh, double (*)(double));
  REGISTER_LIBM_SYMBOL(tgamma, double (*)(double));
  REGISTER_LIBM_SYMBOL(trunc, double (*)(double));

#undef REGISTER_LIBM_SYMBOL

  registry->Register("memcpy", reinterpret_cast<void*>(memcpy), "Host");
  registry->Register("memmove", reinterpret_cast<void*>(memmove), "Host");
  registry->Register("memset", reinterpret_cast<void*>(memset), "Host");

  // Used by MLIR lowering.
  registry->Register("malloc", reinterpret_cast<void*>(malloc), "Host");
  registry->Register("calloc", reinterpret_cast<void*>(calloc), "Host");
  registry->Register("free", reinterpret_cast<void*>(free), "Host");
#ifndef _WIN32
  // TODO(b/246980307): fails to link on windows because it's marked dllimport.
  registry->Register("memrefCopy", reinterpret_cast<void*>(memrefCopy), "Host");
#endif

#ifdef __APPLE__
  registry->Register("__bzero", reinterpret_cast<void*>(bzero), "Host");
  registry->Register("bzero", reinterpret_cast<void*>(bzero), "Host");
  registry->Register("memset_pattern16",
                     reinterpret_cast<void*>(memset_pattern16), "Host");
#endif

#ifdef MEMORY_SANITIZER
  registry->Register("__msan_unpoison",
                     reinterpret_cast<void*>(__msan_unpoison), "Host");
#endif

#if defined(PLATFORM_WINDOWS)
  registry->Register("__chkstk", reinterpret_cast<void*>(__chkstk), "Host");
#endif

  return true;
}

bool unused = RegisterKnownJITSymbols();

}  // namespace
}  // namespace xla::cpu
