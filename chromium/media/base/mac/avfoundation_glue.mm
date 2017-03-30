// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/base/mac/avfoundation_glue.h"

#include <dlfcn.h>
#include <stddef.h>

#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/mac/mac_util.h"
#include "base/macros.h"
#include "base/metrics/histogram.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"

namespace {

// Used for logging capture API usage. Classes are a partition. Elements in this
// enum should not be deleted or rearranged; the only permitted operation is to
// add new elements before CAPTURE_API_MAX, that must be equal to the last item.
enum CaptureApi {
  CAPTURE_API_QTKIT_DUE_TO_OS_PREVIOUS_TO_LION = 0,
  CAPTURE_API_QTKIT_FORCED_BY_FLAG = 1,
  CAPTURE_API_QTKIT_DUE_TO_NO_FLAG = 2,
  CAPTURE_API_QTKIT_DUE_TO_AVFOUNDATION_LOAD_ERROR = 3,
  CAPTURE_API_AVFOUNDATION_LOADED_OK = 4,
  CAPTURE_API_MAX = CAPTURE_API_AVFOUNDATION_LOADED_OK
};

void LogCaptureApi(CaptureApi api) {
  UMA_HISTOGRAM_ENUMERATION("Media.VideoCaptureApi.Mac",
                            api,
                            CAPTURE_API_MAX + 1);
}

// This class is used to retrieve AVFoundation NSBundle and library handle. It
// must be used as a LazyInstance so that it is initialised once and in a
// thread-safe way. Normally no work is done in constructors: LazyInstance is
// an exception.
class AVFoundationInternal {
 public:
  AVFoundationInternal() {
    bundle_ = [NSBundle
        bundleWithPath:@"/System/Library/Frameworks/AVFoundation.framework"];

    const char* path = [[bundle_ executablePath] fileSystemRepresentation];
    CHECK(path);
    library_handle_ = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    CHECK(library_handle_) << dlerror();

    struct {
      NSString** loaded_string;
      const char* symbol;
    } av_strings[] = {
        {&AVPlayerItemDidPlayToEndTimeNotification_,
         "AVPlayerItemDidPlayToEndTimeNotification"},
        {&AVPlayerItemFailedToPlayToEndTimeNotification_,
         "AVPlayerItemFailedToPlayToEndTimeNotification"},
        {&AVCaptureDeviceWasConnectedNotification_,
         "AVCaptureDeviceWasConnectedNotification"},
        {&AVCaptureDeviceWasDisconnectedNotification_,
         "AVCaptureDeviceWasDisconnectedNotification"},
        {&AVMediaTypeVideo_, "AVMediaTypeVideo"},
        {&AVMediaTypeAudio_, "AVMediaTypeAudio"},
        {&AVMediaTypeMuxed_, "AVMediaTypeMuxed"},
        {&AVCaptureSessionRuntimeErrorNotification_,
         "AVCaptureSessionRuntimeErrorNotification"},
        {&AVCaptureSessionDidStopRunningNotification_,
         "AVCaptureSessionDidStopRunningNotification"},
        {&AVCaptureSessionErrorKey_, "AVCaptureSessionErrorKey"},
        {&AVFormatIDKey_, "AVFormatIDKey"},
        {&AVChannelLayoutKey_, "AVChannelLayoutKey"},
        {&AVVideoScalingModeKey_, "AVVideoScalingModeKey"},
        {&AVVideoScalingModeResizeAspectFill_,
         "AVVideoScalingModeResizeAspectFill"},
    };
    for (size_t i = 0; i < arraysize(av_strings); ++i) {
      *av_strings[i].loaded_string = *reinterpret_cast<NSString**>(
          dlsym(library_handle_, av_strings[i].symbol));
      DCHECK(*av_strings[i].loaded_string) << dlerror();
    }
  }

  NSBundle* bundle() const { return bundle_; }

  NSString* AVPlayerItemDidPlayToEndTimeNotification() const {
    return AVPlayerItemDidPlayToEndTimeNotification_;
  }
  NSString* AVPlayerItemFailedToPlayToEndTimeNotification() const {
    return AVPlayerItemFailedToPlayToEndTimeNotification_;
  }
  NSString* AVCaptureDeviceWasConnectedNotification() const {
    return AVCaptureDeviceWasConnectedNotification_;
  }
  NSString* AVCaptureDeviceWasDisconnectedNotification() const {
    return AVCaptureDeviceWasDisconnectedNotification_;
  }
  NSString* AVMediaTypeVideo() const { return AVMediaTypeVideo_; }
  NSString* AVMediaTypeAudio() const { return AVMediaTypeAudio_; }
  NSString* AVMediaTypeMuxed() const { return AVMediaTypeMuxed_; }
  NSString* AVCaptureSessionRuntimeErrorNotification() const {
    return AVCaptureSessionRuntimeErrorNotification_;
  }
  NSString* AVCaptureSessionDidStopRunningNotification() const {
    return AVCaptureSessionDidStopRunningNotification_;
  }
  NSString* AVCaptureSessionErrorKey() const {
    return AVCaptureSessionErrorKey_;
  }
  NSString* AVFormatIDKey() const { return AVFormatIDKey_; }
  NSString* AVChannelLayoutKey() const { return AVChannelLayoutKey_; }
  NSString* AVVideoScalingModeKey() const { return AVVideoScalingModeKey_; }
  NSString* AVVideoScalingModeResizeAspectFill() const {
    return AVVideoScalingModeResizeAspectFill_;
  }

 private:
  NSBundle* bundle_;
  void* library_handle_;
  // The following members are replicas of the respectives in AVFoundation.
  NSString* AVPlayerItemDidPlayToEndTimeNotification_;
  NSString* AVPlayerItemFailedToPlayToEndTimeNotification_;
  NSString* AVCaptureDeviceWasConnectedNotification_;
  NSString* AVCaptureDeviceWasDisconnectedNotification_;
  NSString* AVMediaTypeVideo_;
  NSString* AVMediaTypeAudio_;
  NSString* AVMediaTypeMuxed_;
  NSString* AVCaptureSessionRuntimeErrorNotification_;
  NSString* AVCaptureSessionDidStopRunningNotification_;
  NSString* AVCaptureSessionErrorKey_;
  NSString* AVFormatIDKey_;
  NSString* AVChannelLayoutKey_;
  NSString* AVVideoScalingModeKey_;
  NSString* AVVideoScalingModeResizeAspectFill_;

  DISALLOW_COPY_AND_ASSIGN(AVFoundationInternal);
};

// This contains the logic of checking whether AVFoundation is supported.
// It's called only once and the results are cached in a static bool.
bool LoadAVFoundationInternal() {
  // AVFoundation is only available on OS Lion and above.
  if (!base::mac::IsOSLionOrLater()) {
    LogCaptureApi(CAPTURE_API_QTKIT_DUE_TO_OS_PREVIOUS_TO_LION);
    return false;
  }

  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();
  // The force-qtkit flag takes precedence over enable-avfoundation.
  if (command_line->HasSwitch(switches::kForceQTKit)) {
    LogCaptureApi(CAPTURE_API_QTKIT_FORCED_BY_FLAG);
    return false;
  }

  if (!command_line->HasSwitch(switches::kEnableAVFoundation)) {
    LogCaptureApi(CAPTURE_API_QTKIT_DUE_TO_NO_FLAG);
    return false;
  }
  const bool ret = [AVFoundationGlue::AVFoundationBundle() load];
  LogCaptureApi(ret ? CAPTURE_API_AVFOUNDATION_LOADED_OK
                    : CAPTURE_API_QTKIT_DUE_TO_AVFOUNDATION_LOAD_ERROR);
  return ret;
}

}  // namespace

static base::LazyInstance<AVFoundationInternal>::Leaky g_avfoundation_handle =
    LAZY_INSTANCE_INITIALIZER;

enum {
  INITIALIZE_NOT_CALLED = 0,
  AVFOUNDATION_IS_SUPPORTED,
  AVFOUNDATION_NOT_SUPPORTED
} static g_avfoundation_initialization = INITIALIZE_NOT_CALLED;

void AVFoundationGlue::InitializeAVFoundation() {
  TRACE_EVENT0("video", "AVFoundationGlue::InitializeAVFoundation");
  CHECK([NSThread isMainThread]);
  if (g_avfoundation_initialization != INITIALIZE_NOT_CALLED)
    return;
  g_avfoundation_initialization = LoadAVFoundationInternal() ?
      AVFOUNDATION_IS_SUPPORTED : AVFOUNDATION_NOT_SUPPORTED;
}

bool AVFoundationGlue::IsAVFoundationSupported() {
  CHECK_NE(g_avfoundation_initialization, INITIALIZE_NOT_CALLED);
  return g_avfoundation_initialization == AVFOUNDATION_IS_SUPPORTED;
}

NSBundle const* AVFoundationGlue::AVFoundationBundle() {
  return g_avfoundation_handle.Get().bundle();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasConnectedNotification() {
  return g_avfoundation_handle.Get().AVCaptureDeviceWasConnectedNotification();
}

NSString* AVFoundationGlue::AVCaptureDeviceWasDisconnectedNotification() {
  return
      g_avfoundation_handle.Get().AVCaptureDeviceWasDisconnectedNotification();
}

NSString* AVFoundationGlue::AVMediaTypeVideo() {
  return g_avfoundation_handle.Get().AVMediaTypeVideo();
}

NSString* AVFoundationGlue::AVMediaTypeAudio() {
  return g_avfoundation_handle.Get().AVMediaTypeAudio();
}

NSString* AVFoundationGlue::AVMediaTypeMuxed() {
  return g_avfoundation_handle.Get().AVMediaTypeMuxed();
}

NSString* AVFoundationGlue::AVCaptureSessionRuntimeErrorNotification() {
  return g_avfoundation_handle.Get().AVCaptureSessionRuntimeErrorNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionDidStopRunningNotification() {
  return
      g_avfoundation_handle.Get().AVCaptureSessionDidStopRunningNotification();
}

NSString* AVFoundationGlue::AVCaptureSessionErrorKey() {
  return g_avfoundation_handle.Get().AVCaptureSessionErrorKey();
}

NSString* AVFoundationGlue::AVFormatIDKey() {
  return g_avfoundation_handle.Get().AVFormatIDKey();
}

NSString* AVFoundationGlue::AVChannelLayoutKey() {
  return g_avfoundation_handle.Get().AVChannelLayoutKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeKey() {
  return g_avfoundation_handle.Get().AVVideoScalingModeKey();
}

NSString* AVFoundationGlue::AVVideoScalingModeResizeAspectFill() {
  return g_avfoundation_handle.Get().AVVideoScalingModeResizeAspectFill();
}

Class AVFoundationGlue::AVCaptureSessionClass() {
  return [AVFoundationBundle() classNamed:@"AVCaptureSession"];
}

Class AVFoundationGlue::AVCaptureVideoDataOutputClass() {
  return [AVFoundationBundle() classNamed:@"AVCaptureVideoDataOutput"];
}

Class AVFoundationGlue::AVAssetClass() {
  return [AVFoundationBundle() classNamed:@"AVAsset"];
}

Class AVFoundationGlue::AVAssetReaderClass() {
  return [AVFoundationBundle() classNamed:@"AVAssetReader"];
}

Class AVFoundationGlue::AVAssetReaderTrackOutputClass() {
  return [AVFoundationBundle() classNamed:@"AVAssetReaderTrackOutput"];
}

Class AVFoundationGlue::AVAssetResourceLoaderClass() {
  return [AVFoundationBundle() classNamed:@"AVAssetResourceLoader"];
}

Class AVFoundationGlue::AVAssetResourceLoadingContentInformationRequestClass() {
  return [AVFoundationBundle()
      classNamed:@"AVAssetResourceLoadingContentInformationRequest"];
}

Class AVFoundationGlue::AVAssetResourceLoadingDataRequestClass() {
  return [AVFoundationBundle() classNamed:@"AVAssetResourceLoadingDataRequest"];
}

Class AVFoundationGlue::AVAssetResourceLoadingRequestClass() {
  return [AVFoundationBundle() classNamed:@"AVAssetResourceLoadingRequest"];
}

Class AVFoundationGlue::AVMutableAudioMixClass() {
  return [AVFoundationBundle() classNamed:@"AVMutableAudioMix"];
}

Class AVFoundationGlue::AVPlayerClass() {
  return [AVFoundationBundle() classNamed:@"AVPlayer"];
}

Class AVFoundationGlue::AVPlayerItemClass() {
  return [AVFoundationBundle() classNamed:@"AVPlayerItem"];
}

Class AVFoundationGlue::AVPlayerItemVideoOutputClass() {
  return [AVFoundationBundle() classNamed:@"AVPlayerItemVideoOutput"];
}

Class AVFoundationGlue::AVMutableAudioMixInputParametersClass() {
  return [AVFoundationBundle() classNamed:@"AVMutableAudioMixInputParameters"];
}

NSString* AVFoundationGlue::AVPlayerItemDidPlayToEndTimeNotification() {
  return g_avfoundation_handle.Get().AVPlayerItemDidPlayToEndTimeNotification();
}

NSString* AVFoundationGlue::AVPlayerItemFailedToPlayToEndTimeNotification() {
  return g_avfoundation_handle.Get()
      .AVPlayerItemFailedToPlayToEndTimeNotification();
}

@implementation AVCaptureDeviceGlue

+ (NSArray*)devices {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  if ([avcClass respondsToSelector:@selector(devices)]) {
    return [avcClass performSelector:@selector(devices)];
  }
  return nil;
}

+ (CrAVCaptureDevice*)deviceWithUniqueID:(NSString*)deviceUniqueID {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  return [avcClass performSelector:@selector(deviceWithUniqueID:)
                        withObject:deviceUniqueID];
}

@end  // @implementation AVCaptureDeviceGlue

@implementation AVCaptureDeviceInputGlue

+ (CrAVCaptureDeviceInput*)deviceInputWithDevice:(CrAVCaptureDevice*)device
                                           error:(NSError**)outError {
  return [[AVFoundationGlue::AVFoundationBundle()
      classNamed:@"AVCaptureDeviceInput"] deviceInputWithDevice:device
                                                          error:outError];
}

@end  // @implementation AVCaptureDeviceInputGlue

@implementation CrAVAssetResourceLoader

- (void) setDelegate:(id<CrAVAssetResourceLoaderDelegate>)delegate
               queue:(dispatch_queue_t)delegateQueue {
  SEL selector =
      @selector(setDelegate:CrAVAssetResourceLoaderDelegate:dispatch_queue_t:);
  NSInvocation* inv = [NSInvocation invocationWithMethodSignature:
      [AVFoundationGlue::AVAssetResourceLoaderClass()
          methodSignatureForSelector:selector]];
  [inv setSelector:selector];
  [inv setTarget:AVFoundationGlue::AVAssetResourceLoaderClass()];
  [inv setArgument:&delegate atIndex:2];
  [inv setArgument:&delegateQueue atIndex:3];
  [inv invoke];
}

@end

@implementation CrAVAssetResourceLoadingDataRequest

- (void) respondWithData:(NSData*)data {
  [AVFoundationGlue::AVAssetResourceLoadingDataRequestClass()
      performSelector:@selector(respondWithData) withObject:data];
}

- (int64_t) requestedOffset {
  return (int64_t)([AVFoundationGlue::AVAssetResourceLoadingDataRequestClass()
      performSelector:@selector(requestedOffset)]);
}

- (NSInteger) requestedLength {
  return (NSInteger)[AVFoundationGlue::AVAssetResourceLoadingDataRequestClass()
                     performSelector : @selector(requestedLength)];
}

- (int64_t) currentOffset {
  return (int64_t)([AVFoundationGlue::AVAssetResourceLoadingDataRequestClass()
      performSelector:@selector(currentOffset)]);
}

@end

@implementation CrAVAssetResourceLoadingRequest : NSObject

- (CrAVAssetResourceLoadingDataRequest*) dataRequest {
  return (CrAVAssetResourceLoadingDataRequest*)
      ([AVFoundationGlue::AVAssetResourceLoadingRequestClass()
          performSelector:@selector(dataRequest)]);
}

- (void) finishLoading {
  [AVFoundationGlue::AVAssetResourceLoadingRequestClass()
      performSelector:@selector(finishLoading)];
}

- (void) finishLoadingWithError:(NSError*)error {
  [AVFoundationGlue::AVAssetResourceLoadingRequestClass()
      performSelector:@selector(finishLoading) withObject:error];
}

-(bool) isFinished {
  return (bool)([AVFoundationGlue::AVAssetResourceLoadingRequestClass()
      performSelector:@selector(isFinished)]);
}

- (NSURLRequest*) request {
  return (NSURLRequest*)([AVFoundationGlue::AVAssetResourceLoadingRequestClass()
      performSelector:@selector(request)]);
}

- (CrAVAssetResourceLoadingContentInformationRequest*)
    contentInformationRequest {
  return (CrAVAssetResourceLoadingContentInformationRequest*)
      ([AVFoundationGlue::AVAssetResourceLoadingRequestClass()
          performSelector:@selector(contentInformationRequest)]);
}

@end

@implementation CrAVAssetResourceLoadingContentInformationRequest

- (NSString*) contentType {
  return (NSString*)
      [AVFoundationGlue::AVAssetResourceLoadingContentInformationRequestClass()
          performSelector:@selector(contentType)];
}

- (void) setContentType:(NSString*)type {
  [AVFoundationGlue::AVAssetResourceLoadingContentInformationRequestClass()
      performSelector:@selector(setContentType) withObject:type];
}

- (int64_t) contentLength {
  return (int64_t)
      [AVFoundationGlue::AVAssetResourceLoadingContentInformationRequestClass()
          performSelector:@selector(contentLength)];
}

- (void) setContentLength:(int64_t)length {
  NSNumber* object = [NSNumber numberWithLong:length];
  [AVFoundationGlue::AVAssetResourceLoadingContentInformationRequestClass()
      performSelector:@selector(setContentLength) withObject:object];
}

- (void) setByteRangeAccessSupported:(bool)supported {
  [AVFoundationGlue::AVAssetResourceLoadingContentInformationRequestClass()
      performSelector:@selector(setByteRangeAccessSupported)
           withObject:[NSNumber numberWithBool:supported]];
}

@end

@implementation CrAVMutableAudioMixInputParameters

- (void) setAudioTapProcessor:(MediaToolboxGlue::MTAudioProcessingTapRef)tap {
  [AVFoundationGlue::AVMutableAudioMixInputParametersClass()
      performSelector:@selector(setAudioTapProcessor)
           withObject:[NSValue valueWithPointer:tap]];
}

@end
