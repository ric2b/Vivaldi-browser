// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#include "platform_media/ipc_demuxer/platform_media_pipeline_types.h"

namespace media {

PlatformVideoConfig::PlatformVideoConfig() = default;
PlatformVideoConfig::PlatformVideoConfig(const PlatformVideoConfig&) = default;
PlatformVideoConfig::~PlatformVideoConfig() = default;
PlatformVideoConfig& PlatformVideoConfig::operator=(
    const PlatformVideoConfig&) = default;

}  // namespace media