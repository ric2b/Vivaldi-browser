// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA.

#include "content/common/gpu/media/avf_media_decoder.h"

#include "base/lazy_instance.h"
#include "base/mac/foundation_util.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "content/common/gpu/media/avf_audio_tap.h"
#import "content/common/gpu/media/data_source_loader.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/data_buffer.h"
#include "media/base/mac/avfoundation_glue.h"
#include "media/base/mac/framework_type_conversions.h"
#include "media/base/timestamp_constants.h"
#include "media/base/video_frame.h"
#include "media/filters/platform_media_pipeline_types_mac.h"
#include "net/base/mime_util.h"

namespace {
typedef base::Callback<void(base::scoped_nsobject<id>)> PlayerObserverCallback;
}  // namespace

@interface AVURLAsset (MavericksSDK)
@property(nonatomic, readonly) CrAVAssetResourceLoader* resourceLoader;
@end

@interface PlayerObserver : NSObject {
 @private
  NSString* keyPath_;
  PlayerObserverCallback callback_;
}

@property (retain,readonly) NSString* keyPath;

- (id)initForKeyPath:(NSString*)keyPath
        withCallback:(const PlayerObserverCallback&)callback;
@end

@implementation PlayerObserver

@synthesize keyPath = keyPath_;

- (id)initForKeyPath:(NSString*)keyPath
        withCallback:(const PlayerObserverCallback&)callback {
  if ((self = [super init])) {
    keyPath_ = [keyPath retain];
    callback_ = callback;
  }
  return self;
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:[self keyPath]]) {
    callback_.Run(base::scoped_nsobject<id>(
        [[change objectForKey:NSKeyValueChangeNewKey] retain]));
  }
}

@end

@interface PlayerNotificationObserver : NSObject {
 @private
  base::Closure callback_;
}
- (id)initWithCallback:(const base::Closure&)callback;
@end

@implementation PlayerNotificationObserver

- (id)initWithCallback:(const base::Closure&)callback {
  if ((self = [super init])) {
    callback_ = callback;
  }
  return self;
}

- (void)observe:(NSNotification*)notification {
  callback_.Run();
}

@end

namespace content {

namespace {

// The initial value of the amount of data that we require AVPlayer to have in
// order to consider it unlikely to stall right after starting to play.
const base::TimeDelta kInitialRequiredLoadedTimeRange =
    base::TimeDelta::FromMilliseconds(300);

// Each time AVPlayer runs out of data we increase the required loaded time
// range size up to this value.
const base::TimeDelta kMaxRequiredLoadedTimeRange =
    base::TimeDelta::FromSeconds(4);

class BackgroundThread {
 public:
  BackgroundThread() : thread_("OpMediaDecoder") { CHECK(thread_.Start()); }

  scoped_refptr<base::TaskRunner> task_runner() const {
    return thread_.task_runner();
  }

 private:
  base::Thread thread_;
};

base::LazyInstance<BackgroundThread> g_background_thread =
    LAZY_INSTANCE_INITIALIZER;

class ScopedBufferLock {
 public:
  explicit ScopedBufferLock(CVPixelBufferRef buffer) : buffer_(buffer) {
    CVPixelBufferLockBaseAddress(buffer_, kCVPixelBufferLock_ReadOnly);
  }
  ~ScopedBufferLock() {
    CVPixelBufferUnlockBaseAddress(buffer_, kCVPixelBufferLock_ReadOnly);
  }

 private:
  const CVPixelBufferRef buffer_;
};

scoped_refptr<media::DataBuffer> GetVideoFrame(
    AVPlayerItemVideoOutput* video_output,
    const CMTime& timestamp,
    const gfx::Size& coded_size) {
  TRACE_EVENT0("IPC_MEDIA", __FUNCTION__);

  base::ScopedCFTypeRef<CVPixelBufferRef> pixel_buffer;
  if ([video_output hasNewPixelBufferForItemTime:timestamp]) {
    pixel_buffer.reset([video_output copyPixelBufferForItemTime:timestamp
                                             itemTimeForDisplay:nil]);
  }
  if (pixel_buffer == NULL) {
    DVLOG(3) << "No pixel buffer available for time "
             << media::CMTimeToTimeDelta(timestamp).InMicroseconds();
    return NULL;
  }

  DCHECK_EQ(CVPixelBufferGetPlaneCount(pixel_buffer), 3u);

  // TODO(wdzierzanowski): Don't copy pixel buffers to main memory, share GL
  // textures with the render process instead. Will be investigated in work
  // package DNA-21454.

  ScopedBufferLock auto_lock(pixel_buffer);

  int strides[3] = { 0 };
  size_t plane_sizes[3] = { 0 };
  const int video_frame_planes[] = { media::VideoFrame::kYPlane,
                                     media::VideoFrame::kUPlane,
                                     media::VideoFrame::kVPlane };

  // The planes in the pixel buffer are YUV, but PassThroughVideoDecoder
  // assumes YVU, so we switch the order of the last two planes.
  const int planes[] = { 0, 2, 1 };
  for (int i = 0; i < 3; ++i) {
    const int plane = planes[i];

    // TODO(wdzierzanowski): Use real stride values for video config. Will be
    // fixed in work package DNA-21454
    strides[plane] = CVPixelBufferGetBytesPerRowOfPlane(pixel_buffer, plane);
    DVLOG(7) << "strides[" << plane << "] = " << strides[plane];

    plane_sizes[plane] = strides[plane] * media::VideoFrame::PlaneSize(
          media::PIXEL_FORMAT_YV12,
          video_frame_planes[plane],
          coded_size).height();
  }

  // Copy all planes into contiguous memory.
  const int data_size = plane_sizes[0] + plane_sizes[1] + plane_sizes[2];
  scoped_refptr<media::DataBuffer> video_data_buffer =
      new media::DataBuffer(data_size);
  size_t data_offset = 0;
  for (int i = 0; i < 3; ++i) {
    const int plane = planes[i];

    memcpy(video_data_buffer->writable_data() + data_offset,
           CVPixelBufferGetBaseAddressOfPlane(pixel_buffer, plane),
           plane_sizes[plane]);

    data_offset += plane_sizes[plane];
  }

  video_data_buffer->set_data_size(data_size);
  video_data_buffer->set_timestamp(media::CMTimeToTimeDelta(timestamp));

  return video_data_buffer;
}

void SetAudioMix(
    const scoped_refptr<AVFMediaDecoder::SharedCancellationFlag> canceled,
    AVPlayerItem* item,
    base::scoped_nsobject<AVAudioMix> audio_mix) {
  DCHECK(g_background_thread.Get().task_runner()->RunsTasksOnCurrentThread());
  TRACE_EVENT0("IPC_MEDIA", __FUNCTION__);

  if (canceled->data.IsSet())
    return;

  [item setAudioMix:audio_mix];
}

bool IsPlayerLikelyToStallWithRanges(
    AVPlayerItem* item,
    base::scoped_nsobject<NSArray> loaded_ranges,
    base::TimeDelta min_range_size) {
  // The ranges provided might be discontinuous, but this decoder is interested
  // only in first continuous range, and how much time is buffered in this
  // range. Other ranges are not necessary for playback continuation.
  if ([loaded_ranges count] > 0) {
    CMTimeRange time_range = [[loaded_ranges objectAtIndex:0] CMTimeRangeValue];

    const base::TimeDelta end_of_loaded_range =
        media::CMTimeToTimeDelta(time_range.start) +
        media::CMTimeToTimeDelta(time_range.duration);
    if (end_of_loaded_range >= media::CMTimeToTimeDelta([item duration]))
        return false;

    const base::TimeDelta current_time =
        media::CMTimeToTimeDelta([item currentTime]);
    return end_of_loaded_range - current_time < min_range_size;
  }

  DVLOG(5) << "AVPlayerItem does not have any loadedTimeRanges value";
  return true;
}

bool IsPlayerLikelyToStall(
    const scoped_refptr<AVFMediaDecoder::SharedCancellationFlag> canceled,
    AVPlayerItem* item,
    base::TimeDelta min_range_size) {
  DCHECK(g_background_thread.Get().task_runner()->RunsTasksOnCurrentThread());
  TRACE_EVENT0("IPC_MEDIA", "will stall?");

  if (canceled->data.IsSet())
    return false;

  return IsPlayerLikelyToStallWithRanges(
      item,
      base::scoped_nsobject<NSArray>([[item loadedTimeRanges] retain]),
      min_range_size);
}

}  // namespace


AVFMediaDecoder::AVFMediaDecoder(AVFMediaDecoderClient* client)
    : client_(client),
      audio_stream_format_({0}),
      bitrate_(-1),
      last_audio_timestamp_(media::kNoTimestamp()),
      last_video_timestamp_(media::kNoTimestamp()),
      playback_state_(STOPPED),
      seeking_(false),
      stream_has_ended_(false),
      min_loaded_range_size_(kInitialRequiredLoadedTimeRange),
      background_tasks_canceled_(new SharedCancellationFlag),
      weak_ptr_factory_(this) {
  DCHECK(client_ != NULL);
}

AVFMediaDecoder::~AVFMediaDecoder() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  background_tasks_canceled_->data.Set();

  // Without it, memory allocated by AVFoundation when we call
  // -[AVAssetResourceLoadingDataRequest respondWithData:] in DataSourceLoader
  // is never released.  Yes, it's weird, but I don't know how else we can
  // avoid the memory leak.
  // Also, the AVSimplePlayer demo application does it, too.
  [player_ pause];

  [player_ removeTimeObserver:time_observer_handle_];

  [[player_ currentItem] removeOutput:video_output_];

  // This finalizes the audio processing tap.
  [[player_ currentItem] setAudioMix:nil];

  if (player_item_loaded_times_observer_.get() != nil) {
    [[player_ currentItem]
        removeObserver:player_item_loaded_times_observer_
            forKeyPath:[player_item_loaded_times_observer_ keyPath]];
  }

  [[NSNotificationCenter defaultCenter] removeObserver:played_to_end_observer_];

  if (rate_observer_.get() != nil) {
    [player_ removeObserver:rate_observer_ forKeyPath:[rate_observer_ keyPath]];
  }
  if (status_observer_.get() != nil) {
    [player_ removeObserver:status_observer_
                 forKeyPath:[status_observer_ keyPath]];
  }

  [data_source_loader_ stop];
}

void AVFMediaDecoder::Initialize(IPCDataSource* data_source,
                                 const std::string& mime_type,
                                 const ResultCB& initialize_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  data_source_loader_.reset([[DataSourceLoader alloc]
      initWithDataSource:data_source
            withMIMEType:base::SysUTF8ToNSString(mime_type)]);

  // Use a "custom" URL scheme to force AVURLAsset to ask our instance of
  // AVAssetResourceLoaderDelegate for data.  This way, we make sure all data
  // is fetched through Chrome's network stack rather than by AVURLAsset
  // directly.
  // AVPlayer does not play some links (without extension, with query or
  // containing some characters like ';'). To avoid all future problems with
  // invalid URL, file name set in AVURLAsset is constant.
  AVURLAsset* url_asset = [AVFoundationGlue::AVAssetClass()
      assetWithURL:[NSURL URLWithString:@"opop://media_file.mp4"]];
  base::scoped_nsobject<AVAsset> asset([url_asset retain]);

  [[url_asset resourceLoader] setDelegate:data_source_loader_
                                    queue:[data_source_loader_ dispatchQueue]];

  base::scoped_nsobject<NSArray> asset_keys_to_load_and_test(
      [[NSArray arrayWithObjects:@"playable",
                                 @"hasProtectedContent",
                                 @"tracks",
                                 @"duration",
                                 nil] retain]);
  const base::Closure asset_keys_loaded_cb =
      media::BindToCurrentLoop(base::Bind(&AVFMediaDecoder::AssetKeysLoaded,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          initialize_cb,
                                          asset,
                                          asset_keys_to_load_and_test));

  [url_asset loadValuesAsynchronouslyForKeys:asset_keys_to_load_and_test
                           completionHandler:^{ asset_keys_loaded_cb.Run(); }];
}

void AVFMediaDecoder::Seek(const base::TimeDelta& time,
                           const ResultCB& result_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "AVFMediaDecoder::Seek", this);
  DVLOG(1) << "Seeking to " << time.InMicroseconds() << " per pipeline request";

  if (seeking_) {
    DCHECK(seek_on_seek_done_task_.is_null());
    DVLOG(1) << "Auto-seeking now, postponing pipeline seek request";
    seek_on_seek_done_task_ = base::Bind(&AVFMediaDecoder::Seek,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         time,
                                         result_cb);
    return;
  }

  ScheduleSeekTask(base::Bind(&AVFMediaDecoder::SeekTask,
                              weak_ptr_factory_.GetWeakPtr(),
                              time,
                              result_cb));
}

void AVFMediaDecoder::NotifyStreamCapacityDepleted() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(5) << __FUNCTION__;

  // We were notified by _a_ stream.  Other streams may still have some
  // capacity available.
  if (client_->HasAvailableCapacity())
    return;

  if (playback_state_ != PLAYING)
    return;

  if (seeking_) {
    DVLOG(3) << "Ignoring stream capacity depletion while seeking";
    return;
  }

  DVLOG(1) << "PAUSING AVPlayer";
  playback_state_ = STOPPING;
  [player_ pause];
}

void AVFMediaDecoder::NotifyStreamCapacityAvailable() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(5) << __FUNCTION__;

  PlayWhenReady("stream capacity available");
}

base::TimeDelta AVFMediaDecoder::start_time() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  return media::GetStartTimeFromTrack(has_audio_track() ? AudioTrack()
                                                        : VideoTrack());
}

void AVFMediaDecoder::AssetKeysLoaded(const ResultCB& initialize_cb,
                                      base::scoped_nsobject<AVAsset> asset,
                                      base::scoped_nsobject<NSArray> keys) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  // First test whether the values of each of the keys we need have been
  // successfully loaded.
  for (NSString* key in keys.get()) {
    NSError* error = nil;
    if ([asset statusOfValueForKey:key error:&error] !=
        AVKeyValueStatusLoaded) {
      DVLOG(1) << "Can't access asset key: " << [key UTF8String];
      initialize_cb.Run(false);
      return;
    }
  }

  if (![asset isPlayable]) {
    DVLOG(1) << "Asset is not playable";
    initialize_cb.Run(false);
    return;
  }

  if ([asset hasProtectedContent]) {
    DVLOG(1) << "Asset has protected content";
    initialize_cb.Run(false);
    return;
  }

  // We can play this asset.

  AVPlayerItem* player_item =
      [AVFoundationGlue::AVPlayerItemClass() playerItemWithAsset:asset];
  player_.reset([[AVFoundationGlue::AVPlayerClass() alloc]
      initWithPlayerItem:player_item]);

  const PlayerObserverCallback status_known_cb =
      media::BindToCurrentLoop(base::Bind(&AVFMediaDecoder::PlayerStatusKnown,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          initialize_cb));

  if ([player_ status] == AVPlayerStatusReadyToPlay) {
    status_known_cb.Run(
        base::scoped_nsobject<id>([@(AVPlayerStatusReadyToPlay) retain]));
  } else {
    DCHECK([player_ status] == AVPlayerStatusUnknown);

    status_observer_.reset(
        [[PlayerObserver alloc] initForKeyPath:@"status"
                                  withCallback:status_known_cb]);

    [player_ addObserver:status_observer_
              forKeyPath:[status_observer_ keyPath]
                 options:0
                 context:nil];
  }
}

void AVFMediaDecoder::PlayerStatusKnown(
    const ResultCB& initialize_cb,
    base::scoped_nsobject<id> /* status */) {
  DVLOG(1) << "Player status: " << [player_ status];
  DCHECK(thread_checker_.CalledOnValidThread());

  if ([player_ status] != AVPlayerStatusReadyToPlay) {
    DVLOG(1) << "Player status changed to not playable";
    initialize_cb.Run(false);
    return;
  }

  if (!has_video_track() && !has_audio_track()) {
    DVLOG(1) << "No tracks to play";
    initialize_cb.Run(false);
    return;
  }

  if (!CalculateBitrate()) {
    DVLOG(1) << "Bitrate unavailable";
    initialize_cb.Run(false);
    return;
  }

  // AVPlayer is ready to play.

  duration_ =
      media::CMTimeToTimeDelta([[[player_ currentItem] asset] duration]);

  const PlayerObserverCallback rate_cb = media::BindToCurrentLoop(base::Bind(
      &AVFMediaDecoder::PlayerRateChanged, weak_ptr_factory_.GetWeakPtr()));
  rate_observer_.reset(
      [[PlayerObserver alloc] initForKeyPath:@"rate" withCallback:rate_cb]);

  [player_ addObserver:rate_observer_
            forKeyPath:[rate_observer_ keyPath]
               options:NSKeyValueObservingOptionNew
               context:nil];

  const base::Closure finish_cb =
      media::BindToCurrentLoop(base::Bind(&AVFMediaDecoder::PlayerPlayedToEnd,
                                          weak_ptr_factory_.GetWeakPtr(),
                                          "notification"));
  played_to_end_observer_.reset(
      [[PlayerNotificationObserver alloc] initWithCallback:finish_cb]);

  [[NSNotificationCenter defaultCenter]
      addObserver:played_to_end_observer_
         selector:@selector(observe:)
             name:AVFoundationGlue::AVPlayerItemDidPlayToEndTimeNotification()
           object:[player_ currentItem]];
  [[NSNotificationCenter defaultCenter]
      addObserver:played_to_end_observer_
         selector:@selector(observe:)
             name:AVFoundationGlue::AVPlayerItemFailedToPlayToEndTimeNotification()
           object:[player_ currentItem]];

  const PlayerObserverCallback time_ranges_changed_cb =
      media::BindToCurrentLoop(
          base::Bind(&AVFMediaDecoder::PlayerItemTimeRangesChanged,
                     weak_ptr_factory_.GetWeakPtr()));
  player_item_loaded_times_observer_.reset(
      [[PlayerObserver alloc] initForKeyPath:@"loadedTimeRanges"
                                withCallback:time_ranges_changed_cb]);

  [[player_ currentItem]
      addObserver:player_item_loaded_times_observer_
       forKeyPath:[player_item_loaded_times_observer_ keyPath]
          options:NSKeyValueObservingOptionNew
          context:nil];

  InitializeAudioOutput(initialize_cb);
}

bool AVFMediaDecoder::CalculateBitrate() {
  DCHECK(thread_checker_.CalledOnValidThread());

  const float bitrate =
      [AudioTrack() estimatedDataRate] + [VideoTrack() estimatedDataRate];
  if (std::isnan(bitrate) ||
      !base::IsValueInRangeForNumericType<decltype(bitrate_)>(bitrate))
    return false;

  bitrate_ = bitrate;
  return true;
}

void AVFMediaDecoder::PlayerItemTimeRangesChanged(
    base::scoped_nsobject<id> new_ranges) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::scoped_nsobject<NSArray> new_ranges_value(
      [base::mac::ObjCCastStrict<NSArray>(new_ranges.get()) retain]);

  const bool likely_to_stall = IsPlayerLikelyToStallWithRanges(
      [player_ currentItem], new_ranges_value, min_loaded_range_size_);

  if (!pending_seek_task_.is_null())
    SeekIfNotLikelyToStall(likely_to_stall);
  else
    PlayIfNotLikelyToStall("has enough data", likely_to_stall);
}

void AVFMediaDecoder::PlayerRateChanged(base::scoped_nsobject<id> new_rate) {
  const int new_rate_value =
      [base::mac::ObjCCastStrict<NSNumber>(new_rate.get()) intValue];
  DVLOG(3) << __FUNCTION__ << ": " << new_rate_value;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (new_rate_value > 0) {
    DVLOG(1) << "AVPlayer started PLAYING";
    DCHECK_EQ(playback_state_, STARTING);
    playback_state_ = PLAYING;
    return;
  }

  if (playback_state_ != STOPPING) {
    // AVPlayer stopped spontaneously.  This happens when it can't load data
    // fast enough.  Let's try to give it more chance of playing smoothly by
    // increasing the required amount of data loaded before resuming playback.
    min_loaded_range_size_ =
        std::min(min_loaded_range_size_ * 2, kMaxRequiredLoadedTimeRange);
  }

  playback_state_ = STOPPED;

  const base::TimeDelta current_time =
      media::CMTimeToTimeDelta([player_ currentTime]);
  DVLOG(1) << "AVPlayer was PAUSED @" << current_time.InMicroseconds();

  if (last_audio_timestamp_ >= duration_) {
    PlayerPlayedToEnd("pause");
  } else if (!seeking_ && last_audio_timestamp_ != media::kNoTimestamp()) {
    TRACE_EVENT_ASYNC_BEGIN0("IPC_MEDIA", "AVFMediaDecoder::Auto-seek", this);

    DCHECK(has_audio_track());

    // AVFMediaDecoder receives audio ahead of [player_ currentTime].  In order
    // to preserve audio signal continuity when |player_| resumes playing, we
    // have to seek forwards to the last audio timestamp we got, see
    // |AutoSeekTask()|.
    ScheduleSeekTask(base::Bind(&AVFMediaDecoder::AutoSeekTask,
                                weak_ptr_factory_.GetWeakPtr()));
  }

  if (!play_on_pause_done_task_.is_null()) {
    play_on_pause_done_task_.Run();
    play_on_pause_done_task_.Reset();
  }
}

void AVFMediaDecoder::PlayWhenReady(base::StringPiece reason) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!client_->HasAvailableCapacity())
    return;

  if (playback_state_ == PLAYING || playback_state_ == STARTING) {
    DVLOG(7) << "Giving up on playing AVPlayer when '" << reason
             << ", because already playing/starting to play";
    return;
  }

  if (stream_has_ended_) {
    DVLOG(3) << "Giving up on playing AVPlayer when '" << reason
             << "', because the stream has ended";
    return;
  }

  base::PostTaskAndReplyWithResult(
      background_runner().get(),
      FROM_HERE,
      base::Bind(&IsPlayerLikelyToStall,
                 background_tasks_canceled_,
                 [player_ currentItem],
                 min_loaded_range_size_),
      base::Bind(&AVFMediaDecoder::PlayIfNotLikelyToStall,
                 weak_ptr_factory_.GetWeakPtr(),
                 reason));
}

void AVFMediaDecoder::PlayIfNotLikelyToStall(base::StringPiece reason,
                                             bool likely_to_stall) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!client_->HasAvailableCapacity())
    return;

  if (playback_state_ == PLAYING || playback_state_ == STARTING) {
    DVLOG(7) << "Giving up on playing AVPlayer when '" << reason
             << "', because already playing/starting to play";
    return;
  }

  if (stream_has_ended_) {
    DVLOG(3) << " Giving up on playing AVPlayer when '" << reason
             << "', because the stream has ended";
    return;
  }

  if (likely_to_stall) {
    DVLOG(3) << "Giving up on playing AVPlayer when '" << reason
             << "', because the player is likely to stall";
    return;
  }

  // In the following two cases the client might require a new buffer before it
  // emits another such notification, and we can't provide a new buffer until
  // we play our AVPlayer again.  Thus, instead of just ignoring this
  // notification, postpone it.
  const base::Closure play_task = base::Bind(
      &AVFMediaDecoder::PlayWhenReady, weak_ptr_factory_.GetWeakPtr(), reason);
  if (seeking_) {
    DVLOG(3) << "Temporarily ignoring '" << reason
             << "' notification while seeking";
    play_on_seek_done_task_ = play_task;
    return;
  }
  if (playback_state_ == STOPPING) {
    DVLOG(3) << "Temporarily ignoring '" << reason
             << "' notification while pausing";
    play_on_pause_done_task_ = play_task;
    return;
  }

  DVLOG(1) << "PLAYING AVPlayer because " << reason;
  DCHECK_EQ(playback_state_, STOPPED);
  playback_state_ = STARTING;
  [player_ play];
}

void AVFMediaDecoder::SeekIfNotLikelyToStall(bool likely_to_stall) {
  DVLOG(1) << __FUNCTION__ << '(' << likely_to_stall << ')';
  DCHECK(thread_checker_.CalledOnValidThread());

  if (likely_to_stall)
    return;

  // If |PlayerItemTimeRangesChanged()| is called while seek task is scheduled
  // but not finished yet, then pending_seek_task_ might be consumed already
  // and might be null here.
  if (!pending_seek_task_.is_null()) {
    pending_seek_task_.Run();
    pending_seek_task_.Reset();
  }
}

void AVFMediaDecoder::ScheduleSeekTask(const base::Closure& seek_task) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!seeking_);
  DCHECK(pending_seek_task_.is_null());

  // We must not request a seek if AVPlayer's internal buffers are drained.
  // Sometimes, AVPlayer never finishes the seek if the seek is started in such
  // a state.

  seeking_ = true;
  pending_seek_task_ = seek_task;

  base::PostTaskAndReplyWithResult(
      background_runner().get(),
      FROM_HERE,
      base::Bind(&IsPlayerLikelyToStall,
                 background_tasks_canceled_,
                 [player_ currentItem],
                 min_loaded_range_size_),
      base::Bind(&AVFMediaDecoder::SeekIfNotLikelyToStall,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AVFMediaDecoder::SeekTask(const base::TimeDelta& time,
                               const ResultCB& result_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());

  const scoped_refptr<base::TaskRunner> runner = background_runner();

  const base::Closure set_audio_mix_task =
      has_audio_track()
          // The audio mix will have to be reset to preserve A/V
          // synchronization.
          ? base::Bind(&SetAudioMix,
                       background_tasks_canceled_,
                       [player_ currentItem],
                       audio_tap_->GetAudioMix())
          : base::Closure();

  const base::Callback<void(bool)> seek_done_cb = media::BindToCurrentLoop(
      base::Bind(&AVFMediaDecoder::SeekDone,
                 weak_ptr_factory_.GetWeakPtr(),
                 result_cb));

  [player_ seekToTime:media::TimeDeltaToCMTime(time)
        toleranceBefore:media::CoreMediaGlueCMTimeToCMTime(
                            CoreMediaGlue::kCMTimeZero)
         toleranceAfter:media::CoreMediaGlueCMTimeToCMTime(
                            CoreMediaGlue::kCMTimeZero)
      completionHandler:^(BOOL finished) {
        DVLOG(1) << (finished ? "Seek DONE" : "Seek was interrupted/rejected");
        if (finished && !set_audio_mix_task.is_null()) {
          runner->PostTaskAndReply(FROM_HERE, set_audio_mix_task,
                                   base::Bind(seek_done_cb, true));
        } else {
          seek_done_cb.Run(finished);
        }
      }];
}

void AVFMediaDecoder::AutoSeekTask() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(playback_state_, STOPPED);

  DVLOG(1) << "Auto-seeking to " << last_audio_timestamp_.InMicroseconds();

  const scoped_refptr<base::TaskRunner> runner = background_runner();

  // The audio mix will have to be reset to prevent a "phase shift" in the
  // series of audio buffers.  The rendering end of the Chrome pipeline
  // treats such shifts as errors.
  const base::Closure set_audio_mix_task =
      base::Bind(&SetAudioMix,
                 background_tasks_canceled_,
                 [player_ currentItem],
                 audio_tap_->GetAudioMix());

  const base::Closure auto_seek_done_cb = media::BindToCurrentLoop(base::Bind(
      &AVFMediaDecoder::AutoSeekDone, weak_ptr_factory_.GetWeakPtr()));

  [player_ seekToTime:media::TimeDeltaToCMTime(last_audio_timestamp_)
        toleranceBefore:media::CoreMediaGlueCMTimeToCMTime(
                            CoreMediaGlue::kCMTimeZero)
         toleranceAfter:media::CoreMediaGlueCMTimeToCMTime(
                            CoreMediaGlue::kCMTimeZero)
      completionHandler:^(BOOL finished) {
        DVLOG(1) << (finished ? "Auto-seek DONE"
                              : "Auto-seek was interrupted/rejected");
        // Need to set a new audio mix whether the auto-seek was successful
        // or not.  We will most likely continue decoding.
        runner->PostTaskAndReply(FROM_HERE, set_audio_mix_task,
                                 auto_seek_done_cb);
      }];
}

void AVFMediaDecoder::InitializeAudioOutput(const ResultCB& initialize_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(player_ != NULL);

  AVAssetTrack* audio_track = AudioTrack();

  // If AVPlayer detects video track but no audio track (or audio format is not
  // supported) we proceed with video initialization, to show at least
  // audio-less video.
  if (audio_track == NULL) {
    DVLOG(1) << "Playing video only";
    DCHECK(has_video_track());
    AudioFormatKnown(initialize_cb, audio_stream_format_);
    return;
  }

  // Otherwise, we create an audio processing tap and wait for it to provide
  // the audio stream format (AudioFormatKnown()).
  DCHECK(audio_track != NULL);

  // First, though, make sure AVPlayer doesn't emit any sound on its own
  // (DNA-28672).  The only sound we want is the one played by Chrome from the
  // samples obtained through the audio processing tap.
  [player_ setVolume:0.0];

  audio_tap_.reset(
      new AVFAudioTap(audio_track,
                      base::MessageLoop::current()->task_runner(),
                      base::Bind(&AVFMediaDecoder::AudioFormatKnown,
                                 weak_ptr_factory_.GetWeakPtr(),
                                 initialize_cb),
                      base::Bind(&AVFMediaDecoder::AudioSamplesReady,
                                 weak_ptr_factory_.GetWeakPtr())));

  base::scoped_nsobject<AVAudioMix> audio_mix = audio_tap_->GetAudioMix();
  if (audio_mix.get() == nil) {
    DVLOG(1) << "Could not create AVAudioMix with audio processing tap";
    initialize_cb.Run(false);
    return;
  }

  [[player_ currentItem] setAudioMix:audio_mix];
}

void AVFMediaDecoder::AudioFormatKnown(
    const ResultCB& initialize_cb,
    const AudioStreamBasicDescription& format) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!is_audio_format_known());
  DCHECK(player_ != NULL);

  audio_stream_format_ = format;

  // The audio output is now fully initialized, proceed to initialize the video
  // output.

  if (has_video_track()) {
    if (!InitializeVideoOutput()) {
      initialize_cb.Run(false);
      return;
    }
  }

  DVLOG(1) << "PLAYING AVPlayer";
  playback_state_ = STARTING;
  [player_ play];

  initialize_cb.Run(true);
}

void AVFMediaDecoder::AudioSamplesReady(
    const scoped_refptr<media::DataBuffer>& buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (seeking_ || stream_has_ended_) {
    DVLOG(3) << "Ignoring audio samples: " << (seeking_ ? "seeking"
                                                        : "stream has ended");
    return;
  }

  if (last_audio_timestamp_ != media::kNoTimestamp() &&
      buffer->timestamp() <= last_audio_timestamp_) {
    DVLOG(1) << "Audio buffer @" << buffer->timestamp().InMicroseconds()
             << " older than last buffer @"
             << last_audio_timestamp_.InMicroseconds() << ", dropping";
    return;
  }

  last_audio_timestamp_ = buffer->timestamp();

  client_->AudioSamplesReady(buffer);
}

bool AVFMediaDecoder::InitializeVideoOutput() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(has_video_track());

  if (VideoFrameRate() <= 0.0)
    return false;

  DCHECK_EQ([[VideoTrack() formatDescriptions] count], 1u);
  video_stream_format_ = reinterpret_cast<CMFormatDescriptionRef>(
      [[VideoTrack() formatDescriptions] objectAtIndex:0]);

  const CoreMediaGlue::CMVideoDimensions coded_size =
      CoreMediaGlue::CMVideoFormatDescriptionGetDimensions(
          video_stream_format());
  video_coded_size_ = gfx::Size(coded_size.width, coded_size.height);

  NSDictionary* output_settings = @{
    base::mac::CFToNSCast(kCVPixelBufferPixelFormatTypeKey) :
        @(kCVPixelFormatType_420YpCbCr8Planar)
  };
  video_output_.reset([[AVFoundationGlue::AVPlayerItemVideoOutputClass() alloc]
      initWithPixelBufferAttributes:output_settings]);

  [[player_ currentItem] addOutput:video_output_];

  const base::Callback<void(const CMTime&)> periodic_cb =
      media::BindToCurrentLoop(base::Bind(&AVFMediaDecoder::ReadFromVideoOutput,
                                          weak_ptr_factory_.GetWeakPtr()));

  CoreMediaGlue::CMTime interval =
      CoreMediaGlue::CMTimeMake(1, VideoFrameRate());
  id handle = [player_
      addPeriodicTimeObserverForInterval:media::CoreMediaGlueCMTimeToCMTime(
                                             interval)
                                   queue:nil
                              usingBlock:^(CMTime time) {
                                periodic_cb.Run(time);
                              }];
  DCHECK(time_observer_handle_.get() == nil);
  time_observer_handle_.reset([handle retain]);

  return true;
}

void AVFMediaDecoder::ReadFromVideoOutput(const CMTime& cm_timestamp) {
  DVLOG(3) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  if (seeking_ || playback_state_ != PLAYING || stream_has_ended_) {
    DVLOG(3) << "Not reading video output: "
             << (seeking_ ? "seeking" : playback_state_ != PLAYING
                                            ? "not playing"
                                            : "stream has ended");
    return;
  }

  const base::TimeDelta timestamp = media::CMTimeToTimeDelta(cm_timestamp);

  if (last_video_timestamp_ != media::kNoTimestamp() &&
      timestamp <= last_video_timestamp_) {
    DVLOG(1) << "Video buffer @" << timestamp.InMicroseconds()
             << " older than last buffer @"
             << last_video_timestamp_.InMicroseconds() << ", dropping";
    return;
  }

  const scoped_refptr<media::DataBuffer> buffer =
      GetVideoFrame(video_output_, cm_timestamp, video_coded_size_);
  if (buffer.get() == NULL) {
    if (timestamp >= duration_)
      PlayerPlayedToEnd("video output");
    return;
  }

  last_video_timestamp_ = buffer->timestamp();

  client_->VideoFrameReady(buffer);
}

void AVFMediaDecoder::AutoSeekDone() {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  seeking_ = false;

  RunTasksPendingSeekDone();

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "AVFMediaDecoder::Auto-seek", this);
}

void AVFMediaDecoder::SeekDone(const ResultCB& result_cb, bool finished) {
  DVLOG(1) << __FUNCTION__;
  DCHECK(thread_checker_.CalledOnValidThread());

  seeking_ = false;
  stream_has_ended_ = false;

  last_audio_timestamp_ = media::kNoTimestamp();
  last_video_timestamp_ = media::kNoTimestamp();

  result_cb.Run(finished);

  RunTasksPendingSeekDone();

  TRACE_EVENT_ASYNC_END0("IPC_MEDIA", "AVFMediaDecoder::Seek", this);
}

void AVFMediaDecoder::RunTasksPendingSeekDone() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!seek_on_seek_done_task_.is_null()) {
    seek_on_seek_done_task_.Run();
    seek_on_seek_done_task_.Reset();
  }

  if (!play_on_seek_done_task_.is_null()) {
    play_on_seek_done_task_.Run();
    play_on_seek_done_task_.Reset();
  }
}

void AVFMediaDecoder::PlayerPlayedToEnd(base::StringPiece source) {
  DVLOG(1) << __FUNCTION__ << '(' << source << ')';
  DCHECK(thread_checker_.CalledOnValidThread());

  if (stream_has_ended_)
    return;

  stream_has_ended_ = true;

  // Typically, we receive the |PlayerRateChanged()| callback for a paused
  // AVPlayer right before we receive this callback.  Let's cancel the seek we
  // may have started there.
  [[player_ currentItem] cancelPendingSeeks];

  client_->StreamHasEnded();
}

AVAssetTrack* AVFMediaDecoder::AssetTrackForType(
    NSString* track_type_name) const {
  NSArray* tracks =
      [[[player_ currentItem] asset] tracksWithMediaType:track_type_name];
  return [tracks count] > 0u ? [tracks objectAtIndex:0] : nil;
}

AVAssetTrack* AVFMediaDecoder::VideoTrack() const {
  return AssetTrackForType(AVFoundationGlue::AVMediaTypeVideo());
}

AVAssetTrack* AVFMediaDecoder::AudioTrack() const {
  return AssetTrackForType(AVFoundationGlue::AVMediaTypeAudio());
}

double AVFMediaDecoder::VideoFrameRate() const {
  return [VideoTrack() nominalFrameRate];
}

scoped_refptr<base::TaskRunner> AVFMediaDecoder::background_runner() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return g_background_thread.Get().task_runner();
}

}  // namespace content
