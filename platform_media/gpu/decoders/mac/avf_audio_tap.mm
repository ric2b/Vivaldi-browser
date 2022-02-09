// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "platform_media/gpu/decoders/mac/avf_audio_tap.h"

#include <deque>

#import <AVFoundation/AVFoundation.h>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/trace_event.h"
#include "media/base/data_buffer.h"
#include "platform_media/common/mac/framework_type_conversions.h"

namespace media {

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

  Context();
  ~Context();

  static void Destroy(Context* context);
  static Context* FromTap(MTAudioProcessingTapRef tap);

  void FormatKnown(const AudioStreamBasicDescription& processing_format,
                   CMItemCount max_frame_count);

  scoped_refptr<DataBuffer> GetDataBuffer(int buffer_size);

  void BufferFilled(scoped_refptr<DataBuffer> buffer);

  const AudioStreamBasicDescription& stream_format() const {
    return stream_format_;
  }

 private:
  friend AVFAudioTap;
  void PreallocateBuffers(int size);
  void BufferFilledInternal(scoped_refptr<DataBuffer> buffer);

  AVFAudioTap::FormatKnownCB format_known_cb_;
  AVFAudioTap::SamplesReadyCB samples_ready_cb_;

  // Used to protect access to |buffers|.
  base::Lock buffer_lock_;
  std::deque<scoped_refptr<DataBuffer>> buffers_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  AudioStreamBasicDescription stream_format_{0};

  DISALLOW_COPY_AND_ASSIGN(Context);
};

Context::Context() = default;

Context::~Context() {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__;
  DCHECK(task_runner_->BelongsToCurrentThread());
  if (format_known_cb_) {
    // Error case - the context was destructed before the format bacame known.
    std::move(format_known_cb_).Run(stream_format_);
  }
}

// static
void Context::Destroy(Context* context) {
  context->task_runner_->DeleteSoon(FROM_HERE, context);
}

// static
inline Context* Context::FromTap(MTAudioProcessingTapRef tap) {
  return reinterpret_cast<Context*>(MTAudioProcessingTapGetStorage(tap));
}

void Context::FormatKnown(const AudioStreamBasicDescription& processing_format,
                          CMItemCount max_frame_count) {
  stream_format_ = processing_format;
  DCHECK_NE(stream_format_.mFormatFlags & kAudioFormatFlagIsNonInterleaved, 0u);

  if (format_known_cb_) {
    task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(format_known_cb_), stream_format_));
  }

  const int buffer_size = max_frame_count * stream_format_.mBytesPerFrame *
                          stream_format_.mChannelsPerFrame;
  VLOG(3) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Audio Tap Processor buffer size: " << buffer_size;
  PreallocateBuffers(buffer_size);
}

scoped_refptr<DataBuffer> Context::GetDataBuffer(int buffer_size) {
  scoped_refptr<DataBuffer> buffer;

  {
    base::AutoLock auto_lock(buffer_lock_);
    if (!buffers_.empty()) {
      buffer = buffers_.front();
      buffers_.pop_front();
      buffer->set_data_size(buffer_size);
    }
  }

  if (!buffer) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Ran out of pre-allocated buffers";
    buffer = base::MakeRefCounted<DataBuffer>(buffer_size);
    buffer->set_data_size(buffer_size);
  }

  return buffer;
}

void Context::BufferFilled(scoped_refptr<DataBuffer> buffer) {
  // Unretained is safe as this instance will be deleted via posting to the
  // task_runner_ after posting the callback there.
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&Context::BufferFilledInternal,
                                base::Unretained(this), std::move(buffer)));
}

void Context::PreallocateBuffers(int size) {
  base::AutoLock auto_lock(buffer_lock_);
  while (buffers_.size() < kMaxPreallocatedBufferCount) {
    buffers_.push_back(base::MakeRefCounted<DataBuffer>(size));
    buffers_.back()->set_data_size(size);
  }
}

void Context::BufferFilledInternal(scoped_refptr<DataBuffer> buffer) {
  DCHECK(task_runner_->BelongsToCurrentThread());

  int data_size = buffer->data_size();
  samples_ready_cb_.Run(std::move(buffer));

  PreallocateBuffers(data_size);
}

void Init(MTAudioProcessingTapRef tap,
          void* client_info,
          void** tap_storage_out) {
  DCHECK(client_info);
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Initialising the Audio Tap Processor";

  // See comments in GetAudioMix why the client_info is a pointer to unique_ptr.
  std::unique_ptr<Context>* context =
      reinterpret_cast<std::unique_ptr<Context>*>(client_info);
  DCHECK(context->get());
  *tap_storage_out = context->release();
}

void Finalize(MTAudioProcessingTapRef tap) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Finalizing the Audio Tap Processor";

  Context* context = Context::FromTap(tap);
  Context::Destroy(context);
}

void Prepare(MTAudioProcessingTapRef tap,
             CMItemCount max_frame_count,
             const AudioStreamBasicDescription* processing_format) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Preparing the Audio Tap Processor";

  Context* context = Context::FromTap(tap);
  context->FormatKnown(*processing_format, max_frame_count);
}

void Process(MTAudioProcessingTapRef tap,
             CMItemCount frame_count,
             MTAudioProcessingTapFlags flags,
             AudioBufferList* buffer_list,
             CMItemCount* frame_count_out,
             MTAudioProcessingTapFlags* flags_out) {
  VLOG(5) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " The Audio Tap Processor is processing";

  Context* context = Context::FromTap(tap);

  CMTimeRange time_range;
  const OSStatus status = MTAudioProcessingTapGetSourceAudio(
      tap, frame_count, buffer_list, flags_out,
      reinterpret_cast<CMTimeRange*>(&time_range), frame_count_out);

  if (status != 0) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Error getting audio buffers from the stream.";
    return;
  }
  DCHECK_EQ(frame_count, *frame_count_out);
  // TODO(wdzierzanowski): How to handle this?  This is set for some movies.
  VLOG_IF(1, (((flags | *flags_out) &
               kMTAudioProcessingTapFlag_StartOfStream) != 0))
      << " PROPMEDIA(GPU) : " << __FUNCTION__ << " StartOfStream flag set";
  DCHECK(((flags | *flags_out) & kMTAudioProcessingTapFlag_EndOfStream) == 0);

  const base::TimeDelta timestamp = CMTimeToTimeDelta(time_range.start);
  const base::TimeDelta duration = CMTimeToTimeDelta(time_range.duration);

  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Timestamp: " << timestamp.InMicroseconds() << ", duration "
          << duration.InMicroseconds();

  if (duration.InMicroseconds() <= 0) {
    LOG(WARNING) << " PROPMEDIA(GPU) : " << __FUNCTION__
                 << " Non-positive duration " << duration.InMicroseconds()
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
  scoped_refptr<DataBuffer> buffer = context->GetDataBuffer(buffer_size);

  for (unsigned channel = 0; channel < channel_count; ++channel) {
    const AudioBuffer& channel_buffer = buffer_list->mBuffers[channel];
    DCHECK_EQ(channel_buffer_size, channel_buffer.mDataByteSize);
    std::copy((const uint8_t*)channel_buffer.mData,
              (const uint8_t*)channel_buffer.mData + channel_buffer_size,
              buffer->writable_data() + channel * channel_buffer_size);
  }

  buffer->set_timestamp(timestamp);
  buffer->set_duration(duration);

  context->BufferFilled(std::move(buffer));
}

void Unprepare(MTAudioProcessingTapRef tap) {
  VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
          << " Unpreparing the Audio Tap Processor";
}

}  // audio_tap

// static
base::scoped_nsobject<AVAudioMix> AVFAudioTap::GetAudioMix(
    AVAssetTrack* audio_track,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    FormatKnownCB format_known_cb,
    SamplesReadyCB samples_ready_cb) {
  DCHECK(task_runner->BelongsToCurrentThread());
  DCHECK(samples_ready_cb);

  TRACE_EVENT0("IPC_MEDIA", __FUNCTION__);

  auto context = std::make_unique<audio_tap::Context>();
  context->task_runner_ = std::move(task_runner);
  context->format_known_cb_ = std::move(format_known_cb);
  context->samples_ready_cb_ = std::move(samples_ready_cb);

  MTAudioProcessingTapCallbacks tap_callbacks;
  tap_callbacks.version = kMTAudioProcessingTapCallbacksVersion_0;

  // Pass the address of context, not context.release() as clientInfo. This way
  // we transfer the context ownership to the tap only when the Init callback
  // will be called. In turn on any error during the tap creation when returning
  // from this method we delete the context and call the known format method.
  tap_callbacks.clientInfo = &context;
  tap_callbacks.init = audio_tap::Init;
  tap_callbacks.prepare = audio_tap::Prepare;
  tap_callbacks.process = audio_tap::Process;
  tap_callbacks.unprepare = audio_tap::Unprepare;
  tap_callbacks.finalize = audio_tap::Finalize;

  MTAudioProcessingTapRef tap = nullptr;
  const OSStatus status = MTAudioProcessingTapCreate(
      kCFAllocatorDefault, &tap_callbacks,
      kMTAudioProcessingTapCreationFlag_PreEffects, &tap);
  if (status != 0 || !tap) {
    VLOG(1) << " PROPMEDIA(GPU) : " << __FUNCTION__
            << " Unable to create the audio processing tap";
    return base::scoped_nsobject<AVAudioMix>();
  }
  DCHECK(tap);
  base::ScopedCFTypeRef<MTAudioProcessingTapRef> scoped_tap(tap);

  AVMutableAudioMixInputParameters* input_parameters =
      reinterpret_cast<AVMutableAudioMixInputParameters*>(
          [AVMutableAudioMixInputParameters
              audioMixInputParametersWithTrack:audio_track]);
  [input_parameters setAudioTapProcessor:tap];

  AVMutableAudioMix* audio_mix = [AVMutableAudioMix audioMix];
  [audio_mix setInputParameters:@[ input_parameters ]];

  return base::scoped_nsobject<AVAudioMix>([audio_mix retain]);
}

}  // namespace media
