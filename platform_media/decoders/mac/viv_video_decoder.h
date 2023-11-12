// Copyright 2019 Vivaldi Technologies. All rights reserved.
// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_MEDIA_DECODERS_MAC_VIV_VIDEO_DECODER_H_
#define PLATFORM_MEDIA_DECODERS_MAC_VIV_VIDEO_DECODER_H_

#include <set>

#include <VideoToolbox/VideoToolbox.h>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/queue.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "media/base/media_log.h"
#include "media/base/video_decoder.h"
#include "media/video/h264_parser.h"
#include "media/video/h264_poc.h"

namespace media {

// A VideoDecoder that calls the macOS VideoToolbox to decode h.264 media
class VivVideoDecoder : public VideoDecoder {
 public:
  static std::unique_ptr<VideoDecoder> Create(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      MediaLog* media_log);

  ~VivVideoDecoder() override;
  VivVideoDecoder(const VivVideoDecoder&) = delete;
  VivVideoDecoder& operator=(const VivVideoDecoder&) = delete;

  static void DestroyAsync(std::unique_ptr<VivVideoDecoder>);

  // media::VideoDecoder implementation.
  VideoDecoderType GetDecoderType() const override;
  void Output(void* source_frame_refcon,
              OSStatus status,
              CVImageBufferRef image_buffer);
  void Initialize(const VideoDecoderConfig& config,
                  bool low_delay,
                  CdmContext* cdm_context,
                  InitCB init_cb,
                  const OutputCB& output_cb,
                  const WaitingCB& waiting_cb) override;
  void Decode(scoped_refptr<DecoderBuffer> buffer, DecodeCB decode_cb) override;
  void Reset(base::OnceClosure reset_cb) override;
  int GetMaxDecodeRequests() const override;

 private:
  VivVideoDecoder(scoped_refptr<base::SequencedTaskRunner> task_runner,
                  MediaLog* media_log);

  bool FinishDelayedFrames();
  void WriteToMediaLog(MediaLogMessageLevel level, const std::string& message);
  void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id);

  // VideoToolbox
  VTDecompressionOutputCallbackRecord callback_;
  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> format_;
  base::apple::ScopedCFTypeRef<VTDecompressionSessionRef> session_;

  //
  // H264 parsing
  //
  bool waiting_for_idr_ = true;
  bool missing_idr_logged_ = false;
  H264Parser parser_;
  H264POC poc_;

  // Last SPS and PPS seen in the bitstream.
  int last_sps_id_ = -1;
  int last_pps_id_ = -1;
  std::vector<uint8_t> last_sps_;
  std::vector<uint8_t> last_spsext_;
  std::vector<uint8_t> last_pps_;

  // Last SPS and PPS referenced by a slice. In practice these will be the same
  // as the last seen values, unless the bitstream is malformatted.
  std::vector<uint8_t> active_sps_;
  std::vector<uint8_t> active_spsext_;
  std::vector<uint8_t> active_pps_;

  // Last SPS and PPS the decoder was confgured with.
  std::vector<uint8_t> configured_sps_;
  std::vector<uint8_t> configured_spsext_;
  std::vector<uint8_t> configured_pps_;
  gfx::Size configured_size_;

  // Set up VideoToolbox using the current SPS and PPS. Returns true or calls
  // NotifyError() before returning false.
  bool ConfigureDecoder();

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  MediaLog* media_log_;

  bool has_error_ = false;

  InitCB init_cb_;
  OutputCB output_cb_;
  DecodeCB flush_cb_;
  base::OnceClosure reset_cb_;
  std::map<int32_t, DecodeCB> decode_cbs_;
  int32_t bitstream_buffer_id_ = 0;

  VideoDecoderConfig config_;
  bool reinitializing_ = false;

  enum State {
    STATE_DECODING,
    STATE_ERROR,
    STATE_DESTROYING,
  };

  enum TaskType {
    TASK_FRAME,
    TASK_FLUSH,
    TASK_RESET,
    TASK_DESTROY,
  };

  struct Frame {
    explicit Frame(int32_t bitstream_id, base::TimeDelta ts);
    ~Frame();

    // Associated bitstream buffer.
    int32_t bitstream_id;

    // Slice header information.
    bool has_slice = false;
    bool is_idr = false;
    bool has_mmco5 = false;
    int32_t pic_order_cnt = 0;
    int32_t reorder_window = 0;
    base::TimeDelta timestamp;

    // Clean aperture size, as computed by CoreMedia.
    gfx::Size image_size;

    // Decoded image, if decoding was successful.
    base::apple::ScopedCFTypeRef<CVImageBufferRef> image;
  };

  struct Task {
    Task(TaskType type);
    Task(Task&& other);
    ~Task();

    TaskType type;
    std::unique_ptr<Frame> frame;
  };

  struct FrameOrder {
    bool operator()(const std::unique_ptr<Frame>& lhs,
                    const std::unique_ptr<Frame>& rhs) const;
  };

  void DecodeTask(scoped_refptr<DecoderBuffer> buffer, Frame* frame);
  void DecodeDone(Frame* frame);

  // |type| is the type of task that the flush will complete, one of TASK_FLUSH,
  // TASK_RESET, or TASK_DESTROY.
  void QueueFlush(TaskType type);
  void FlushTask(TaskType type);
  void FlushTaskDone(TaskType type);

  void Flush();
  void FlushDone();

  // These methods returns true if a task was completed, false otherwise.
  bool ProcessTaskQueue();
  bool ProcessReorderQueue();
  bool ProcessFrame(const Frame& frame);
  bool SendFrame(const Frame& frame);

  // Try to make progress on tasks in the |task_queue_| or sending frames in the
  // |reorder_queue_|.
  void ProcessWorkQueues();

  // Error handling.
  void EnterErrorState(std::string message);
  void DestroyCallbacks();
  void NotifyError(std::string message);

  State state_ = STATE_DECODING;

  // Queue of pending flush tasks. This is used to drop frames when a reset
  // is pending.
  base::queue<TaskType> pending_flush_tasks_;

  // Queue of tasks to complete
  base::queue<Task> task_queue_;

  // Queue of decoded frames in presentation order.
  std::priority_queue<std::unique_ptr<Frame>,
                      std::vector<std::unique_ptr<Frame>>,
                      FrameOrder>
      reorder_queue_;

  // Frames that have not yet been decoded, keyed by bitstream ID; maintains
  // ownership of Frame objects while they flow through VideoToolbox.
  std::map<int32_t, std::unique_ptr<Frame>> pending_frames_;
  // Set of assigned bitstream IDs, so that Destroy() can release them all.
  std::set<int32_t> assigned_bitstream_ids_;

  base::WeakPtr<VivVideoDecoder> weak_this_;
  base::Thread decoder_thread_;

  // Declared last to ensure that all weak pointers are invalidated before
  // other destructors run.
  base::WeakPtrFactory<VivVideoDecoder> weak_this_factory_;
};

}  // namespace media

#endif  // PLATFORM_MEDIA_DECODERS_MAC_VIV_VIDEO_DECODER_H_
