// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// AVFoundation API is only introduced in Mac OS X > 10.6, and there is only one
// build of Chromium, so the (potential) linking with AVFoundation has to happen
// in runtime. For this to be clean, an AVFoundationGlue class is defined to try
// and load these AVFoundation system libraries. If it succeeds, subsequent
// clients can use AVFoundation via the rest of the classes declared in this
// file.

#ifndef MEDIA_BASE_MAC_AVFOUNDATION_GLUE_H_
#define MEDIA_BASE_MAC_AVFOUNDATION_GLUE_H_

#if defined(__OBJC__)
#import <Foundation/Foundation.h>
#import <AVFoundation/AVFoundation.h>
#endif  // defined(__OBJC__)

#include <stdint.h>

#include "base/macros.h"
#include "media/base/mac/coremedia_glue.h"
#include "media/base/mac/mediatoolbox_glue.h"
#include "media/base/media_export.h"

class MEDIA_EXPORT AVFoundationGlue {
 public:
  // Must be called on the UI thread prior to attempting to use any other
  // AVFoundation methods.
  static void InitializeAVFoundation();

  // This method returns true if the OS version supports AVFoundation and the
  // AVFoundation bundle could be loaded correctly, or false otherwise.
  static bool IsAVFoundationSupported();

#if defined(__OBJC__)
  static NSBundle const* AVFoundationBundle();

  // Originally coming from AVCaptureDevice.h but in global namespace.
  static NSString* AVCaptureDeviceWasConnectedNotification();
  static NSString* AVCaptureDeviceWasDisconnectedNotification();

  // Originally coming from AVMediaFormat.h but in global namespace.
  static NSString* AVMediaTypeVideo();
  static NSString* AVMediaTypeAudio();
  static NSString* AVMediaTypeMuxed();

  // Originally from AVCaptureSession.h but in global namespace.
  static NSString* AVCaptureSessionRuntimeErrorNotification();
  static NSString* AVCaptureSessionDidStopRunningNotification();
  static NSString* AVCaptureSessionErrorKey();

  // Originally from AVAudioSettings.h but in global namespace.
  static NSString* AVFormatIDKey();
  static NSString* AVChannelLayoutKey();

  // Originally from AVVideoSettings.h but in global namespace.
  static NSString* AVVideoScalingModeKey();
  static NSString* AVVideoScalingModeResizeAspectFill();

  static NSString* AVPlayerItemDidPlayToEndTimeNotification();
  static NSString* AVPlayerItemFailedToPlayToEndTimeNotification();
  static Class AVAssetClass();
  static Class AVAssetReaderClass();
  static Class AVAssetReaderTrackOutputClass();
  static Class AVAssetResourceLoaderClass();
  static Class AVAssetResourceLoadingContentInformationRequestClass();
  static Class AVAssetResourceLoadingDataRequestClass();
  static Class AVAssetResourceLoadingRequestClass();

  static Class AVMutableAudioMixClass();
  static Class AVMutableAudioMixInputParametersClass();
  static Class AVPlayerClass();
  static Class AVPlayerItemClass();
  static Class AVPlayerItemVideoOutputClass();

  static Class AVCaptureSessionClass();
  static Class AVCaptureVideoDataOutputClass();
#endif  // defined(__OBJC__)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AVFoundationGlue);
};

#if defined(__OBJC__)

// Originally AVAssetResourceLoadingContentInformationRequest and coming from
// AVAssetResourceLoader.h.
@interface CrAVAssetResourceLoadingContentInformationRequest : NSObject

- (NSString*)contentType;
- (void)setContentType:(NSString*)type;
- (int64_t)contentLength;
- (void)setContentLength:(int64_t)length;
- (void)setByteRangeAccessSupported:(bool)supported;

@end

// Originally AVAssetResourceLoadingDataRequest and coming from
// AVAssetResourceLoader.h.
@interface CrAVAssetResourceLoadingDataRequest : NSObject

- (void)respondWithData:(NSData*)data;
- (int64_t)requestedOffset;
- (NSInteger)requestedLength;
- (int64_t)currentOffset;

@end

// Originally AVAssetResourceLoadingRequest and coming from
// AVAssetResourceLoader.h.
@interface CrAVAssetResourceLoadingRequest : NSObject

- (CrAVAssetResourceLoadingDataRequest*)dataRequest;
- (void)finishLoading;
- (void)finishLoadingWithError:(NSError*)error;
- (bool)isFinished;
- (NSURLRequest*)request;
- (CrAVAssetResourceLoadingContentInformationRequest*)contentInformationRequest;

@end

@class CrAVAssetResourceLoader;

// Originally AVAssetResourceLoaderDelegate and coming from
// AVAssetResourceLoader.h.
@protocol CrAVAssetResourceLoaderDelegate<NSObject>

- (BOOL)resourceLoader:(CrAVAssetResourceLoader*)resourceLoader
    shouldWaitForLoadingOfRequestedResource:
        (CrAVAssetResourceLoadingRequest*)loadingRequest;
- (void)resourceLoader:(CrAVAssetResourceLoader*)resourceLoader
    didCancelLoadingRequest:(CrAVAssetResourceLoadingRequest*)loadingRequest;

@end

// Originally AVAssetResourceLoader and coming from AVAssetResourceLoader.h.
@interface CrAVAssetResourceLoader : NSObject

- (void)setDelegate:(id<CrAVAssetResourceLoaderDelegate>)delegate
              queue:(dispatch_queue_t)delegateQueue;

@end

// Originally AVCaptureDevice and coming from AVCaptureDevice.h
MEDIA_EXPORT
@interface CrAVCaptureDevice : NSObject

- (BOOL)hasMediaType:(NSString*)mediaType;
- (NSString*)uniqueID;
- (NSString*)localizedName;
- (BOOL)isSuspended;
- (NSArray*)formats;
- (int32_t)transportType;

@end

// Originally AVCaptureDeviceFormat and coming from AVCaptureDevice.h.
MEDIA_EXPORT
@interface CrAVCaptureDeviceFormat : NSObject

- (CoreMediaGlue::CMFormatDescriptionRef)formatDescription;
- (NSArray*)videoSupportedFrameRateRanges;

@end

// Originally AVFrameRateRange and coming from AVCaptureDevice.h.
MEDIA_EXPORT
@interface CrAVFrameRateRange : NSObject

- (Float64)maxFrameRate;

@end

MEDIA_EXPORT
@interface CrAVCaptureInput : NSObject  // Originally from AVCaptureInput.h.
@end

MEDIA_EXPORT
@interface CrAVCaptureOutput : NSObject  // Originally from AVCaptureOutput.h.
@end

// Originally AVCaptureSession and coming from AVCaptureSession.h.
MEDIA_EXPORT
@interface CrAVCaptureSession : NSObject

- (void)release;
- (void)addInput:(CrAVCaptureInput*)input;
- (void)removeInput:(CrAVCaptureInput*)input;
- (void)addOutput:(CrAVCaptureOutput*)output;
- (void)removeOutput:(CrAVCaptureOutput*)output;
- (BOOL)isRunning;
- (void)startRunning;
- (void)stopRunning;

@end

// Originally AVCaptureConnection and coming from AVCaptureSession.h.
MEDIA_EXPORT
@interface CrAVCaptureConnection : NSObject

- (BOOL)isVideoMinFrameDurationSupported;
- (void)setVideoMinFrameDuration:(CoreMediaGlue::CMTime)minFrameRate;
- (BOOL)isVideoMaxFrameDurationSupported;
- (void)setVideoMaxFrameDuration:(CoreMediaGlue::CMTime)maxFrameRate;

@end

// Originally AVCaptureDeviceInput and coming from AVCaptureInput.h.
MEDIA_EXPORT
@interface CrAVCaptureDeviceInput : CrAVCaptureInput

@end

// Originally AVCaptureVideoDataOutputSampleBufferDelegate from
// AVCaptureOutput.h.
@protocol CrAVCaptureVideoDataOutputSampleBufferDelegate <NSObject>

@optional

- (void)captureOutput:(CrAVCaptureOutput*)captureOutput
didOutputSampleBuffer:(CoreMediaGlue::CMSampleBufferRef)sampleBuffer
       fromConnection:(CrAVCaptureConnection*)connection;

@end

// Originally AVCaptureVideoDataOutput and coming from AVCaptureOutput.h.
MEDIA_EXPORT
@interface CrAVCaptureVideoDataOutput : CrAVCaptureOutput

- (oneway void)release;
- (void)setSampleBufferDelegate:(id)sampleBufferDelegate
                          queue:(dispatch_queue_t)sampleBufferCallbackQueue;

- (void)setAlwaysDiscardsLateVideoFrames:(BOOL)flag;
- (void)setVideoSettings:(NSDictionary*)videoSettings;
- (NSDictionary*)videoSettings;
- (CrAVCaptureConnection*)connectionWithMediaType:(NSString*)mediaType;

@end

// Class to provide access to class methods of AVCaptureDevice.
MEDIA_EXPORT
@interface AVCaptureDeviceGlue : NSObject

+ (NSArray*)devices;

+ (CrAVCaptureDevice*)deviceWithUniqueID:(NSString*)deviceUniqueID;

@end

// Originally AVMutableAudioMixInputParameters and coming from AVAudioMix.h.
@interface CrAVMutableAudioMixInputParameters : AVAudioMixInputParameters

- (void)setAudioTapProcessor:(MediaToolboxGlue::MTAudioProcessingTapRef)tap;

@end

// Class to provide access to class methods of AVCaptureDeviceInput.
MEDIA_EXPORT
@interface AVCaptureDeviceInputGlue : NSObject

+ (CrAVCaptureDeviceInput*)deviceInputWithDevice:(CrAVCaptureDevice*)device
                                           error:(NSError**)outError;

@end

#endif  // defined(__OBJC__)

#endif  // MEDIA_BASE_MAC_AVFOUNDATION_GLUE_H_
