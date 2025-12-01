/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <cstdint>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/op_requires.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/framework/versions.pb.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/core/util/saved_tensor_slice.pb.h"

namespace fcp {
namespace {

using ::tensorflow::OpKernel;
using ::tensorflow::OpKernelConstruction;
using ::tensorflow::OpKernelContext;

}  // namespace

class DeleteDirOp : public OpKernel {
 public:
  explicit DeleteDirOp(OpKernelConstruction* context) : OpKernel(context) {}

  void Compute(OpKernelContext* context) override {
    const tensorflow::Tensor& dirname_t = context->input(0);
    const tensorflow::Tensor& recursively_t = context->input(1);
    {
      const int64_t size = dirname_t.NumElements();
      OP_REQUIRES(
          context, size == 1,
          tensorflow::errors::InvalidArgument(
              "Input 0 (dirname) must be a string scalar; got a tensor of ",
              size, "elements"));
    }
    {
      const int64_t size = recursively_t.NumElements();
      OP_REQUIRES(
          context, size == 1,
          tensorflow::errors::InvalidArgument(
              "Input 1 (recursively) must be a string scalar; got a tensor of ",
              size, "elements"));
    }
    const tensorflow::tstring& dirname =
        dirname_t.scalar<tensorflow::tstring>()();
    const bool recursively = recursively_t.scalar<bool>()();
    if (context->env()->IsDirectory(dirname).ok()) {
      if (recursively) {
        int64_t undeleted_files = 0;
        int64_t undeleted_dirs = 0;
        absl::Status delete_status = context->env()->DeleteRecursively(
            dirname, &undeleted_files, &undeleted_dirs);
        if (!delete_status.ok()) {
          // The directory could be already deleted by another op. Let's not
          // propagate this error.
          LOG(WARNING) << "Failed to recursively delete the directory '"
                       << dirname << "' (remaining files: " << undeleted_files
                       << ", remaining dirs: " << undeleted_dirs << "). "
                       << delete_status;
        }
      } else {
        absl::Status delete_status = context->env()->DeleteDir(dirname);
        if (!delete_status.ok()) {
          // The directory could be already deleted by another op. Let's not
          // propagate this error.
          LOG(WARNING) << "Failed to delete the directory '" << dirname << "'. "
                       << delete_status;
        }
      }
    }
  }
};

REGISTER_OP("DeleteDir")
    .Input("dirname: string")
    .Input("recursively: bool")
    .SetIsStateful();
REGISTER_KERNEL_BUILDER(Name("DeleteDir").Device(tensorflow::DEVICE_CPU),
                        DeleteDirOp);

class DeleteFileOp : public OpKernel {
 public:
  explicit DeleteFileOp(OpKernelConstruction* context)
      : OpKernel(context) {}

  void Compute(OpKernelContext* context) override {
    const tensorflow::Tensor& filename_t = context->input(0);
    {
      const int64_t size = filename_t.NumElements();
      OP_REQUIRES(
          context, size == 1,
          tensorflow::errors::InvalidArgument(
              "Input 0 (filename) must be a string scalar; got a tensor of ",
              size, "elements"));
    }
    const tensorflow::tstring& filename =
        filename_t.scalar<tensorflow::tstring>()();
    if (context->env()->FileExists(filename).ok()) {
      absl::Status delete_status = context->env()->DeleteFile(filename);
      if (!delete_status.ok()) {
        // The file could be already deleted by another op. Let's not propagate
        // this error.
        LOG(WARNING) << "Failed to delete the file '" << filename << "'. "
                     << delete_status;
      }
    }
  }
};

REGISTER_OP("DeleteFile").Input("filename: string").SetIsStateful();
REGISTER_KERNEL_BUILDER(
    Name("DeleteFile").Device(tensorflow::DEVICE_CPU),
    DeleteFileOp);

}  // namespace fcp
