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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/const_init.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "tensorflow/core/framework/bounds_check.h"
#include "tensorflow/core/framework/op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/op_requires.h"
#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_slice.h"
#include "tensorflow/core/framework/types.h"
#include "tensorflow/core/framework/types.pb.h"
#include "tensorflow/core/framework/versions.pb.h"
#include "tensorflow/core/lib/io/table.h"
#include "tensorflow/core/lib/io/table_builder.h"
#include "tensorflow/core/lib/io/table_options.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/errors.h"
#include "tensorflow/core/platform/file_system.h"
#include "tensorflow/core/platform/stringpiece.h"
#include "tensorflow/core/platform/tstring.h"
#include "tensorflow/core/protobuf/error_codes.pb.h"
#include "tensorflow/core/public/version.h"
#include "tensorflow/core/util/saved_tensor_slice.pb.h"
#include "tensorflow/core/util/saved_tensor_slice_util.h"
#include "tensorflow/core/util/tensor_slice_writer.h"
#include "tensorflow/tsl/platform/errors.h"

namespace fcp {
namespace {

using ::tensorflow::OpKernel;
using ::tensorflow::OpKernelConstruction;
using ::tensorflow::OpKernelContext;

constexpr absl::string_view kSavedTensorSlicesKey = "";

// Returns the host-endian byte representation of `value`.
//
// The `value` must be non-null and must continue to be valid as long as the
// return value is used.
absl::string_view Int64ToHostEndianBytes(int64_t* value) {
  return absl::string_view(reinterpret_cast<const char*>(value),
                           sizeof(int64_t));
}

// Returns `value` intepreted as the host-endian bytes of an `int64_t`.
int64_t Int64FromHostEndianBytes(const char value[sizeof(int64_t)]) {
  return *reinterpret_cast<const int64_t*>(value);
}

// Implementation of the save ops.
//
// This is copied without change from save_restore_tensor.cc because that target
// cannot be  included in `tf_custom_op_library` targets due to its dependency
// on `//third_party/tensorflow/core:framework`.
void SaveTensors(
    OpKernelContext* context,
    tensorflow::checkpoint::TensorSliceWriter::CreateBuilderFunction
        builder_func,
    bool save_slices) {
  const tensorflow::Tensor& filename_t = context->input(0);
  {
    const int64_t size = filename_t.NumElements();
    OP_REQUIRES(
        context, size == 1,
        tensorflow::errors::InvalidArgument(
            "Input 0 (filename) must be a string scalar; got a tensor of ",
            size, "elements"));
  }
  const std::string& filename = filename_t.scalar<tensorflow::tstring>()();

  // Path, names, and slices if save_slices is true.
  const int kFixedInputs = save_slices ? 3 : 2;
  const tensorflow::Tensor& tensor_names_t = context->input(1);
  OP_REQUIRES(
      context,
      tensorflow::FastBoundsCheck(tensor_names_t.NumElements() + kFixedInputs,
                                  std::numeric_limits<int>::max()),
      tensorflow::errors::InvalidArgument("Too many inputs to SaveTensors"));
  const int N = static_cast<int>(tensor_names_t.NumElements());
  const tensorflow::tstring* tensor_shapes_and_slices_ptr = nullptr;
  if (save_slices) {
    const tensorflow::Tensor& tensor_shapes_and_slices_t = context->input(2);
    OP_REQUIRES(
        context,
        tensor_shapes_and_slices_t.NumElements() == static_cast<int64_t>(N),
        tensorflow::errors::InvalidArgument(
            "Expected ", N,
            " elements for the tensor "
            "shapes and slices but got ",
            tensor_shapes_and_slices_t.NumElements()));
    tensor_shapes_and_slices_ptr =
        tensor_shapes_and_slices_t.flat<tensorflow::tstring>().data();
  }
  OP_REQUIRES(
      context, context->num_inputs() == N + kFixedInputs,
      tensorflow::errors::InvalidArgument(
          "Expected totally ", N + kFixedInputs,
          " inputs as input #1 (which is a string "
          "tensor of saved names) contains ",
          N, " names, but received ", context->num_inputs(), " inputs"));

  VLOG(1) << "About to save tensors to file " << filename << "...";
  tensorflow::checkpoint::TensorSliceWriter writer(filename,
                                                   std::move(builder_func));

  absl::Status s;
  auto tensor_names_flat = tensor_names_t.flat<tensorflow::tstring>();

  // Process tensors in sorted name order.  This allows us to avoid seeking
  // during restoration in the common case where we are restoring a full
  // checkpoint.
  // RestoreTensorsV2 was changed to sort by file offset, so this sorting isn't
  // strictly necessary anymore. However, restores with TF version <= 2.7 will
  // still benefit.
  std::vector<int> sorted_name_idx(tensor_names_flat.size());
  std::iota(sorted_name_idx.begin(), sorted_name_idx.end(), 0);
  std::sort(sorted_name_idx.begin(), sorted_name_idx.end(),
            [&tensor_names_flat](size_t a, size_t b) {
              return tensor_names_flat(a) < tensor_names_flat(b);
            });

  for (const int i : sorted_name_idx) {
    const std::string& name = tensor_names_flat(i);
    const tensorflow::Tensor& input = context->input(i + kFixedInputs);
    tensorflow::TensorShape shape(input.shape());
    tensorflow::TensorSlice slice(input.dims());
    if (save_slices && !tensor_shapes_and_slices_ptr[i].empty()) {
      const tensorflow::tstring& shape_spec = tensor_shapes_and_slices_ptr[i];
      tensorflow::TensorShape slice_shape;
      OP_REQUIRES_OK(context, tensorflow::checkpoint::ParseShapeAndSlice(
                                  shape_spec, &shape, &slice, &slice_shape));
      OP_REQUIRES(context, slice_shape.IsSameSize(input.shape()),
                  tensorflow::errors::InvalidArgument(
                      "Slice in shape_and_slice "
                      "specification does not match the "
                      "shape of the tensor to  save: ",
                      shape_spec, ", tensor: ", input.shape().DebugString()));
    }

#define WRITER_ADD(T)                                           \
  case tensorflow::DataTypeToEnum<T>::value:                    \
    s = writer.Add(name, shape, slice, input.flat<T>().data()); \
    break;

    switch (input.dtype()) {
      TF_CALL_SAVE_RESTORE_TYPES(WRITER_ADD)
      default:
        context->SetStatus(tensorflow::errors::Unimplemented(
            "Saving data type ", DataTypeString(input.dtype()),
            " not yet supported"));
        return;
    }
#undef WRITER_ADD
    if (!s.ok()) {
      context->SetStatus(s);
      return;
    }
  }

  s = writer.Finish();
  if (!s.ok()) {
    context->SetStatus(s);
  }
}

// A `WritableFile` that wraps an existing file, appending a chunk with a length
// footer to the end of it.
//
// File start position is stored as a footer since `WritableFile` does not allow
// `Seek`ing to modify an earlier position in the file.
class AppendedFileWithStartPosFooter : public tensorflow::WritableFile {
 public:
  static absl::Status FromFile(
      std::unique_ptr<tensorflow::WritableFile> file,
      std::unique_ptr<tensorflow::WritableFile>& wrapped_file_out) {
    int64_t body_start;
    TF_RETURN_IF_ERROR(file->Tell(&body_start));
    VLOG(1) << "Appending to checkpoint with starting position " << body_start;
    // Note: cannot use `make_unique` due to private constructor.
    wrapped_file_out = std::unique_ptr<tensorflow::WritableFile>(
        new AppendedFileWithStartPosFooter(std::move(file), body_start));
    return absl::OkStatus();
  }
  absl::Status Append(absl::string_view data) override {
    return file_->Append(data);
  }
  absl::Status Close() override {
    TF_RETURN_IF_ERROR(file_->Append(Int64ToHostEndianBytes(&body_start_)));
    return file_->Close();
  }
  absl::Status Flush() override { return file_->Flush(); }
  absl::Status Sync() override { return file_->Sync(); }
  absl::Status Tell(int64_t* position) override {
    int64_t internal_position;
    TF_RETURN_IF_ERROR(file_->Tell(&internal_position));
    *position = internal_position - body_start_;
    return absl::OkStatus();
  }

 private:
  AppendedFileWithStartPosFooter(std::unique_ptr<tensorflow::WritableFile> file,
                                 int64_t body_start)
      : file_(std::move(file)), body_start_(body_start) {}

  std::unique_ptr<tensorflow::WritableFile> file_;
  int64_t body_start_;
};

// An implementation of the `TensorSliceWriter::Builder` interface which
// delegates to `tensorflow::table::TableBuilder`.
class TableBuilder : public tensorflow::checkpoint::TensorSliceWriter::Builder {
 public:
  TableBuilder(std::string name, std::unique_ptr<tensorflow::WritableFile> file)
      : name_(std::move(name)), file_(std::move(file)) {
    tensorflow::table::Options option;
    option.compression = tensorflow::table::kNoCompression;
    builder_ =
        std::make_unique<tensorflow::table::TableBuilder>(option, file_.get());
  }
  void Add(absl::string_view key, absl::string_view val) override {
    builder_->Add(key, val);
  }
  absl::Status Finish(int64_t* file_size) override {
    *file_size = -1;
    absl::Status s = builder_->Finish();
    if (s.ok()) {
      s = file_->Close();
      if (s.ok()) {
        *file_size = builder_->FileSize();
      }
    }
    if (!s.ok()) {
      s = tensorflow::errors::Internal(
#if TF_GRAPH_DEF_VERSION < 1467
          "Error writing (tmp) checkpoint file: ", name_, ": ",
          s.error_message());
#else
          "Error writing (tmp) checkpoint file: ", name_, ": ", s.message());
#endif
    }
    return s;
  }

 private:
  std::string name_;
  std::unique_ptr<tensorflow::WritableFile> file_;
  std::unique_ptr<tensorflow::table::TableBuilder> builder_;
};

// Creates a new `TensorSliceWriter::Builder` which will append the tensor
// slices to `filename` along with a footer indicating the start position of
// this particular chunk of slices.
//
// If this method returns `OK`, `builder` will contain a new owned pointer to
// a `TensorSliceWriter::Builder`.
absl::Status CreateAppendingTensorSliceBuilder(
    const std::string& filename,
    tensorflow::checkpoint::TensorSliceWriter::Builder** builder) {
  *builder = nullptr;
  if (VLOG_IS_ON(1)) {
    uint64_t file_size = 0;
    if (tensorflow::Env::Default()->GetFileSize(filename, &file_size).ok()) {
      VLOG(1) << "Appending checkpoint to file " << filename << " with size "
              << file_size;
    } else {
      VLOG(1) << "Appending checkpoint to new file " << filename;
    }
  }
  std::unique_ptr<tensorflow::WritableFile> file;
  TF_RETURN_IF_ERROR(
      tensorflow::Env::Default()->NewAppendableFile(filename, &file));
  std::unique_ptr<tensorflow::WritableFile> wrapped_file;
  TF_RETURN_IF_ERROR(
      AppendedFileWithStartPosFooter::FromFile(std::move(file), wrapped_file));
  *builder = new TableBuilder(filename, std::move(wrapped_file));
  return absl::OkStatus();
}

// A `RandomAccessFile` which wraps another `RandomAccessFile`, providing access
// to only a portion of the file.
class PartialRandomAccessFile : public tensorflow::RandomAccessFile {
 public:
  // Constructs a `PartialRandomAccessFile` pointing to a segment of `file`.
  //
  // `file` must be non-null and must continue to be valid as long as the
  // return value is used.
  PartialRandomAccessFile(tensorflow::RandomAccessFile* file, int64_t start,
                          int64_t end)
      : file_(file), start_(start), end_(end) {}
  ~PartialRandomAccessFile() override = default;
  absl::Status Read(uint64_t offset, size_t n, absl::string_view* result,
                    char* scratch) const override {
    const size_t max_allowable_n = end_ - (start_ + offset);
    bool read_too_long = n > max_allowable_n;
    if (read_too_long) {
      n = max_allowable_n;
    }
    TF_RETURN_WITH_CONTEXT_IF_ERROR(
        file_->Read(offset + start_, n, result, scratch),
        absl::StrCat("Reading from PartialRandomAccessFile at offset ", offset,
                     " from start position ", start_));
    if (read_too_long) {
      return absl::Status(absl::StatusCode::kOutOfRange,
                          "Attempted to read past end of file chunk.");
    }
    return absl::OkStatus();
  }

 private:
  tensorflow::RandomAccessFile* file_;
  int64_t start_;
  int64_t end_;
};

struct TableIteratorComparator {
  // Returns whether `i1` should come after `i2` in the priority queue.
  // That is, whether `i1` has *lower* priority than `i2`.
  bool operator()(const std::unique_ptr<tensorflow::table::Iterator>& i1,
                  const std::unique_ptr<tensorflow::table::Iterator>& i2) {
    // Ensure that iterators which have no remaining elements go last in the
    // list.
    if (!i2->Valid()) {
      return false;
    }
    if (!i1->Valid()) {
      return true;
    }
    if ((i2->key() == kSavedTensorSlicesKey) &&
        (i1->key() != kSavedTensorSlicesKey)) {
      return true;
    }
    return i1->key() > i2->key();
  }
};

// Pops and returns the top element of a `std::priority_queue`.
template <class Element, class Container, class Comparator>
Element PopWithElement(
    std::priority_queue<Element, Container, Comparator>& queue) {
  Element e = std::move(const_cast<Element&>(queue.top()));
  queue.pop();
  return e;
}

// Parses a `serialized` into a `SavedTensorSlices` stored in `meta_out`.
absl::Status MetadataFromString(absl::string_view serialized,
                                tensorflow::SavedTensorSlices& meta_out) {
  // NOTE: The conversion to `std::string` is unfortunately necessary here
  // because the OSS version of `ParseFromString` takes a `const std::string&`
  // rather than a `absl::string_view`.
  if (!meta_out.ParseFromString(std::string(serialized))) {
    return absl::Status(
        absl::StatusCode::kInternal,
        absl::StrCat("Failed to parse table entry as `SavedTensorSlices`: ",
                     serialized));
  }
  return absl::OkStatus();
}

// Merges appended checkpoints in `filename` into a single checkpoint.
//
// Note: this function accepts `filename` as a `const std::string&` rather than
// `string_view` because that is the type accepted by the functions it calls
// (`GetFileSize` and `NewRandomAccessFile`). This avoids unnecessary
// allocation.
absl::Status LoadAndMergeAppendedSlices(const std::string& filename) {
  tensorflow::Env* env = tensorflow::Env::Default();
  uint64_t file_size;
  TF_RETURN_IF_ERROR(env->GetFileSize(filename, &file_size));
  // Short-circuit on empty files so that we can assume at least a single entry
  // below.
  if (file_size == 0) {
    return absl::OkStatus();
  }
  std::unique_ptr<tensorflow::RandomAccessFile> file;
  TF_RETURN_IF_ERROR(env->NewRandomAccessFile(filename, &file));

  // Overwrite the underlying file, relying on `file` above to provide a handle
  // into the old file contents even after it is overwritten.
  TF_RETURN_IF_ERROR(tensorflow::Env::Default()->DeleteFile(filename));

  // `chunk_files` and `chunk_tables` must be kept around since they are
  // referenced internally by `chunk_iterators`.
  std::vector<std::unique_ptr<tensorflow::RandomAccessFile>> chunk_files;
  std::vector<std::unique_ptr<tensorflow::table::Table>> chunk_tables;
  std::priority_queue<std::unique_ptr<tensorflow::table::Iterator>,
                      std::vector<std::unique_ptr<tensorflow::table::Iterator>>,
                      TableIteratorComparator>
      chunk_iterators;

  tensorflow::SavedTensorSlices merged_sts;
  tensorflow::SavedTensorSliceMeta* merged_meta = merged_sts.mutable_meta();
  std::set<std::string> slices_added;

  // Read all of the chunks into tables.
  int64_t chunk_footer_end = file_size;
  bool version_was_set = false;
  while (chunk_footer_end > 0) {
    // Read in the footer telling us where the chunk started.
    char footer_scratch[sizeof(int64_t)];
    absl::string_view chunk_footer;
    TF_RETURN_IF_ERROR(file->Read(chunk_footer_end - sizeof(int64_t),
                                  sizeof(int64_t), &chunk_footer,
                                  footer_scratch));
    int64_t chunk_start = Int64FromHostEndianBytes(chunk_footer.data());
    int64_t chunk_end = chunk_footer_end - sizeof(int64_t);
    int64_t chunk_len = chunk_end - chunk_start;
    std::unique_ptr<tensorflow::RandomAccessFile> chunk_file =
        std::make_unique<PartialRandomAccessFile>(file.get(), chunk_start,
                                                  chunk_end);
    tensorflow::table::Options options;
    tensorflow::table::Table* raw_table;
    TF_RETURN_WITH_CONTEXT_IF_ERROR(
        tensorflow::table::Table::Open(options, chunk_file.get(), chunk_len,
                                       &raw_table),
        absl::StrCat("Error opening sub-table of file ", filename,
                     " starting at ", chunk_start, " and ending at ", chunk_end,
                     ". Total file size: ", file_size));
    std::unique_ptr<tensorflow::table::Table> table(raw_table);
    tensorflow::table::Iterator* raw_iterator = table->NewIterator();
    std::unique_ptr<tensorflow::table::Iterator> iterator(raw_iterator);
    iterator->SeekToFirst();
    if (!iterator->Valid()) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Unexpected immediately-invalid iterator. "
                          "Expected table to iterator to have at least a "
                          "single entry (metadata)");
    }
    if (iterator->key() != kSavedTensorSlicesKey) {
      return absl::Status(
          absl::StatusCode::kInternal,
          absl::StrCat("Expected table iterator to have an initial metadata "
                       "entry with key `",
                       kSavedTensorSlicesKey, "`, found key `", iterator->key(),
                       "`"));
    }
    tensorflow::SavedTensorSlices sts;
    TF_RETURN_IF_ERROR(MetadataFromString(iterator->value(), sts));
    iterator->Next();
    if (!version_was_set) {
      version_was_set = true;
      *merged_meta->mutable_versions() = sts.meta().versions();
    }
    for (const tensorflow::SavedSliceMeta& slice_meta : sts.meta().tensor()) {
      if (slices_added.find(slice_meta.name()) != slices_added.end()) {
        return absl::Status(
            absl::StatusCode::kInvalidArgument,
            absl::StrCat(
                "Attempted to merge two checkpoint entries for slice name: `",
                slice_meta.name(), "`. Only one entry per name is permitted."));
      }
      slices_added.insert(slice_meta.name());
    }
    merged_meta->mutable_tensor()->MergeFrom(sts.meta().tensor());
    chunk_iterators.push(std::move(iterator));
    chunk_files.push_back(std::move(chunk_file));
    chunk_tables.push_back(std::move(table));
    chunk_footer_end = chunk_start;
  }
  VLOG(1) << "Merging " << chunk_files.size() << " checkpoint chunks from file "
          << filename;

  tensorflow::checkpoint::TensorSliceWriter::Builder* raw_builder;
  TF_RETURN_IF_ERROR(tensorflow::checkpoint::CreateTableTensorSliceBuilder(
      filename, &raw_builder));
  std::unique_ptr<tensorflow::checkpoint::TensorSliceWriter::Builder> builder(
      raw_builder);

  // First, we add the merged entry which holds a `SavedTensorSlices` proto.
  builder->Add(kSavedTensorSlicesKey, merged_sts.SerializeAsString());

  // Then the remaining entries are concatenated alphabetically.
  while (chunk_iterators.top()->Valid()) {
    std::unique_ptr<tensorflow::table::Iterator> iter =
        PopWithElement(chunk_iterators);
    VLOG(2) << "Merging table entry for key " << iter->key();
    builder->Add(iter->key(), iter->value());
    iter->Next();
    chunk_iterators.push(std::move(iter));
  }
  int64_t resulting_file_size;
  TF_RETURN_WITH_CONTEXT_IF_ERROR(builder->Finish(&resulting_file_size),
                                  "Finishing TensorSliceWriter::Builder");
  return absl::OkStatus();
}

ABSL_CONST_INIT absl::Mutex append_mutex(absl::kConstInit);

}  // namespace

class AppendSlicesOp : public OpKernel {
 public:
  explicit AppendSlicesOp(OpKernelConstruction* context) : OpKernel(context) {}

  void Compute(OpKernelContext* context) override {
    absl::MutexLock lock(&append_mutex);
    const tensorflow::Tensor& filename_t = context->input(0);
    tensorflow::tstring filename = filename_t.flat<tensorflow::tstring>()(0);
    SaveTensors(
        context,
        [context](
            const std::string& target_filename,
            tensorflow::checkpoint::TensorSliceWriter::Builder** builder) {
          // `TensorSliceWriter` targets writing to a new temporary file which
          // it then moves into the location of the final file once complete.
          // In order to comply with this behavior while still retaining
          // "append" semantics, the original file (if it exists) is first moved
          // into the temporary target location.
          tensorflow::tstring original_filename =
              context->input(0).scalar<tensorflow::tstring>()();
          absl::Status status = tensorflow::Env::Default()->RenameFile(
              original_filename, target_filename);
          if (status.ok()) {
            VLOG(1) << "Appending to existing file " << original_filename
                    << " via move to temporary location " << target_filename;
          } else if (status.code() == absl::StatusCode::kNotFound) {
            VLOG(1) << "Appending to new file " << original_filename
                    << " in temporary location " << target_filename;
          } else {
            return status;
          }
          return CreateAppendingTensorSliceBuilder(target_filename, builder);
        },
        /*save_slices=*/true);
  }
};

class MergeAppendedSlicesOp : public OpKernel {
 public:
  explicit MergeAppendedSlicesOp(OpKernelConstruction* context)
      : OpKernel(context) {}

  void Compute(OpKernelContext* context) override {
    absl::MutexLock lock(&append_mutex);
    const tensorflow::Tensor* filename_tensor;
    OP_REQUIRES_OK(context, context->input("filename", &filename_tensor));
    const tensorflow::tstring filename =
        filename_tensor->scalar<tensorflow::tstring>()();
    OP_REQUIRES_OK(context, LoadAndMergeAppendedSlices(filename));
  }
};

// Note: `key` *must* come last so that the indices of the other arguments are
// as expected by `SaveTensors`.
REGISTER_OP("AppendSlices")
    .Input("filename: string")
    .Input("tensor_names: string")
    .Input("shapes_and_slices: string")
    .Input("data: T")
    .Attr("T: list(type)")
    .SetIsStateful();

REGISTER_KERNEL_BUILDER(Name("AppendSlices").Device(tensorflow::DEVICE_CPU),
                        AppendSlicesOp);

REGISTER_OP("MergeAppendedSlices").Input("filename: string").SetIsStateful();

REGISTER_KERNEL_BUILDER(
    Name("MergeAppendedSlices").Device(tensorflow::DEVICE_CPU),
    MergeAppendedSlicesOp);

}  // namespace fcp
