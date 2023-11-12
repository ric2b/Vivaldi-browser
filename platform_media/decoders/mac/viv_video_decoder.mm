// Copyright 2019 Vivaldi Technologies. All rights reserved.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This code is a merging of vda_video_decoder and vt_video_decode_accelerator,
// modified to create a VideoDecoder that is not dependant on the GPU process.
// The decoder calles the macOS VideoToolbox to decode h.264 media.
// Preprocessing is done with an H264 parser, the same way as in
// vt_video_decode_accelerator. The output from VideoToolbox is then wrapped
// inside a VideoFrame and sent onward via the output callback.

#include "platform_media/decoders/mac/viv_video_decoder.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CVPixelBuffer.h>
#include <CoreVideo/CoreVideo.h>

#include "base/apple/osstatus_logging.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/sys_byteorder.h"
#include "media/base/async_destroy_video_decoder.h"
#include "media/base/decoder_status.h"
#include "media/base/video_frame.h"

#define NOTIFY_STATUS(name, status)       \
  do {                                    \
    OSSTATUS_DLOG(ERROR, status) << name; \
    NotifyError(name);                    \
  } while (0)

namespace media {

// Only H.264 with 4:2:0 chroma sampling is supported.
const VideoCodecProfile kSupportedProfiles[] = {
    H264PROFILE_BASELINE, H264PROFILE_EXTENDED, H264PROFILE_MAIN,
    H264PROFILE_HIGH,

    // TODO(sandersd): Hi10p fails during
    // CMVideoFormatDescriptionCreateFromH264ParameterSets with
    // kCMFormatDescriptionError_InvalidParameter.
    //
    // H264PROFILE_HIGH10PROFILE,

    // TODO(sandersd): Find and test media with these profiles before enabling.
    //
    // H264PROFILE_SCALABLEBASELINE,
    // H264PROFILE_SCALABLEHIGH,
    // H264PROFILE_STEREOHIGH,
    // H264PROFILE_MULTIVIEWHIGH,
};

int32_t ComputeReorderWindow(const H264SPS* sps) {
  // When |pic_order_cnt_type| == 2, decode order always matches presentation
  // order.
  // TODO(sandersd): For |pic_order_cnt_type| == 1, analyze the delta cycle to
  // find the minimum required reorder window.
  if (sps->pic_order_cnt_type == 2)
    return 0;

  // TODO(sandersd): Compute MaxDpbFrames.
  int32_t max_dpb_frames = 16;

  // See AVC spec section E.2.1 definition of |max_num_reorder_frames|.
  if (sps->vui_parameters_present_flag && sps->bitstream_restriction_flag) {
    return std::min(sps->max_num_reorder_frames, max_dpb_frames);
  } else if (sps->constraint_set3_flag) {
    if (sps->profile_idc == 44 || sps->profile_idc == 86 ||
        sps->profile_idc == 100 || sps->profile_idc == 110 ||
        sps->profile_idc == 122 || sps->profile_idc == 244) {
      return 0;
    }
  }
  return max_dpb_frames;
}

// Size to use for NALU length headers in AVC format (can be 1, 2, or 4).
const int kNALUHeaderLength = 4;

// Maximum number of frames to queue for reordering. (Also controls the maximum
// number of in-flight frames, since NotifyEndOfBitstreamBuffer() is called when
// frames are moved into the reorder queue.)
//
// Since the maximum possible |reorder_window| is 16 for H.264, 17 is the
// minimum safe (static) size of the reorder queue.
const int kMaxReorderQueueSize = 17;

// Generates nonnegative bitstream buffer IDs, which are assumed to be unique.
int32_t NextID(int32_t* counter) {
  int32_t value = *counter;
  *counter = (*counter + 1) & 0x3FFFFFFF;
  return value;
}

bool IsProfileSupported(VideoCodecProfile profile, gfx::Size coded_size) {
  for (const auto& supported_profile : kSupportedProfiles) {
    if (profile == supported_profile) {
      return true;
    }
  }
  return false;
}

// Build an |image_config| dictionary for VideoToolbox initialization.
base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> BuildImageConfig(
    CMVideoDimensions coded_dimensions) {
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> image_config;

  // Note that 4:2:0 textures cannot be used directly as RGBA in OpenGL, but are
  // lower power than 4:2:2 when composited directly by CoreAnimation.
  int32_t pixel_format = kCVPixelFormatType_420YpCbCr8Planar;
#define CFINT(i) CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &i)
  base::apple::ScopedCFTypeRef<CFNumberRef> cf_pixel_format(CFINT(pixel_format));
  base::apple::ScopedCFTypeRef<CFNumberRef> cf_width(CFINT(coded_dimensions.width));
  base::apple::ScopedCFTypeRef<CFNumberRef> cf_height(CFINT(coded_dimensions.height));
#undef CFINT
  if (!cf_pixel_format.get() || !cf_width.get() || !cf_height.get())
    return image_config;

  image_config.reset(CFDictionaryCreateMutable(
      kCFAllocatorDefault,
      3,  // capacity
      &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
  if (!image_config.get())
    return image_config;

  CFDictionarySetValue(image_config, kCVPixelBufferPixelFormatTypeKey,
                       cf_pixel_format);
  CFDictionarySetValue(image_config, kCVPixelBufferWidthKey, cf_width);
  CFDictionarySetValue(image_config, kCVPixelBufferHeightKey, cf_height);

  return image_config;
}

// This function is used as |no_longer_needed_cb| of
// VideoFrame::WrapExternalYuvData to make sure we keep reference to DataBuffer
// object as long as we need it.
void BufferHolder(const CVImageBufferRef& buffer) {
  /* Intentionally empty */
}

// static
std::unique_ptr<VideoDecoder> VivVideoDecoder::Create(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log) {
  auto* decoder = new VivVideoDecoder(std::move(task_runner), media_log);
  return std::make_unique<AsyncDestroyVideoDecoder<VivVideoDecoder>>(
      base::WrapUnique(decoder));
}

VivVideoDecoder::Frame::Frame(int32_t bitstream_id, base::TimeDelta ts)
    : bitstream_id(bitstream_id), timestamp(ts) {}

VivVideoDecoder::Frame::~Frame() {}

VivVideoDecoder::Task::Task(TaskType type) : type(type) {}

VivVideoDecoder::Task::Task(Task&& other) = default;

VivVideoDecoder::Task::~Task() {}

bool VivVideoDecoder::FrameOrder::operator()(
    const std::unique_ptr<Frame>& lhs,
    const std::unique_ptr<Frame>& rhs) const {
  return lhs->timestamp > rhs->timestamp;
}

void decompressionSessionDecodeFrameCallback(void* decompressionOutputRefCon,
                                             void* sourceFrameRefCon,
                                             OSStatus status,
                                             VTDecodeInfoFlags infoFlags,
                                             CVImageBufferRef imageBuffer,
                                             CMTime presentationTimeStamp,
                                             CMTime presentationDuration) {
  VivVideoDecoder* decoder =
      reinterpret_cast<VivVideoDecoder*>(decompressionOutputRefCon);
  decoder->Output(sourceFrameRefCon, status, imageBuffer);
}

VivVideoDecoder::VivVideoDecoder(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    MediaLog* media_log)
    : task_runner_(std::move(task_runner)),
      media_log_(std::move(media_log)),
      decoder_thread_("VivaldiVTDecoderThread"),
      weak_this_factory_(this) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(media_log_);

  weak_this_ = weak_this_factory_.GetWeakPtr();

  callback_.decompressionOutputCallback =
      decompressionSessionDecodeFrameCallback;
  callback_.decompressionOutputRefCon = this;
}

VivVideoDecoder::~VivVideoDecoder() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  decoder_thread_.Stop();
}

// static
void VivVideoDecoder::DestroyAsync(std::unique_ptr<VivVideoDecoder> decoder) {
  DVLOG(3) << __func__;
  DCHECK(decoder->task_runner_->RunsTasksInCurrentSequence());

  if (!decoder->decoder_thread_.IsRunning()) {
    // Initialize was not called or failed.
    return;
  }

  // For a graceful shutdown, return assigned buffers and flush before
  // destructing |this|.
  for (int32_t bitstream_id : decoder->assigned_bitstream_ids_)
    decoder->NotifyEndOfBitstreamBuffer(bitstream_id);
  decoder->assigned_bitstream_ids_.clear();
  decoder->state_ = STATE_DESTROYING;
  decoder->QueueFlush(TASK_DESTROY);
  decoder->media_log_ = nullptr;

  // The decoder will be deleted after we hop to the decoder thread and back to
  // the media thread in FlushTaskDone.
  decoder.release();
}

VideoDecoderType VivVideoDecoder::GetDecoderType() const {
  return VideoDecoderType::kVivVTVideo;
}

void VivVideoDecoder::Output(void* source_frame_refcon,
                             OSStatus status,
                             CVImageBufferRef image_buffer) {
  // Sometimes, for unknown reasons (http://crbug.com/453050), |image_buffer| is
  // NULL, which causes CFGetTypeID() to crash. While the rest of the code would
  // smoothly handle NULL as a dropped frame, we choose to fail permanantly here
  // until the issue is better understood.
  if (!image_buffer || CFGetTypeID(image_buffer) != CVPixelBufferGetTypeID()) {
    NotifyError("Decoded frame is not a CVPixelBuffer");
    return;
  }

  Frame* frame = reinterpret_cast<Frame*>(source_frame_refcon);
  frame->image.reset(image_buffer, base::scoped_policy::RETAIN);

  task_runner_->PostTask(FROM_HERE, base::BindOnce(&VivVideoDecoder::DecodeDone,
                                                   weak_this_, frame));
}

void VivVideoDecoder::DecodeDone(Frame* frame) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  // pending_frames_.erase() will delete |frame|.
  int32_t bitstream_id = frame->bitstream_id;
  DCHECK_EQ(1u, pending_frames_.count(bitstream_id));

  if (state_ == STATE_ERROR || state_ == STATE_DESTROYING) {
    // DestroyAsync() handles NotifyEndOfBitstreamBuffer().
    pending_frames_.erase(bitstream_id);
    return;
  }

  DCHECK_EQ(state_, STATE_DECODING);
  if (!frame->image.get()) {
    pending_frames_.erase(bitstream_id);
    assigned_bitstream_ids_.erase(bitstream_id);
    NotifyEndOfBitstreamBuffer(bitstream_id);
    return;
  }

  Task task(TASK_FRAME);
  task.frame = std::move(pending_frames_[bitstream_id]);
  pending_frames_.erase(bitstream_id);
  task_queue_.push(std::move(task));
  ProcessWorkQueues();
}

void VivVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool low_delay,
                                 CdmContext* cdm_context,
                                 InitCB init_cb,
                                 const OutputCB& output_cb,
                                 const WaitingCB& waiting_cb) {
  DVLOG(3) << __func__ << "(" << config.AsHumanReadableString() << ")";
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(config.IsValidConfig());
  DCHECK(!init_cb_);
  DCHECK(!flush_cb_);
  DCHECK(!reset_cb_);
  DCHECK(decode_cbs_.empty());

  if (has_error_) {
    std::move(init_cb).Run(DecoderStatus::Codes::kFailed);
    return;
  }

  bool reinitializing = config_.IsValidConfig();

  // Store |init_cb| ASAP so that EnterErrorState() can use it. Leave |config_|
  // alone for now so that the checks can inspect it.
  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;

  // Verify that the configuration is supported.
  if (reinitializing && config.codec() != config_.codec()) {
    EnterErrorState("Codec cannot be changed");
    return;
  }

  if (config.is_encrypted()) {
    EnterErrorState("Encrypted streams are not supported");
    return;
  }

  if (std::find(std::begin(kSupportedProfiles), std::end(kSupportedProfiles),
                config.profile()) == std::end(kSupportedProfiles)) {
    EnterErrorState("Unsupported profile");
    return;
  }

  if (!IsProfileSupported(config.profile(), config.coded_size())) {
    EnterErrorState("Unsupported profile");
    return;
  }

  // The configuration is supported.
  config_ = config;

  reinitializing_ = false;

  // Spawn a thread to handle parsing and calling VideoToolbox.
  if (!decoder_thread_.IsRunning() && !decoder_thread_.Start()) {
    DLOG(ERROR) << "Failed to start decoder thread";
    return;
  }
  std::move(init_cb_).Run(DecoderStatus::Codes::kOk);
}

void VivVideoDecoder::NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__ << "(" << bitstream_buffer_id << ")";
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (has_error_)
    return;

  const auto decode_cb_it = decode_cbs_.find(bitstream_buffer_id);
  if (decode_cb_it == decode_cbs_.end()) {
    EnterErrorState("Unknown bitstream buffer");
    return;
  }

  // Run a local copy in case the decode callback modifies |decode_cbs_|.
  DecodeCB decode_cb = std::move(decode_cb_it->second);
  decode_cbs_.erase(decode_cb_it);
  std::move(decode_cb).Run(DecoderStatus::Codes::kOk);
}

void VivVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             DecodeCB decode_cb) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!init_cb_);
  DCHECK(!flush_cb_);
  DCHECK(!reset_cb_);
  DCHECK(buffer->end_of_stream() || !buffer->decrypt_config());

  if (has_error_ || !buffer) {
    std::move(decode_cb).Run(DecoderStatus::Codes::kInvalidArgument);
    return;
  }

  if (buffer->end_of_stream()) {
    flush_cb_ = std::move(decode_cb);
    Flush();
    return;
  }

  DCHECK_EQ(state_, STATE_DECODING);
  // Assign a bitstream buffer ID and record the decode request.
  int32_t bitstream_id = NextID(&bitstream_buffer_id_);
  decode_cbs_[bitstream_id] = std::move(decode_cb);

  Frame* frame = new Frame(bitstream_id, buffer->timestamp());

  DCHECK_EQ(0u, assigned_bitstream_ids_.count(bitstream_id));
  assigned_bitstream_ids_.insert(bitstream_id);
  pending_frames_[bitstream_id] = base::WrapUnique(frame);

  decoder_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VivVideoDecoder::DecodeTask, base::Unretained(this),
                     std::move(buffer), frame));
}

void VivVideoDecoder::DecodeTask(scoped_refptr<DecoderBuffer> buffer,
                                 Frame* frame) {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  // NALUs are stored with Annex B format in the bitstream buffer (start codes),
  // but VideoToolbox expects AVC format (length headers), so we must rewrite
  // the data.
  //
  // Locate relevant NALUs and compute the size of the rewritten data. Also
  // record parameter sets for VideoToolbox initialization.
  size_t data_size = 0;
  std::vector<H264NALU> nalus;
  parser_.SetStream(buffer->data(), buffer->data_size());
  while (true) {
    H264NALU nalu;
    H264Parser::Result result = parser_.AdvanceToNextNALU(&nalu);
    if (result == H264Parser::kEOStream)
      break;
    if (result == H264Parser::kUnsupportedStream) {
      NotifyError("PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM");
      return;
    }
    if (result != H264Parser::kOk) {
      NotifyError("UNREADABLE_INPUT, SFT_INVALID_STREAM");
      return;
    }
    switch (nalu.nal_unit_type) {
      case H264NALU::kSPS:
        result = parser_.ParseSPS(&last_sps_id_);
        if (result == H264Parser::kUnsupportedStream) {
          NotifyError("PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM");
          return;
        }
        if (result != H264Parser::kOk) {
          NotifyError("UNREADABLE_INPUT, SFT_INVALID_STREAM");
          return;
        }
        last_sps_.assign(nalu.data, nalu.data + nalu.size);
        last_spsext_.clear();
        break;

      case H264NALU::kSPSExt:
        last_spsext_.assign(nalu.data, nalu.data + nalu.size);
        break;

      case H264NALU::kPPS:
        result = parser_.ParsePPS(&last_pps_id_);
        if (result == H264Parser::kUnsupportedStream) {
          NotifyError("PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM");
          return;
        }
        if (result != H264Parser::kOk) {
          NotifyError("UNREADABLE_INPUT, SFT_INVALID_STREAM");
          return;
        }
        last_pps_.assign(nalu.data, nalu.data + nalu.size);
        break;

      case H264NALU::kSliceDataA:
      case H264NALU::kSliceDataB:
      case H264NALU::kSliceDataC:
      case H264NALU::kNonIDRSlice:
      case H264NALU::kIDRSlice:
        // Only the first slice is examined. Other slices are at least one of:
        // the same frame, not decoded, invalid.
        if (!frame->has_slice) {
          // Parse slice header.
          H264SliceHeader slice_hdr;
          result = parser_.ParseSliceHeader(nalu, &slice_hdr);
          if (result == H264Parser::kUnsupportedStream) {
            NotifyError("PLATFORM_FAILURE, SFT_UNSUPPORTED_STREAM");
            return;
          }
          if (result != H264Parser::kOk) {
            NotifyError("UNREADABLE_INPUT, SFT_INVALID_STREAM");
            return;
          }

          // Lookup SPS and PPS.
          DCHECK_EQ(slice_hdr.pic_parameter_set_id, last_pps_id_);
          const H264PPS* pps = parser_.GetPPS(slice_hdr.pic_parameter_set_id);
          if (!pps) {
            NotifyError("UNREADABLE_INPUT, SFT_INVALID_STREAM");
            return;
          }

          DCHECK_EQ(pps->seq_parameter_set_id, last_sps_id_);
          const H264SPS* sps = parser_.GetSPS(pps->seq_parameter_set_id);
          if (!sps) {
            NotifyError("UNREADABLE_INPUT, SFT_INVALID_STREAM");
            return;
          }

          // Record the configuration.
          // TODO(sandersd): Ideally this would be skipped if we know there
          // have not been any parameter sets since the last frame.
          active_sps_ = last_sps_;
          active_spsext_ = last_spsext_;
          active_pps_ = last_pps_;

          // Compute and store frame properties. |image_size| gets filled in
          // later, since it comes from the decoder configuration.
          absl::optional<int32_t> pic_order_cnt =
              poc_.ComputePicOrderCnt(sps, slice_hdr);
          if (!pic_order_cnt.has_value()) {
            NotifyError("UNREADABLE_INPUT, SFT_INVALID_STREAM");
            return;
          }

          frame->has_slice = true;
          frame->is_idr = nalu.nal_unit_type == media::H264NALU::kIDRSlice;
          frame->has_mmco5 = poc_.IsPendingMMCO5();
          frame->pic_order_cnt = *pic_order_cnt;
          frame->reorder_window = ComputeReorderWindow(sps);
        }
        [[fallthrough]];

      default:
        nalus.push_back(nalu);
        data_size += kNALUHeaderLength + nalu.size;
        break;
    }
  }

  if (frame->is_idr)
    waiting_for_idr_ = false;
  // If no IDR has been seen yet, skip decoding. Note that Flash sends
  // configuration changes as a bitstream with only SPS/PPS; we don't print
  // error messages for those.
  if (frame->has_slice && waiting_for_idr_) {
    if (!missing_idr_logged_) {
      missing_idr_logged_ = true;
    }
    frame->has_slice = false;
  }

  // If there is nothing to decode, drop the request by returning a frame with
  // no image.
  if (!frame->has_slice) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VivVideoDecoder::DecodeDone, weak_this_, frame));
    return;
  }

  // Apply any configuration change, but only at an IDR. If there is no IDR, we
  // just hope for the best from the decoder.
  if (frame->is_idr &&
      (configured_sps_ != active_sps_ || configured_spsext_ != active_spsext_ ||
       configured_pps_ != active_pps_)) {
    if (active_sps_.empty()) {
      NotifyError("active_sps_.empty() INVALID_ARGUMENT, SFT_INVALID_STREAM");
      return;
    }
    if (active_pps_.empty()) {
      NotifyError("active_pps_.empty() INVALID_ARGUMENT, SFT_INVALID_STREAM");
      return;
    }
    if (!ConfigureDecoder())
      return;
  }
  // If the session is not configured by this point, fail.
  if (!session_) {
    NotifyError("INVALID_ARGUMENT, SFT_INVALID_STREAM");
    return;
  }

  // Now that the configuration is up to date, copy it into the frame.
  frame->image_size = configured_size_;

  // Create a memory-backed CMBlockBuffer for the translated data.
  // TODO(sandersd): Pool of memory blocks.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> data;
  OSStatus status = CMBlockBufferCreateWithMemoryBlock(
      kCFAllocatorDefault,
      nullptr,              // &memory_block
      data_size,            // block_length
      kCFAllocatorDefault,  // block_allocator
      nullptr,              // &custom_block_source
      0,                    // offset_to_data
      data_size,            // data_length
      0,                    // flags
      data.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMBlockBufferCreateWithMemoryBlock()", status);
    return;
  }

  // Make sure that the memory is actually allocated.
  // CMBlockBufferReplaceDataBytes() is documented to do this, but prints a
  // message each time starting in Mac OS X 10.10.
  status = CMBlockBufferAssureBlockMemory(data);
  if (status) {
    NOTIFY_STATUS("CMBlockBufferAssureBlockMemory()", status);
    return;
  }

  // Copy NALU data into the CMBlockBuffer, inserting length headers.
  size_t offset = 0;
  for (size_t i = 0; i < nalus.size(); i++) {
    H264NALU& nalu = nalus[i];
    uint32_t header = base::HostToNet32(static_cast<uint32_t>(nalu.size));
    status =
        CMBlockBufferReplaceDataBytes(&header, data, offset, kNALUHeaderLength);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status);
      return;
    }
    offset += kNALUHeaderLength;
    status = CMBlockBufferReplaceDataBytes(nalu.data, data, offset, nalu.size);
    if (status) {
      NOTIFY_STATUS("CMBlockBufferReplaceDataBytes()", status);
      return;
    }
    offset += nalu.size;
  }

  // Package the data in a CMSampleBuffer.
  base::apple::ScopedCFTypeRef<CMSampleBufferRef> sample;
  status = CMSampleBufferCreate(kCFAllocatorDefault,
                                data,        // data_buffer
                                true,        // data_ready
                                nullptr,     // make_data_ready_callback
                                nullptr,     // make_data_ready_refcon
                                format_,     // format_description
                                1,           // num_samples
                                0,           // num_sample_timing_entries
                                nullptr,     // &sample_timing_array
                                1,           // num_sample_size_entries
                                &data_size,  // &sample_size_array
                                sample.InitializeInto());
  if (status) {
    NOTIFY_STATUS("CMSampleBufferCreate()", status);
    return;
  }

  // Send the frame for decoding.
  // Asynchronous Decompression allows for parallel submission of frames
  // (without it, DecodeFrame() does not return until the frame has been
  // decoded). We don't enable Temporal Processing because we are not passing
  // timestamps anyway.
  VTDecodeFrameFlags decode_flags =
      kVTDecodeFrame_EnableAsynchronousDecompression;
  status = VTDecompressionSessionDecodeFrame(
      session_,
      sample,                          // sample_buffer
      decode_flags,                    // decode_flags
      reinterpret_cast<void*>(frame),  // source_frame_refcon
      nullptr);                        // &info_flags_out
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionDecodeFrame()", status);
    return;
  }
}

bool VivVideoDecoder::FinishDelayedFrames() {
  DVLOG(3) << __func__;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  if (session_) {
    OSStatus status = VTDecompressionSessionWaitForAsynchronousFrames(session_);
    if (status) {
      NOTIFY_STATUS("VTDecompressionSessionWaitForAsynchronousFrames()",
                    status);
      return false;
    }
  }
  return true;
}

void VivVideoDecoder::ProcessWorkQueues() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  switch (state_) {
    case STATE_DECODING:
      // TODO(sandersd): Batch where possible.
      while (state_ == STATE_DECODING) {
        if (!ProcessReorderQueue() && !ProcessTaskQueue())
          break;
      }
      return;

    case STATE_ERROR:
      // Do nothing until DestroyAsync() is called.
      return;

    case STATE_DESTROYING:
      // std::queue lacks clear().
      task_queue_ = {};
      return;
  }
}

bool VivVideoDecoder::ProcessTaskQueue() {
  DVLOG(3) << __func__ << " size=" << task_queue_.size();
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, STATE_DECODING);

  if (task_queue_.empty())
    return false;

  Task& task = task_queue_.front();
  switch (task.type) {
    case TASK_FRAME: {
      bool reorder_queue_has_space =
          reorder_queue_.size() < kMaxReorderQueueSize;
      bool reorder_queue_flush_needed =
          task.frame->is_idr || task.frame->has_mmco5;
      bool reorder_queue_flush_done = reorder_queue_.empty();
      if (reorder_queue_has_space &&
          (!reorder_queue_flush_needed || reorder_queue_flush_done)) {
        DVLOG(2) << "Decode(" << task.frame->bitstream_id << ") complete";
        assigned_bitstream_ids_.erase(task.frame->bitstream_id);
        NotifyEndOfBitstreamBuffer(task.frame->bitstream_id);
        reorder_queue_.push(std::move(task.frame));
        task_queue_.pop();
        return true;
      }
      return false;
    }

    case TASK_FLUSH:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if (reorder_queue_.size() == 0) {
        DVLOG(1) << "Flush complete";
        pending_flush_tasks_.pop();
        FlushDone();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_RESET:
      DCHECK_EQ(task.type, pending_flush_tasks_.front());
      if (reorder_queue_.size() == 0) {
        DVLOG(1) << "Reset complete";
        waiting_for_idr_ = true;
        pending_flush_tasks_.pop();
        DestroyCallbacks();
        task_queue_.pop();
        return true;
      }
      return false;

    case TASK_DESTROY:
      NOTREACHED() << "Can't destroy while in STATE_DECODING";
      NotifyError("Can't destroy while in STATE_DECODING");
      return false;
  }
}

bool VivVideoDecoder::ProcessReorderQueue() {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, STATE_DECODING);

  if (reorder_queue_.empty())
    return false;

  // If the next task is a flush (because there is a pending flush or because
  // the next frame is an IDR), then we don't need a full reorder buffer to send
  // the next frame.
  bool flushing =
      !task_queue_.empty() && (task_queue_.front().type != TASK_FRAME ||
                               task_queue_.front().frame->is_idr ||
                               task_queue_.front().frame->has_mmco5);

  size_t reorder_window = std::max(0, reorder_queue_.top()->reorder_window);
  DVLOG(3) << __func__ << " size=" << reorder_queue_.size()
           << " window=" << reorder_window << " flushing=" << flushing;
  if (flushing || reorder_queue_.size() > reorder_window) {
    if (ProcessFrame(*reorder_queue_.top())) {
      reorder_queue_.pop();
      return true;
    }
  }
  return false;
}

bool VivVideoDecoder::ProcessFrame(const Frame& frame) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, STATE_DECODING);

  // If the next pending flush is for a reset, then the frame will be dropped.
  bool resetting = !pending_flush_tasks_.empty() &&
                   pending_flush_tasks_.front() == TASK_RESET;

  if (!resetting) {
    DCHECK(frame.image.get());
    if (!SendFrame(frame)) {
      DLOG(ERROR) << "frame " << frame.timestamp << " NOT sent successfully";
      return false;
    }
  }
  return true;
}

bool VivVideoDecoder::SendFrame(const Frame& frame) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK_EQ(state_, STATE_DECODING);
  DCHECK(frame.image.get());

  CVPixelBufferRef image = frame.image.get();

  // Lock the returned image data.
  CVReturn status =
      CVPixelBufferLockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
  if (status != kCVReturnSuccess) {
    DLOG(ERROR) << "Error locking CVPixelBuffer data";
    return false;
  }

  const gfx::Size coded_size(CVImageBufferGetEncodedSize(image));
  const gfx::Rect visible_rect(CVImageBufferGetCleanRect(image));
  const gfx::Size natural_size(CVImageBufferGetDisplaySize(image));

  // Y plane.
  uint8_t* Y_data =
      static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(image, 0));
  uint32_t Y_stride = CVPixelBufferGetBytesPerRowOfPlane(image, 0);

  // Cb plane.
  uint8_t* U_data =
      static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(image, 1));
  uint32_t U_stride = CVPixelBufferGetBytesPerRowOfPlane(image, 1);

  // Cr plane.
  uint8_t* V_data =
      static_cast<uint8_t*>(CVPixelBufferGetBaseAddressOfPlane(image, 2));
  uint32_t V_stride = CVPixelBufferGetBytesPerRowOfPlane(image, 2);

  const OSType cv_format = CVPixelBufferGetPixelFormatType(image);
  VideoPixelFormat format;
  // There are very few compatible CV pixel formats, so just check each.
  if (cv_format == kCVPixelFormatType_420YpCbCr8Planar) {
    format = PIXEL_FORMAT_I420;
  } else if (cv_format == kCVPixelFormatType_444YpCbCr8) {
    format = PIXEL_FORMAT_I444;
  } else if (cv_format == '420v') {
    // TODO(jfroy): Use kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange when the
    // minimum OS X and iOS SDKs permits it.
    format = PIXEL_FORMAT_NV12;
  } else {
    DLOG(ERROR) << "CVPixelBuffer format not supported: " << cv_format;
    return false;
  }

  scoped_refptr<VideoFrame> output = VideoFrame::WrapExternalYuvData(
      format, coded_size, visible_rect, natural_size, Y_stride, U_stride,
      V_stride, Y_data, U_data, V_data, frame.timestamp);

  output->AddDestructionObserver(
      base::BindRepeating(&BufferHolder, frame.image));

  // Unlock the returned image data.
  CVPixelBufferUnlockBaseAddress(image, kCVPixelBufferLock_ReadOnly);
  if (!output_cb_.is_null()) {
    output_cb_.Run(std::move(output));
    return true;
  }
  return false;
}

bool VivVideoDecoder::ConfigureDecoder() {
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());
  DCHECK(!active_sps_.empty());
  DCHECK(!active_pps_.empty());

  // Build the configuration records.
  std::vector<const uint8_t*> nalu_data_ptrs;
  std::vector<size_t> nalu_data_sizes;
  nalu_data_ptrs.reserve(3);
  nalu_data_sizes.reserve(3);
  nalu_data_ptrs.push_back(&active_sps_.front());
  nalu_data_sizes.push_back(active_sps_.size());
  if (!last_spsext_.empty()) {
    nalu_data_ptrs.push_back(&active_spsext_.front());
    nalu_data_sizes.push_back(active_spsext_.size());
  }
  nalu_data_ptrs.push_back(&active_pps_.front());
  nalu_data_sizes.push_back(active_pps_.size());

  // Construct a new format description from the parameter sets.
  format_.reset();
  OSStatus status = CMVideoFormatDescriptionCreateFromH264ParameterSets(
      kCFAllocatorDefault,
      nalu_data_ptrs.size(),     // parameter_set_count
      &nalu_data_ptrs.front(),   // &parameter_set_pointers
      &nalu_data_sizes.front(),  // &parameter_set_sizes
      kNALUHeaderLength,         // nal_unit_header_length
      format_.InitializeInto());

  if (status) {
    NOTIFY_STATUS("CMVideoFormatDescriptionCreateFromH264ParameterSets()",
                  status);
    return false;
  }

  // Prepare VideoToolbox configuration dictionaries.
  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> decoder_config(
      CFDictionaryCreateMutable(kCFAllocatorDefault,
                                1,  // capacity
                                &kCFTypeDictionaryKeyCallBacks,
                                &kCFTypeDictionaryValueCallBacks));

  if (!decoder_config.get()) {
    NotifyError("Failed to create CFMutableDictionary");
    return false;
  }

  // VideoToolbox scales the visible rect to the output size, so we set the
  // output size for a 1:1 ratio. (Note though that VideoToolbox does not handle
  // top or left crops correctly.) We expect the visible rect to be integral.
  CGRect visible_rect = CMVideoFormatDescriptionGetCleanAperture(format_, true);
  CMVideoDimensions visible_dimensions = {
      static_cast<int32_t>(visible_rect.size.width),
      static_cast<int32_t>(visible_rect.size.height)};

  base::apple::ScopedCFTypeRef<CFMutableDictionaryRef> image_config(
      BuildImageConfig(visible_dimensions));

  if (!image_config.get()) {
    NotifyError("Failed to create decoder image configuration");
    return false;
  }

  // Ensure that the old decoder emits all frames before the new decoder can
  // emit any.
  if (!FinishDelayedFrames())
    return false;
  session_.reset();

  status = VTDecompressionSessionCreate(
      kCFAllocatorDefault,
      format_,       // video_format_description
      NULL,          // video_decoder_specification, NULL means VT decides
      image_config,  // destination_image_buffer_attributes
      &callback_,    // output_callback
      session_.InitializeInto());
  if (status) {
    NOTIFY_STATUS("VTDecompressionSessionCreate()", status);
    return false;
  }

  // Record that the configuration change is complete.
  configured_sps_ = active_sps_;
  configured_spsext_ = active_spsext_;
  configured_pps_ = active_pps_;
  configured_size_.SetSize(visible_rect.size.width, visible_rect.size.height);
  return true;
}

int VivVideoDecoder::GetMaxDecodeRequests() const {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  return 4;
}

void VivVideoDecoder::Reset(base::OnceClosure reset_cb) {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!init_cb_);
  // Note: |flush_cb_| may be non-null. If so, the flush can be completed by
  // NotifyResetDone().
  DCHECK(!reset_cb_);

  if (has_error_) {
    task_runner_->PostTask(FROM_HERE, std::move(reset_cb));
    return;
  }

  reset_cb_ = std::move(reset_cb);
  QueueFlush(TASK_RESET);
}

void VivVideoDecoder::FlushTask(TaskType type) {
  DVLOG(3) << __func__;
  DCHECK(decoder_thread_.task_runner()->BelongsToCurrentThread());

  FinishDelayedFrames();

  if (type == TASK_DESTROY && session_) {
    // Destroy the decoding session before returning from the decoder thread.
    VTDecompressionSessionInvalidate(session_);
    session_.reset();
  }

  // Queue a task even if flushing fails, so that destruction always completes.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivVideoDecoder::FlushTaskDone, weak_this_, type));
}

void VivVideoDecoder::FlushTaskDone(TaskType type) {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  if (state_ == STATE_DESTROYING) {
    delete this;
    return;
  }
  task_queue_.push(Task(type));
  ProcessWorkQueues();
}

void VivVideoDecoder::FlushDone() {
  DVLOG(3) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  if (has_error_)
    return;

  if (!flush_cb_)
    return;

  DCHECK(decode_cbs_.empty());
  std::move(flush_cb_).Run(DecoderStatus::Codes::kOk);
}

void VivVideoDecoder::QueueFlush(TaskType type) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  pending_flush_tasks_.push(type);
  decoder_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&VivVideoDecoder::FlushTask,
                                base::Unretained(this), type));

  // If this is a new flush request, see if we can make progress.
  if (state_ != STATE_DESTROYING && pending_flush_tasks_.size() == 1)
    ProcessWorkQueues();
}

void VivVideoDecoder::Flush() {
  DVLOG(1) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  QueueFlush(TASK_FLUSH);
}

void VivVideoDecoder::NotifyError(std::string message) {
  if (!task_runner_->RunsTasksInCurrentSequence()) {
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VivVideoDecoder::NotifyError, weak_this_, message));
  } else if (state_ == STATE_DECODING) {
    state_ = STATE_ERROR;
    EnterErrorState(message);
  }
}

void VivVideoDecoder::EnterErrorState(std::string message) {
  DCHECK(task_runner_->RunsTasksInCurrentSequence());
  DCHECK(weak_this_);

  MEDIA_LOG(ERROR, media_log_) << message;

  if (has_error_)
    return;

  // Start rejecting client calls immediately.
  has_error_ = true;

  // Destroy callbacks aynchronously to avoid calling them on a client stack.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VivVideoDecoder::DestroyCallbacks, weak_this_));
}

void VivVideoDecoder::DestroyCallbacks() {
  DVLOG(2) << __func__;
  DCHECK(task_runner_->RunsTasksInCurrentSequence());

  auto status = DecoderStatus::Codes::kAborted;
  if (has_error_)
    status = DecoderStatus::Codes::kInvalidArgument;

  // We use an on-stack WeakPtr to detect Destroy() being called. Note that any
  // calls to Initialize(), Decode(), or Reset() are asynchronously rejected
  // when |has_error_| is set.
  base::WeakPtr<VivVideoDecoder> weak_this = weak_this_;

  std::map<int32_t, DecodeCB> local_decode_cbs = std::move(decode_cbs_);
  decode_cbs_.clear();
  for (auto& it : local_decode_cbs) {
    std::move(it.second).Run(status);
    if (!weak_this)
      return;
  }

  if (weak_this && flush_cb_)
    std::move(flush_cb_).Run(status);

  if (weak_this && reset_cb_)
    std::move(reset_cb_).Run();

  if (has_error_ && weak_this && init_cb_)
    std::move(init_cb_).Run(DecoderStatus::Codes::kFailed);
}

}  // namespace media
