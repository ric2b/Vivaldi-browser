// Copyright (c) 2021 Vivaldi Technologies AS. All rights reserved.

#ifndef PLATFORM_MEDIA_IPC_DEMUXER_RENDERER_INIT_FOR_RENDER_THREAD_H_
#define PLATFORM_MEDIA_IPC_DEMUXER_RENDERER_INIT_FOR_RENDER_THREAD_H_

namespace content {
class RenderThreadImpl;
}

namespace platform_media {

void InitForRenderThread(content::RenderThreadImpl& t);

}
#endif  // PLATFORM_MEDIA_IPC_DEMUXER_RENDERER_INIT_FOR_RENDER_THREAD_H_
