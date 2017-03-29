// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_
#define MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "media/base/media_export.h"
#include "media/filters/context_3d.h"

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
#include "media/filters/ipc_media_pipeline_host.h"
#endif

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebContentDecryptionModule;
class WebMediaPlayerClient;
}

namespace media {

class AudioRendererSink;
class MediaLog;
class MediaPermission;

// Holds parameters for constructing WebMediaPlayerImpl without having
// to plumb arguments through various abstraction layers.
class MEDIA_EXPORT WebMediaPlayerParams {
 public:
  typedef base::Callback<void(const base::Closure&)> DeferLoadCB;
  typedef base::Callback<Context3D()> Context3DCB;

  // |defer_load_cb|, |audio_renderer_sink|, |compositor_task_runner|, and
  // |context_3d_cb| may be null.
  WebMediaPlayerParams(
      const DeferLoadCB& defer_load_cb,
      const scoped_refptr<AudioRendererSink>& audio_renderer_sink,
      const scoped_refptr<MediaLog>& media_log,
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
      const IPCMediaPipelineHost::Creator& ipc_media_pipeline_host_creator,
#endif
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner,
      const Context3DCB& context_3d,
      MediaPermission* media_permission,
      blink::WebContentDecryptionModule* initial_cdm);

  ~WebMediaPlayerParams();

  DeferLoadCB defer_load_cb() const { return defer_load_cb_; }

  const scoped_refptr<AudioRendererSink>& audio_renderer_sink() const {
    return audio_renderer_sink_;
  }

  const scoped_refptr<MediaLog>& media_log() const {
    return media_log_;
  }

#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  media::IPCMediaPipelineHost::Creator ipc_media_pipeline_host_creator() const {
    return ipc_media_pipeline_host_creator_;
  }
#endif

  const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner() const {
    return media_task_runner_;
  }

  const scoped_refptr<base::SingleThreadTaskRunner>& compositor_task_runner()
      const {
    return compositor_task_runner_;
  }

  Context3DCB context_3d_cb() const { return context_3d_cb_; }

  MediaPermission* media_permission() const { return media_permission_; }

  blink::WebContentDecryptionModule* initial_cdm() const {
    return initial_cdm_;
  }


 private:
  DeferLoadCB defer_load_cb_;
  scoped_refptr<AudioRendererSink> audio_renderer_sink_;
  scoped_refptr<MediaLog> media_log_;
#if defined(USE_SYSTEM_PROPRIETARY_CODECS)
  IPCMediaPipelineHost::Creator ipc_media_pipeline_host_creator_;
#endif
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;
  Context3DCB context_3d_cb_;

  // TODO(xhwang): Remove after prefixed EME API support is removed.
  MediaPermission* media_permission_;
  blink::WebContentDecryptionModule* initial_cdm_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebMediaPlayerParams);
};

}  // namespace media

#endif  // MEDIA_BLINK_WEBMEDIAPLAYER_PARAMS_H_
