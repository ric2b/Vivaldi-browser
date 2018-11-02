// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#ifndef PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_MEDIA_DECODER_H_
#define PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_MEDIA_DECODER_H_

#include "platform_media/common/feature_toggles.h"

#import <AudioToolbox/AudioToolbox.h>
#import <AVFoundation/AVFoundation.h>

#include <string>

#include "base/callback.h"
#include "base/task_runner.h"
#include "base/mac/scoped_nsobject.h"
#include "base/mac/sdk_forward_declarations.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "base/synchronization/cancellation_flag.h"
#include "base/threading/thread_checker.h"
#include "media/base/data_source.h"
#include "ui/gfx/geometry/size.h"

@class DataSourceLoader;
@class PlayerNotificationObserver;
@class PlayerObserver;

namespace media {
class DataBuffer;
}  // namespace media

namespace media {

class AVFAudioTap;
class IPCDataSource;

class AVFMediaDecoderClient {
 public:
  virtual ~AVFMediaDecoderClient() {}

  virtual void AudioSamplesReady(
      const scoped_refptr<DataBuffer>& buffer) = 0;
  virtual void VideoFrameReady(
      const scoped_refptr<DataBuffer>& buffer) = 0;
  virtual void StreamHasEnded() = 0;

  virtual bool HasAvailableCapacity() = 0;
};


// The glue between AV Foundation and the media module used for media playback.
//
// This class mediates between AV Foundation and Chrome's classes and data
// structures to allow media decoding through PlatformMediaPipeline on Mac.  An
// AVFMediaDecoder object must be created for each media element that is
// decoded through PlatformMediaPipeline.
//
// AVFMediaDecoder takes raw media data as input and outputs decoded audio and
// video data, handling the demuxing and decoding internally.  Input data is
// provided via an IPCDataSource, and output data is consumed by an
// AVFMediaDecoderClient.
class AVFMediaDecoder {
 public:
  typedef base::Callback<void(bool success)> ResultCB;
  typedef base::RefCountedData<base::CancellationFlag> SharedCancellationFlag;

  // |client| must outlive AVFMediaDecoder.
  explicit AVFMediaDecoder(AVFMediaDecoderClient* client);
  ~AVFMediaDecoder();

  void Initialize(media::DataSource* data_source,
                  const std::string& mime_type,
                  const ResultCB& result_cb);
  void Seek(const base::TimeDelta& time, const ResultCB& result_cb);

  void NotifyStreamCapacityDepleted();
  void NotifyStreamCapacityAvailable();

  bool has_audio_track() const { return AudioTrack() != nil; }
  bool has_video_track() const { return VideoTrack() != nil; }

  base::TimeDelta duration() const { return duration_; }
  base::TimeDelta start_time() const;
  const AudioStreamBasicDescription& audio_stream_format() const {
    DCHECK(!has_audio_track() || is_audio_format_known());
    return audio_stream_format_;
  }
  CMFormatDescriptionRef video_stream_format() const {
    DCHECK(!has_video_track() || is_video_format_known());
    return video_stream_format_;
  }
  CGAffineTransform video_transform() const {
    DCHECK(has_video_track());
    return [VideoTrack() preferredTransform];
  }
  gfx::Size video_coded_size() const { return video_coded_size_; }
  int bitrate() const {
    DCHECK_GE(bitrate_, 0);
    return bitrate_;
  }

 private:
  enum PlaybackState { STARTING, PLAYING, STOPPING, STOPPED };

  void AssetKeysLoaded(const ResultCB& initialize_cb,
                       base::scoped_nsobject<AVAsset> asset,
                       base::scoped_nsobject<NSArray> keys);
  void PlayerStatusKnown(const ResultCB& initialize_cb,
                         base::scoped_nsobject<id> status);

  bool CalculateBitrate();

  void InitializeAudioOutput(const ResultCB& initialize_cb);
  void AudioFormatKnown(const ResultCB& initialize_cb,
                        const AudioStreamBasicDescription& format);
  bool InitializeVideoOutput();

  void AudioSamplesReady(const scoped_refptr<DataBuffer>& buffer);
  void ReadFromVideoOutput(const CMTime& timestamp);

  void AutoSeekDone();
  void SeekDone(const ResultCB& result_cb, bool finished);
  void RunTasksPendingSeekDone();

  void PlayerPlayedToEnd(base::StringPiece source);
  void PlayerItemTimeRangesChanged(base::scoped_nsobject<id> new_ranges);
  void PlayerRateChanged(base::scoped_nsobject<id> new_rate);
  void PlayWhenReady(base::StringPiece reason);

  void PlayIfNotLikelyToStall(base::StringPiece reason, bool likely_to_stall);
  void SeekIfNotLikelyToStall(bool likely_to_stall);

  void ScheduleSeekTask(const base::Closure& seek_task);
  void SeekTask(const base::TimeDelta& time, const ResultCB& result_cb);
  void AutoSeekTask();

  bool is_audio_format_known() const {
    return audio_stream_format_.mSampleRate != 0;
  }
  bool is_video_format_known() const {
    return video_stream_format_ != NULL;
  }

  AVAssetTrack* AssetTrackForType(NSString* track_type_name) const;
  AVAssetTrack* VideoTrack() const;
  AVAssetTrack* AudioTrack() const;
  double VideoFrameRate() const;

  // Returns a background runner of long-running tasks.  Certain AVPlayerItem
  // functions can take a lot more than a few milliseconds, and we can't afford
  // to block the main thread of the GPU process for that long.
  scoped_refptr<base::TaskRunner> background_runner() const;


  AVFMediaDecoderClient* const client_;

  base::scoped_nsobject<DataSourceLoader> data_source_loader_;
  base::scoped_nsobject<AVPlayer> player_;
  base::scoped_nsobject<PlayerObserver> status_observer_;
  base::scoped_nsobject<PlayerObserver> rate_observer_;
  base::scoped_nsobject<PlayerNotificationObserver> played_to_end_observer_;
  base::scoped_nsobject<PlayerObserver> player_item_loaded_times_observer_;
  base::scoped_nsobject<AVPlayerItemVideoOutput> video_output_;
  base::scoped_nsobject<id> time_observer_handle_;

  base::TimeDelta duration_;
  AudioStreamBasicDescription audio_stream_format_;
  CMFormatDescriptionRef video_stream_format_;
  gfx::Size video_coded_size_;
  int bitrate_;

  std::unique_ptr<AVFAudioTap> audio_tap_;

  base::TimeDelta last_audio_timestamp_;
  base::TimeDelta last_video_timestamp_;
  PlaybackState playback_state_;

  // Whether we are currently processing either a user- or auto-initiated seek
  // request.
  bool seeking_;

  // A user- or auto-initiated seek request postponed until AVPlayer is not
  // considered likely to stall for lack of data.
  base::Closure pending_seek_task_;

  // Wraps a |PlayWhenReady()| call to be run once AVPlayer is actually paused
  // following a -[AVPlayer pause] call.
  base::Closure play_on_pause_done_task_;

  // Wraps a |PlayWhenReady()| call to be run once we are done processing a
  // seek request.
  base::Closure play_on_seek_done_task_;

  // Wraps a |Seek()| call to be run once we are done processing an auto-seek
  // request.
  base::Closure seek_on_seek_done_task_;

  bool stream_has_ended_;
  base::TimeDelta min_loaded_range_size_;

  scoped_refptr<SharedCancellationFlag> background_tasks_canceled_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<AVFMediaDecoder> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AVFMediaDecoder);
};

}  // namespace media

#endif  // PLATFORM_MEDIA_GPU_DECODERS_MAC_AVF_MEDIA_DECODER_H_
