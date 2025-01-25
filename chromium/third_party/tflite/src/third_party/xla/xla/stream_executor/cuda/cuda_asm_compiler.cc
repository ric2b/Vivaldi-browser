/* Copyright 2020 The OpenXLA Authors.

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

#include "xla/stream_executor/cuda/cuda_asm_compiler.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/base/const_init.h"
#include "absl/base/optimization.h"
#include "absl/base/thread_annotations.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "third_party/gpus/cuda/include/cuda.h"
#include "xla/status_macros.h"
#include "xla/stream_executor/activate_context.h"
#include "xla/stream_executor/cuda/cuda_status.h"
#include "xla/stream_executor/cuda/ptx_compiler.h"
#include "xla/stream_executor/cuda/ptx_compiler_helpers.h"
#include "xla/stream_executor/cuda/ptx_compiler_support.h"
#include "xla/stream_executor/device_description.h"
#include "xla/stream_executor/gpu/gpu_asm_opts.h"
#include "xla/stream_executor/semantic_version.h"
#include "xla/stream_executor/stream_executor.h"
#include "xla/util.h"
#include "tsl/platform/cuda_root_path.h"
#include "tsl/platform/env.h"
#include "tsl/platform/errors.h"
#include "tsl/platform/path.h"
#include "tsl/platform/regexp.h"
#include "tsl/platform/status.h"
#include "tsl/platform/statusor.h"
#include "tsl/platform/subprocess.h"

namespace stream_executor {

static absl::StatusOr<std::string> GetToolVersionString(
    std::string_view binary_path) {
  // If binary_path doesn't exist, then tsl::SubProcess will log a bunch of
  // error messages that have confused users in the past. Therefore we first
  // check whether the binary_path exists and error out early if not.
  tsl::Env* env = tsl::Env::Default();
  if (absl::Status file_exists = env->FileExists(std::string{binary_path});
      !file_exists.ok()) {
    return file_exists;
  }

  tsl::SubProcess binary;
  std::string binary_path_str(binary_path);
  binary.SetProgram(binary_path_str, {binary_path_str, "--version"});
  binary.SetChannelAction(tsl::CHAN_STDOUT, tsl::ACTION_PIPE);
  if (!binary.Start()) {
    return absl::InternalError(
        absl::StrFormat("Couldn't invoke %s --version", binary_path));
  }

  std::string out;
  int exit_code = binary.Communicate(/*stdin_input=*/nullptr, &out,
                                     /*stderr_output=*/nullptr);
  if (exit_code != 0) {
    return absl::InternalError(absl::StrFormat(
        "Running %s --version returned %d", binary_path, exit_code));
  }

  return out;
}

static absl::StatusOr<SemanticVersion> GetToolVersionImpl(
    std::string_view tool_path) {
  absl::StatusOr<std::string> tool_version = GetToolVersionString(tool_path);
  if (!tool_version.ok()) {
    return absl::FailedPreconditionError(
        absl::StrCat("Couldn't get ptxas/nvlink version string: ",
                     tool_version.status().ToString()));
  }
  static constexpr LazyRE2 kVersionRegex = {R"(\bV(\d+)\.(\d+)\.(\d+)\b)"};
  SemanticVersion version{0, 0, 0};
  std::string_view vmaj_str, vmin_str, vdot_str;
  if (!RE2::PartialMatch(tool_version.value(), *kVersionRegex, &vmaj_str,
                         &vmin_str, &vdot_str) ||
      !absl::SimpleAtoi(vmaj_str, &version.major()) ||
      !absl::SimpleAtoi(vmin_str, &version.minor()) ||
      !absl::SimpleAtoi(vdot_str, &version.patch())) {
    return absl::FailedPreconditionError(
        absl::StrCat("Couldn't parse ptxas/nvlink version in output of ",
                     tool_path, " --version:\n", tool_version.value()));
  }
  return version;
}

absl::StatusOr<SemanticVersion> GetToolVersion(std::string_view tool_path) {
  // This is only implementing a static cache. `GetToolVersionImpl` has the
  // actual business logic.
  static absl::Mutex mutex(absl::kConstInit);
  static auto cache =
      new absl::flat_hash_map<std::string, absl::StatusOr<SemanticVersion>>
          ABSL_GUARDED_BY(mutex);

  absl::MutexLock lock(&mutex);
  auto it = cache->find(tool_path);
  if (it != cache->end()) {
    return it->second;
  }

  return cache->try_emplace(tool_path, GetToolVersionImpl(tool_path))
      .first->second;
}

absl::StatusOr<absl::Span<const uint8_t>> CompileGpuAsmOrGetCached(
    stream_executor::StreamExecutor* executor, const char* ptx,
    GpuAsmOpts compilation_options) {
  using PtxCacheKey = std::tuple<stream_executor::StreamExecutor*, std::string,
                                 GpuAsmOpts::PtxOptionsTuple>;
  using PtxCompilerResult = absl::StatusOr<std::vector<uint8_t>>;
  static absl::Mutex ptx_cache_mutex(absl::kConstInit);
  static auto& ptx_cache ABSL_GUARDED_BY(ptx_cache_mutex) =
      *new absl::flat_hash_map<PtxCacheKey, PtxCompilerResult>();

  absl::MutexLock lock(&ptx_cache_mutex);
  PtxCacheKey cache_key{executor, std::string(ptx),
                        compilation_options.ToTuple()};
  auto it = ptx_cache.find(cache_key);
  if (it == ptx_cache.end()) {
    PtxCompilerResult compiled =
        CompileGpuAsm(executor, ptx, compilation_options);
    it = ptx_cache.emplace(cache_key, std::move(compiled)).first;
  }

  CHECK(it != ptx_cache.end());

  // Failed compilation attempts are cached.
  // Use separate status check and ValueOrDie invocation on ptx_cache
  // entry to avoid value moving introduced by TF_ASSIGN_OR_RETURN.

  if (ABSL_PREDICT_FALSE(!it->second.ok())) {
    return it->second.status();
  }

  const std::vector<uint8_t>& compiled = it->second.value();
  return absl::MakeSpan(compiled);
}

absl::StatusOr<std::vector<uint8_t>> CompileGpuAsm(
    stream_executor::StreamExecutor* executor, const char* ptx_contents,
    GpuAsmOpts options) {
  auto& device_description = executor->GetDeviceDescription();
  int cc_major = device_description.cuda_compute_capability().major;
  int cc_minor = device_description.cuda_compute_capability().minor;
  return CompileGpuAsm(cc_major, cc_minor, ptx_contents, options);
}

absl::StatusOr<std::string> FindCudaExecutable(
    std::string_view binary_name, std::string_view preferred_cuda_dir,
    SemanticVersion minimum_version,
    absl::Span<const SemanticVersion> excluded_versions) {
  std::string binary_filename = std::string{binary_name};
  tsl::io::AppendDotExeIfWindows(binary_filename);

  std::vector<std::string> candidates{};

  // #1 - Check the preferred CUDA directory
  candidates.emplace_back(
      tsl::io::JoinPath(preferred_cuda_dir, "bin", binary_filename));

  // #2 - Check generic CUDA locations if that is preferred over the PATH
  if (!tsl::PreferPtxasFromPath()) {
    for (std::string_view path : tsl::CandidateCudaRoots()) {
      candidates.emplace_back(tsl::io::JoinPath(path, "bin", binary_filename));
    }
  }

  // #3 - Check the PATH environment variable
  if (const auto* path_env_ptr = std::getenv("PATH")) {
    std::string_view path_env{path_env_ptr};

#if defined(PLATFORM_WINDOWS)
    constexpr char kSearchPathSeparator = ';';
#else
    constexpr char kSearchPathSeparator = ':';
#endif

    for (std::string_view path :
         absl::StrSplit(path_env, kSearchPathSeparator)) {
      candidates.emplace_back(tsl::io::JoinPath(path, binary_filename));
    }
  }

  // #4 - Check generic CUDA locations if we didn't do that already in #2
  if (tsl::PreferPtxasFromPath()) {
    for (std::string_view path : tsl::CandidateCudaRoots()) {
      candidates.emplace_back(tsl::io::JoinPath(path, "bin", binary_filename));
    }
  }

  for (const auto& candidate : candidates) {
    VLOG(2) << "Looking for " << candidate;
    auto candidate_version = GetToolVersion(candidate);
    if (!candidate_version.ok()) {
      continue;
    }

    if (candidate_version.value() < minimum_version) {
      VLOG(2) << candidate << " with version " << minimum_version
              << " is too old.";
      continue;
    }

    if (absl::c_find(excluded_versions, candidate_version.value()) !=
        excluded_versions.end()) {
      VLOG(2) << candidate << " has version " << candidate_version.value()
              << " which was explicitly excluded.";
      continue;
    }

    VLOG(2) << "Using " << candidate << " with version "
            << candidate_version.value();
    return candidate;
  }

  return absl::NotFoundError(
      absl::StrCat("Couldn't find a suitable version of ", binary_name,
                   ". The following locations were considered: ",
                   absl::StrJoin(candidates, ", ")));
}

absl::StatusOr<std::string> FindCudaExecutable(
    std::string_view binary_name, std::string_view preferred_cuda_dir) {
  static constexpr SemanticVersion kNoMinimumVersion{0, 0, 0};
  static constexpr absl::Span<const SemanticVersion> kNoExcludedVersions{};
  return FindCudaExecutable(binary_name, preferred_cuda_dir, kNoMinimumVersion,
                            kNoExcludedVersions);
}

static void LogPtxasTooOld(const std::string& ptxas_path, int cc_major,
                           int cc_minor) {
  using AlreadyLoggedSetTy =
      absl::flat_hash_set<std::tuple<std::string, int, int>>;

  static absl::Mutex* mutex = new absl::Mutex;
  static AlreadyLoggedSetTy* already_logged = new AlreadyLoggedSetTy;

  absl::MutexLock lock(mutex);

  if (already_logged->insert(std::make_tuple(ptxas_path, cc_major, cc_minor))
          .second) {
    LOG(WARNING) << "Falling back to the CUDA driver for PTX compilation; "
                    "ptxas does not support CC "
                 << cc_major << "." << cc_minor;
    LOG(WARNING) << "Used ptxas at " << ptxas_path;
  }
}

static void AppendArgsFromOptions(GpuAsmOpts options,
                                  std::vector<std::string>& args) {
  if (options.disable_gpuasm_optimizations) {
    args.push_back("-O0");
  }
  args.insert(args.end(), options.extra_flags.begin(),
              options.extra_flags.end());
}

static absl::StatusOr<std::string> FindPtxAsExecutable(
    std::string_view preferred_cuda_dir) {
  static constexpr SemanticVersion kMinimumSupportedPtxAsVersion{11, 8, 0};
  static constexpr SemanticVersion kBuggyPtxAsVersions[] = {{12, 3, 103}};
  static constexpr std::string_view kPtxAsBinaryName = "ptxas";

  return FindCudaExecutable(kPtxAsBinaryName, preferred_cuda_dir,
                            kMinimumSupportedPtxAsVersion, kBuggyPtxAsVersions);
}

absl::StatusOr<SemanticVersion> GetAsmCompilerVersion(
    std::string_view preferred_cuda_dir) {
  TF_ASSIGN_OR_RETURN(std::string ptxas_path,
                      FindPtxAsExecutable(preferred_cuda_dir));
  return GetToolVersion(ptxas_path);
}

absl::StatusOr<std::vector<uint8_t>> CompileGpuAsmUsingPtxAs(
    int cc_major, int cc_minor, const char* ptx_contents, GpuAsmOpts options,
    bool cancel_if_reg_spill) {
  TF_ASSIGN_OR_RETURN(std::string ptxas_path,
                      FindPtxAsExecutable(options.preferred_cuda_dir));

  // Write ptx into a temporary file.
  std::string ptx_path;
  auto env = tsl::Env::Default();
  if (!env->LocalTempFilename(&ptx_path)) {
    return absl::InternalError("couldn't get temp PTX file name");
  }
  TF_RETURN_WITH_CONTEXT_IF_ERROR(
      tsl::WriteStringToFile(env, ptx_path, ptx_contents),
      "Unable to write PTX contents to: ", ptx_path);
  VLOG(2) << "ptx written to: " << ptx_path;

  absl::Cleanup ptx_cleaner = [&ptx_path] {
    TF_CHECK_OK(tsl::Env::Default()->DeleteFile(ptx_path));
  };

  // Invoke ptxas and collect its output.
  std::string cubin_path;
  if (!env->LocalTempFilename(&cubin_path)) {
    return absl::InternalError("couldn't get temp CUBIN file name");
  }
  absl::Cleanup cubin_cleaner = [&cubin_path] {
    // CUBIN file may never be created, so the failure to delete it should not
    // produce TF error.
    tsl::Env::Default()->DeleteFile(cubin_path).IgnoreError();
  };
  tsl::SubProcess ptxas_info_dumper;
  // On Hopper, default to sm_90a so that all instructions can be used. But
  // only sm_90 is forward compatible, so don't use sm_90a with newer hardware:
  // https://docs.nvidia.com/cuda/cuda-c-programming-guide/index.html#ptx-compatibility
  std::string extension = (cc_major == 9 && cc_minor == 0) ? "a" : "";
  std::vector<std::string> ptxas_args = {
      ptxas_path,
      ptx_path,
      "-o",
      cubin_path,
      absl::StrCat("-arch=sm_", cc_major, cc_minor, extension),
      "--warn-on-spills"};
  if (VLOG_IS_ON(2)) {
    ptxas_args.push_back("-v");
  }
  AppendArgsFromOptions(options, ptxas_args);
  if (VLOG_IS_ON(3)) {
    VLOG(3) << absl::StrJoin(ptxas_args, " ");
  }

  ptxas_info_dumper.SetProgram(ptxas_path, ptxas_args);
  ptxas_info_dumper.SetChannelAction(tsl::CHAN_STDERR, tsl::ACTION_PIPE);
  if (!ptxas_info_dumper.Start()) {
    return absl::InternalError("Failed to launch ptxas");
  }
  std::string stderr_output;
  int exit_status = ptxas_info_dumper.Communicate(
      /*stdin_input=*/nullptr, /*stdout_output=*/nullptr, &stderr_output);
  if (exit_status != 0) {
    //  It happens when the ptxas installed is too old for the current GPU.
    //  Example error message associated with this error code:
    //      ptxas fatal   : Value 'sm_80' is not defined for option 'gpu-name'
    // In that case, fallback to the driver for compilation
    if (absl::StrContains(stderr_output, "ptxas fatal   : Value '") &&
        absl::StrContains(stderr_output,
                          "is not defined for option 'gpu-name'")) {
      LogPtxasTooOld(ptxas_path, cc_major, cc_minor);
      return absl::UnimplementedError(absl::StrFormat(
          "%s ptxas too old. Falling back to the driver to compile.",
          ptxas_path));
    }
    if (IsPtxRegisterAllocationError(stderr_output)) {
      LOG(INFO) << stderr_output;
      return absl::ResourceExhaustedError(stderr_output);
    }

    return absl::InternalError(
        absl::StrFormat("ptxas exited with non-zero error code %d, output: %s",
                        exit_status, stderr_output));
  }
  // Print the verbose output of ptxas.
  if (!stderr_output.empty()) {
    if (absl::StrContains(stderr_output, "warning")) {
      LOG(INFO) << stderr_output;
      if (cancel_if_reg_spill &&
          absl::StrContains(stderr_output, "Registers are spilled")) {
        return xla::Cancelled(
            "Compilation result discarded due to register spilling");
      }
    } else {
      VLOG(2) << stderr_output;
    }
  }

  // Read in the result of compilation and return it as a byte vector.
  std::string cubin;
  TF_RETURN_IF_ERROR(
      tsl::ReadFileToString(tsl::Env::Default(), cubin_path, &cubin));
  std::vector<uint8_t> cubin_vector(cubin.begin(), cubin.end());
  return cubin_vector;
}

absl::StatusOr<std::vector<uint8_t>> BundleGpuAsm(
    std::vector<CubinOrPTXImage> images, GpuAsmOpts options) {
  TF_ASSIGN_OR_RETURN(
      std::string fatbinary_path,
      FindCudaExecutable("fatbinary", options.preferred_cuda_dir));

  // Write images to temporary files.
  std::vector<std::string> image_paths;
  auto env = tsl::Env::Default();
  for (const CubinOrPTXImage& img : images) {
    std::string img_path;
    if (!env->LocalTempFilename(&img_path)) {
      return absl::InternalError(
          "Could not get temporary filenames for images.");
    }
    TF_RETURN_IF_ERROR(tsl::WriteStringToFile(
        env, img_path, std::string(img.bytes.begin(), img.bytes.end())));
    VLOG(2) << "image written to " << img_path;
    image_paths.push_back(std::move(img_path));
  }
  absl::Cleanup image_files_cleaner = [&image_paths] {
    for (const auto& path : image_paths) {
      TF_CHECK_OK(tsl::Env::Default()->DeleteFile(path));
    }
  };

  // Prepare temporary result file.
  std::string result_path;
  if (!env->LocalTempFilename(&result_path)) {
    return absl::InternalError(
        "Could not get temporary filename for fatbin result.");
  }
  absl::Cleanup result_file_cleaner = [&result_path] {
    // This file may never be created, so the failure to delete it should not
    // propagate to TF.
    tsl::Env::Default()->DeleteFile(result_path).IgnoreError();
  };

  // Compute the ptxas options that were used to produce the cubins.
  std::vector<std::string> ptxas_options;
  AppendArgsFromOptions(options, ptxas_options);

  // Invoke fatbinary and collect its output.
  tsl::SubProcess fatbinary;
  std::vector<std::string> fatbinary_args = {
      fatbinary_path, "--64", "--link", "--compress-all",
      absl::StrCat("--create=", result_path)};
  if (!ptxas_options.empty()) {
    auto command_line = absl::StrJoin(ptxas_options, " ");
    fatbinary_args.push_back(absl::StrFormat("--cmdline=%s", command_line));
  }
  assert(images.size() == image_paths.size());
  for (int i = 0; i < images.size(); i++) {
    fatbinary_args.push_back(absl::StrFormat(
        "--image=profile=%s,file=%s", images[i].profile, image_paths[i]));
  }
  if (VLOG_IS_ON(3)) {
    VLOG(3) << absl::StrJoin(fatbinary_args, " ");
  }
  fatbinary.SetProgram(fatbinary_path, fatbinary_args);
  fatbinary.SetChannelAction(tsl::CHAN_STDERR, tsl::ACTION_PIPE);
  if (!fatbinary.Start()) {
    return absl::InternalError("Failed to launch fatbinary.");
  }
  std::string stderr_output;
  int exit_status = fatbinary.Communicate(
      /*stdin_input=*/nullptr, /*stdout_output=*/nullptr, &stderr_output);
  if (exit_status != 0) {
    return absl::InternalError(absl::StrFormat(
        "fatbinary exited with non-zero error code %d, output: %s", exit_status,
        stderr_output));
  }
  if (!stderr_output.empty()) {
    VLOG(2) << stderr_output;
  }

  // Read in the result and return it as a byte vector.
  std::string result_blob;
  TF_RETURN_IF_ERROR(
      tsl::ReadFileToString(tsl::Env::Default(), result_path, &result_blob));
  return std::vector<uint8_t>(result_blob.begin(), result_blob.end());
}

static absl::StatusOr<std::string> FindNvlinkExecutable(
    std::string_view preferred_cuda_dir) {
  static constexpr SemanticVersion kMinimumNvlinkVersion{11, 8, 0};
  static constexpr absl::Span<const SemanticVersion> kNoExcludedVersions{};
  static constexpr std::string_view kNvLinkBinaryName = "nvlink";

  return FindCudaExecutable(kNvLinkBinaryName, preferred_cuda_dir,
                            kMinimumNvlinkVersion, kNoExcludedVersions);
}

absl::StatusOr<SemanticVersion> GetNvLinkVersion(
    std::string_view preferred_cuda_dir) {
  // Make sure nvlink exists and is executable.
  TF_ASSIGN_OR_RETURN(std::string bin_path,
                      FindNvlinkExecutable(preferred_cuda_dir));

  return GetToolVersion(bin_path);
}

absl::StatusOr<std::vector<uint8_t>> LinkUsingNvlink(
    stream_executor::CudaComputeCapability cc,
    std::string_view preferred_cuda_dir, std::vector<CubinOrPTXImage> images) {
  LOG_FIRST_N(INFO, 1) << "Using nvlink for parallel linking";

  TF_ASSIGN_OR_RETURN(std::string bin_path,
                      FindNvlinkExecutable(preferred_cuda_dir));

  if (images.empty()) {
    return std::vector<uint8_t>();
  }

  auto env = tsl::Env::Default();
  std::vector<std::string> temp_files;
  absl::Cleanup cleaners = [&] {
    for (auto& f : temp_files) {
      TF_CHECK_OK(tsl::Env::Default()->DeleteFile(f));
    }
  };
  for (int i = 0; i < images.size(); i++) {
    temp_files.emplace_back();
    TF_RET_CHECK(env->LocalTempFilename(&temp_files.back()));
    temp_files.back() += ".cubin";
    TF_RETURN_IF_ERROR(tsl::WriteStringToFile(
        env, temp_files.back(),
        absl::string_view(reinterpret_cast<const char*>(images[i].bytes.data()),
                          images[i].bytes.size())));
  }
  std::string output_path;
  TF_RET_CHECK(env->LocalTempFilename(&output_path));
  absl::Cleanup output_cleaner = [&] {
    // CUBIN file may never be created, so the failure to delete it should not
    // produce TF error.
    tsl::Env::Default()->DeleteFile(output_path).IgnoreError();
  };
  std::vector<std::string> args;
  args.push_back(bin_path);
  std::string_view extension = (cc.major == 9 && cc.minor == 0) ? "a" : "";
  args.push_back(absl::StrCat("-arch=sm_", cc.major, cc.minor, extension));
  for (int i = 0; i < images.size(); i++) {
    args.push_back(temp_files[i]);
  }
  args.push_back("-o");
  args.push_back(output_path);

  tsl::SubProcess process;
  process.SetProgram(bin_path, args);
  process.SetChannelAction(tsl::CHAN_STDERR, tsl::ACTION_PIPE);

  TF_RET_CHECK(process.Start());
  std::string stderr_output;
  int exit_status = process.Communicate(
      /*stdin_input=*/nullptr, /*stdout_output=*/nullptr, &stderr_output);

  if (exit_status != 0) {
    return absl::InternalError(
        absl::StrFormat("nvlink exited with non-zero error code %d, output: %s",
                        exit_status, stderr_output));
  }

  if (!stderr_output.empty()) {
    if (absl::StrContains(stderr_output, "warning")) {
      LOG(INFO) << stderr_output;
    } else {
      VLOG(2) << stderr_output;
    }
  }

  // Read in the result of compilation and return it as a byte vector.
  std::string cubin;
  TF_RETURN_IF_ERROR(
      tsl::ReadFileToString(tsl::Env::Default(), output_path, &cubin));
  std::vector<uint8_t> cubin_vector(cubin.begin(), cubin.end());
  return cubin_vector;
}

absl::StatusOr<std::vector<uint8_t>> LinkGpuAsm(
    stream_executor::CudaComputeCapability cc,
    stream_executor::StreamExecutor* executor,
    std::vector<CubinOrPTXImage> images) {
  std::unique_ptr<ActivateContext> activation = executor->Activate();

  CUlinkState link_state;
  CUjit_option options[] = {CU_JIT_TARGET};
  CUjit_target target = static_cast<CUjit_target>(cc.major * 10 + cc.minor);
#if CUDA_VERSION >= 12000
  // Even though CUDA 11.8 has Hopper support, SM 9.0a and most Hopper features
  // (WGMMA, TMA, and more) are only supported in CUDA 12+.
  if (cc.major == 9 && cc.minor == 0) {
    target =
        static_cast<CUjit_target>(target + CU_COMPUTE_ACCELERATED_TARGET_BASE);
  }
#endif
  void* option_values[] = {
      // We first cast to an integer type the same size as a pointer, and then
      // we reinterpret that integer as a pointer.
      reinterpret_cast<void*>(static_cast<std::ptrdiff_t>(target))};

  // Both arrays must have the same number of elements.
  static_assert(sizeof(options) / sizeof(options[0]) ==
                sizeof(option_values) / sizeof(option_values[0]));

  TF_RETURN_IF_ERROR(
      cuda::ToStatus(cuLinkCreate(sizeof(options) / sizeof(options[0]), options,
                                  option_values, &link_state)));
  for (auto& image : images) {
    auto status = cuda::ToStatus(cuLinkAddData(
        link_state, CU_JIT_INPUT_CUBIN, static_cast<void*>(image.bytes.data()),
        image.bytes.size(), "", 0, nullptr, nullptr));
    if (!status.ok()) {
      LOG(ERROR) << "cuLinkAddData fails. This is usually caused by stale "
                    "driver version.";
      return status;
    }
  }
  void* cubin_out;
  size_t cubin_size;
  TF_RETURN_IF_ERROR(
      cuda::ToStatus(cuLinkComplete(link_state, &cubin_out, &cubin_size)));
  std::vector<uint8_t> cubin(static_cast<uint8_t*>(cubin_out),
                             static_cast<uint8_t*>(cubin_out) + cubin_size);
  TF_RETURN_IF_ERROR(cuda::ToStatus(cuLinkDestroy(link_state)));
  return std::move(cubin);
}

absl::StatusOr<std::vector<uint8_t>> CompileGpuAsm(int cc_major, int cc_minor,
                                                   const char* ptx_contents,
                                                   GpuAsmOpts options,
                                                   bool cancel_if_reg_spill) {
  if (IsLibNvPtxCompilerSupported()) {
    VLOG(3) << "Compiling GPU ASM with libnvptxcompiler";
    return CompileGpuAsmUsingLibNvPtxCompiler(cc_major, cc_minor, ptx_contents,
                                              options, cancel_if_reg_spill);
  }

  VLOG(3) << "Compiling GPU ASM with PTXAS. Libnvptxcompiler compilation "
             "not supported.";
  return CompileGpuAsmUsingPtxAs(cc_major, cc_minor, ptx_contents, options,
                                 cancel_if_reg_spill);
}

}  // namespace stream_executor
