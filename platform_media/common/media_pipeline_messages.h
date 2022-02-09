// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.
// Copyright (C) 2014 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/common/feature_toggles.h"

#include "base/memory/unsafe_shared_memory_region.h"
#include "base/time/time.h"
#include "ipc/ipc_message_macros.h"
#include "ipc/ipc_message_start.h"
#include "platform_media/common/platform_media_pipeline_types.h"
#include "media/base/ipc/media_param_traits.h"

#define IPC_MESSAGE_START MediaPipelineMsgStart

IPC_ENUM_TRAITS_MAX_VALUE(media::PlatformStreamType,
                          media::kPlatformStreamTypeCount - 1)

IPC_ENUM_TRAITS_MAX_VALUE(media::MediaDataStatus,
                          media::kMediaDataStatusCount - 1)

IPC_STRUCT_TRAITS_BEGIN(media::PlatformMediaTimeInfo)
  IPC_STRUCT_TRAITS_MEMBER(duration)
  IPC_STRUCT_TRAITS_MEMBER(start_time)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::PlatformAudioConfig)
  IPC_STRUCT_TRAITS_MEMBER(format)
  IPC_STRUCT_TRAITS_MEMBER(channel_count)
  IPC_STRUCT_TRAITS_MEMBER(samples_per_second)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::PlatformVideoConfig::Plane)
  IPC_STRUCT_TRAITS_MEMBER(stride)
  IPC_STRUCT_TRAITS_MEMBER(offset)
  IPC_STRUCT_TRAITS_MEMBER(size)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::PlatformVideoConfig)
  IPC_STRUCT_TRAITS_MEMBER(coded_size)
  IPC_STRUCT_TRAITS_MEMBER(visible_rect)
  IPC_STRUCT_TRAITS_MEMBER(natural_size)
  IPC_STRUCT_TRAITS_MEMBER(planes)
  IPC_STRUCT_TRAITS_MEMBER(rotation)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_BEGIN(MediaPipelineMsg_DecodedDataReady_Params)
  IPC_STRUCT_MEMBER(media::PlatformStreamType, stream_type)
  IPC_STRUCT_MEMBER(media::MediaDataStatus, status)
  IPC_STRUCT_MEMBER(int, size)
  IPC_STRUCT_MEMBER(base::TimeDelta, timestamp)
  IPC_STRUCT_MEMBER(base::TimeDelta, duration)
IPC_STRUCT_END()

IPC_MESSAGE_ROUTED3(MediaPipelineMsg_ReadRawData,
                    int64_t /* tag */,
                    int64_t /* position */,
                    int /* size */)

IPC_MESSAGE_ROUTED2(MediaPipelineMsg_RawDataReady,
                    int64_t /* tag */,
                    int /* size (DataSource::kReadError on error, 0 on EOS) */)

IPC_MESSAGE_ROUTED1(MediaPipelineMsg_ReadDecodedData,
                    media::PlatformStreamType /* stream_type */)

IPC_MESSAGE_ROUTED2(MediaPipelineMsg_DecodedDataReady,
                    MediaPipelineMsg_DecodedDataReady_Params /* data */,
                    base::ReadOnlySharedMemoryRegion
                    /* new region or not valid to reuse cached one*/)

IPC_MESSAGE_ROUTED5(MediaPipelineMsg_Initialized,
                    bool /* status */,
                    int /* bitrate */,
                    media::PlatformMediaTimeInfo /* time_info */,
                    media::PlatformAudioConfig /* audio_config */,
                    media::PlatformVideoConfig /* video_config */)

IPC_MESSAGE_ROUTED1(MediaPipelineMsg_AudioConfigChanged,
                    media::PlatformAudioConfig /* audio_config */)

IPC_MESSAGE_ROUTED1(MediaPipelineMsg_VideoConfigChanged,
                    media::PlatformVideoConfig /* video_config */)

IPC_MESSAGE_ROUTED0(MediaPipelineMsg_WillSeek)

IPC_MESSAGE_ROUTED1(MediaPipelineMsg_Seek,
                    base::TimeDelta /* time */)

IPC_MESSAGE_ROUTED1(MediaPipelineMsg_Sought,
                    bool /* success */)
