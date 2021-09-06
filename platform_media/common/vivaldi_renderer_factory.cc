// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (c) 2018 Vivaldi Technologies AS. All rights reserved.

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer_factory.h"

namespace media {

std::unique_ptr<Renderer> RendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space) {
  return CreateRenderer(media_task_runner, worker_task_runner,
                        audio_renderer_sink, video_renderer_sink,
                        std::move(request_overlay_info_cb), target_color_space,
                        false);
}

std::unique_ptr<Renderer> RendererFactory::CreateRenderer(
    const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
    const scoped_refptr<base::TaskRunner>& worker_task_runner,
    AudioRendererSink* audio_renderer_sink,
    VideoRendererSink* video_renderer_sink,
    RequestOverlayInfoCB request_overlay_info_cb,
    const gfx::ColorSpace& target_color_space,
    bool use_platform_media_pipeline) {
  return CreateRenderer(media_task_runner, worker_task_runner,
                        audio_renderer_sink, video_renderer_sink,
                        std::move(request_overlay_info_cb), target_color_space);
}

}
