// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "content/common/gpu/media/avf_audio_tap.h"

#include <deque>

#import <AVFoundation/AVFoundation.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/data_buffer.h"
#include "media/base/mac/avfoundation_glue.h"
#include "media/base/mac/framework_type_conversions.h"
#include "media/base/mac/mediatoolbox_glue.h"

namespace content {

namespace {

// The maximum number of DataBuffers pre-allocated ahead of time so that
// Process() doesn't have to allocate memory (which would interfere with its
// real-time processing nature, according to the documentation of
// MTAudioProcessingTapProcessCallback).
const size_t kMaxPreallocatedBufferCount = 5;

}  // namespace

namespace audio_tap {

// A context for the audio processor.  Manages DataBuffers that the tap
// callbacks fill with decoded audio.  Dispatches tasks between audio tap
// threads and the Chrome platform media pipeline thread.
class Context {
 public:
  struct InitParams {
    AVFAudioTap::FormatKnownCB format_known_cb;
    AVFAudioTap::SamplesReadyCB samples_ready_cb;

    // The task runner of the Chrome platform media pipeline thread.
    scoped_refptr<base::SingleThreadTaskRunner> task_runner;
  };

  explicit Context(const InitParams& params);
  ~Context();

  static void Destroy(Context* context);
  static Context* FromTap(MediaToolboxGlue::MTAudioProcessingTapRef tap);

  void FormatKnown(const AudioStreamBasicDescription& processing_format,
                   MediaToolboxGlue::CMItemCount max_frame_count);

  scoped_refptr<media::DataBuffer> GetDataBuffer(int buffer_size);

  void BufferFilled(const scoped_refptr<media::DataBuffer>& buffer);

  const AudioStreamBasicDescription& stream_format() const {
    return stream_format_;
  }

 private:
  void PreallocateBuffers(int size);
  void BufferFilledInternal(const scoped_refptr<media::DataBuffer>& buffer);

  AVFAudioTap::FormatKnownCB format_known_cb_;
  AVFAudioTap::SamplesReadyCB samples_ready_cb_;

  // Used to protect access to |buffers|.
  base::Lock buffer_lock_;
  std::deque<scoped_refptr<media::DataBuffer>> buffers_;

  base::Callback<void(const scoped_refptr<media::DataBuffer>&)>
      buffer_filled_cb_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  AudioStreamBasicDescription stream_format_;

  base::WeakPtrFactory<Context> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

Context::Context(const InitParams& params)
    : format_known_cb_(params.format_known_cb),
      samples_ready_cb_(params.samples_ready_cb),
      task_runner_(params.task_runner),
      weak_ptr_factory_(this) {
  DCHECK(task_runner_->BelongsToCurrentThread());
  buffer_filled_cb_ = base::Bind(&Context::BufferFilledInternal,
                                 weak_ptr_factory_.GetWeakPtr());
}

Context::~Context() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
}

// static
void Context::Destroy(Context* context) {
  context->task_runner_->DeleteSoon(FROM_HERE, context);
}

// static
inline Context* Context::FromTap(
    MediaToolboxGlue::MTAudioProcessingTapRef tap) {
  return reinterpret_cast<Context*>(
      MediaToolboxGlue::MTAudioProcessingTapGetStorage(tap));
}

void Context::FormatKnown(const AudioStreamBasicDescription& processing_format,
                          MediaToolboxGlue::CMItemCount max_frame_count) {
  stream_format_ = processing_format;
  DCHECK_NE(stream_format_.mFormatFlags & kAudioFormatFlagIsNonInterleaved, 0u);

  if (!format_known_cb_.is_null()) {
    task_runner_->PostTask(FROM_HERE,
                           base::Bind(format_known_cb_, stream_format_));
  }

  const int buffer_size = max_frame_count * stream_format_.mBytesPerFrame *
                          stream_format_.mChannelsPerFrame;
  DVLOG(3) << "Audio Tap Processor buffer size: " << buffer_size;
  PreallocateBuffers(buffer_size);
}

scoped_refptr<media::DataBuffer> Context::GetDataBuffer(int buffer_size) {
  scoped_refptr<media::DataBuffer> buffer;

  {
    base::AutoLock auto_lock(buffer_lock_);
    if (!buffers_.empty()) {
      buffer = buffers_.front();
      buffers_.pop_front();
      buffer->set_data_size(buffer_size);
    }
  }

  if (!buffer) {
    DVLOG(1) << "Ran out of pre-allocated buffers";
    buffer = new media::DataBuffer(buffer_size);
    buffer->set_data_size(buffer_size);
  }

  return buffer;
}

void Context::BufferFilled(const scoped_refptr<media::DataBuffer>& buffer) {
  task_runner_->PostTask(FROM_HERE, base::Bind(buffer_filled_cb_, buffer));
}

void Context::PreallocateBuffers(int size) {
  base::AutoLock auto_lock(buffer_lock_);
  while (buffers_.size() < kMaxPreallocatedBufferCount) {
    buffers_.push_back(new media::DataBuffer(size));
    buffers_.back()->set_data_size(size);
  }
}

void Context::BufferFilledInternal(
    const scoped_refptr<media::DataBuffer>& buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  samples_ready_cb_.Run(buffer);

  PreallocateBuffers(buffer->data_size());
}

void Init(MediaToolboxGlue::MTAudioProcessingTapRef tap,
          void* client_info,
          void** tap_storage_out) {
  DCHECK(client_info != NULL);
  DVLOG(1) << "Initialising the Audio Tap Processor";

  const Context::InitParams& params =
      *reinterpret_cast<Context::InitParams*>(client_info);
  *tap_storage_out = new Context(params);
}

void Finalize(MediaToolboxGlue::MTAudioProcessingTapRef tap) {
  DVLOG(1) << "Finalizing the Audio Tap Processor";

  Context* context = Context::FromTap(tap);
  Context::Destroy(context);
}

void Prepare(MediaToolboxGlue::MTAudioProcessingTapRef tap,
             MediaToolboxGlue::CMItemCount max_frame_count,
             const AudioStreamBasicDescription* processing_format) {
  DVLOG(1) << "Preparing the Audio Tap Processor";

  Context* context = Context::FromTap(tap);
  context->FormatKnown(*processing_format, max_frame_count);
}

void Process(MediaToolboxGlue::MTAudioProcessingTapRef tap,
             MediaToolboxGlue::CMItemCount frame_count,
             MediaToolboxGlue::MTAudioProcessingTapFlags flags,
             AudioBufferList* buffer_list,
             MediaToolboxGlue::CMItemCount* frame_count_out,
             MediaToolboxGlue::MTAudioProcessingTapFlags* flags_out) {
  DVLOG(5) << "The Audio Tap Processor is processing";

  Context* context = Context::FromTap(tap);

  CMTimeRange time_range;
  const OSStatus status = MediaToolboxGlue::MTAudioProcessingTapGetSourceAudio(
      tap,
      frame_count,
      buffer_list,
      flags_out,
      reinterpret_cast<CoreMediaGlue::CMTimeRange*>(&time_range),
      frame_count_out);

  if (status != 0) {
    DVLOG(1) << "There was an error getting audio buffers from the stream.";
    return;
  }
  DCHECK_EQ(frame_count, *frame_count_out);
  // TODO(wdzierzanowski): How to handle this?  This is set for some movies.
  DVLOG_IF(
      1,
      (((flags | *flags_out) &
        MediaToolboxGlue::kMTAudioProcessingTapFlag_StartOfStream) != 0))
      << "StartOfStream flag set";
  DCHECK(((flags | *flags_out) &
          MediaToolboxGlue::kMTAudioProcessingTapFlag_EndOfStream) == 0);

  const base::TimeDelta timestamp = media::CMTimeToTimeDelta(time_range.start);
  const base::TimeDelta duration =
      media::CMTimeToTimeDelta(time_range.duration);

  DVLOG(1) << "Timestamp: " << timestamp.InMicroseconds() << ", duration "
           << duration.InMicroseconds();
  if (duration.InMicroseconds() <= 0) {
    DVLOG(3) << "Non-positive duration " << duration.InMicroseconds()
             << " @ timestamp " << timestamp.InMicroseconds()
             << ", dropping audio buffers";
    return;
  }
  const unsigned channel_buffer_size =
      buffer_list->mNumberBuffers > 0 ? buffer_list->mBuffers[0].mDataByteSize
                                      : 0;
  const unsigned channel_count = context->stream_format().mChannelsPerFrame;
  DCHECK_EQ(channel_count, buffer_list->mNumberBuffers);

  const int buffer_size = channel_buffer_size * channel_count;
  scoped_refptr<media::DataBuffer> buffer = context->GetDataBuffer(buffer_size);

  for (unsigned channel = 0; channel < channel_count; ++channel) {
    const AudioBuffer& channel_buffer = buffer_list->mBuffers[channel];
    DCHECK_EQ(channel_buffer_size, channel_buffer.mDataByteSize);
    std::copy((const uint8_t*)channel_buffer.mData,
              (const uint8_t*)channel_buffer.mData + channel_buffer_size,
              buffer->writable_data() + channel * channel_buffer_size);
  }

  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);

  context->BufferFilled(buffer);
}

void Unprepare(MediaToolboxGlue::MTAudioProcessingTapRef tap) {
  DVLOG(1) << "Unpreparing the Audio Tap Processor";
}

}  // audio_tap

AVFAudioTap::AVFAudioTap(
    AVAssetTrack* audio_track,
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
    const FormatKnownCB& format_known_cb,
    const SamplesReadyCB& samples_ready_cb)
    : audio_track_(audio_track),
      task_runner_(task_runner),
      format_known_cb_(format_known_cb),
      samples_ready_cb_(samples_ready_cb) {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

AVFAudioTap::~AVFAudioTap() {
  DVLOG(1) << __FUNCTION__;
}

base::scoped_nsobject<AVAudioMix> AVFAudioTap::GetAudioMix() {
  DCHECK(task_runner_->BelongsToCurrentThread());

  TRACE_EVENT0("IPC_MEDIA", __FUNCTION__);

  audio_tap::Context::InitParams params;
  params.task_runner = task_runner_;
  params.format_known_cb = base::ResetAndReturn(&format_known_cb_);
  params.samples_ready_cb = samples_ready_cb_;

  MediaToolboxGlue::MTAudioProcessingTapCallbacks tap_callbacks;
  tap_callbacks.version =
      MediaToolboxGlue::kMTAudioProcessingTapCallbacksVersion_0;
  tap_callbacks.clientInfo = &params;
  tap_callbacks.init = audio_tap::Init;
  tap_callbacks.prepare = audio_tap::Prepare;
  tap_callbacks.process = audio_tap::Process;
  tap_callbacks.unprepare = audio_tap::Unprepare;
  tap_callbacks.finalize = audio_tap::Finalize;

  MediaToolboxGlue::MTAudioProcessingTapRef tap = NULL;
  const OSStatus status = MediaToolboxGlue::MTAudioProcessingTapCreate(
      kCFAllocatorDefault,
      reinterpret_cast<MediaToolboxGlue::MTAudioProcessingTapCallbacks*>
          (&tap_callbacks),
      MediaToolboxGlue::kMTAudioProcessingTapCreationFlag_PreEffects,
      &tap);
  if (status != 0 || tap == NULL) {
    DVLOG(1) << "Unable to create the audio processing tap";
    return base::scoped_nsobject<AVAudioMix>();
  }
  DCHECK(tap != NULL);
  base::ScopedCFTypeRef<MediaToolboxGlue::MTAudioProcessingTapRef> scoped_tap(
      tap);

  CrAVMutableAudioMixInputParameters* input_parameters =
      reinterpret_cast<CrAVMutableAudioMixInputParameters*>
          ([AVFoundationGlue::AVMutableAudioMixInputParametersClass()
              audioMixInputParametersWithTrack:audio_track_]);
  [input_parameters setAudioTapProcessor:tap];

  AVMutableAudioMix* audio_mix =
      [AVFoundationGlue::AVMutableAudioMixClass() audioMix];
  [audio_mix setInputParameters:@[input_parameters]];

  return base::scoped_nsobject<AVAudioMix>([audio_mix retain]);
}

}  // namespace content
