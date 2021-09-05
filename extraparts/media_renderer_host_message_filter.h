// Copyright (c) 2020 Vivaldi Technologies AS. All rights reserved

#ifndef EXTRAPARTS_MEDIA_RENDERER_HOST_MESSAGE_FILTER_H_
#define EXTRAPARTS_MEDIA_RENDERER_HOST_MESSAGE_FILTER_H_

#include "content/public/browser/browser_message_filter.h"
#include "renderer/vivaldi_render_messages.h"

class Profile;

namespace vivaldi {

// This class filters out incoming video element IPC messages from the renderer
// process on the IPC thread.
class MediaRendererHostMessageFilter : public content::BrowserMessageFilter {
 public:
  // Create the filter.
  MediaRendererHostMessageFilter(int render_process_id, Profile* profile);

  // BrowserMessageFilter methods:
  bool OnMessageReceived(const IPC::Message& message) override;
  void OverrideThreadForMessage(const IPC::Message& message,
                                content::BrowserThread::ID* thread) override;

  int render_process_id() const { return render_process_id_; }


 protected:
  void OnMediaElementAdded();

  ~MediaRendererHostMessageFilter() override;

 private:
  int render_process_id_ = 0;
  Profile* profile_ = nullptr;

  base::WeakPtrFactory<MediaRendererHostMessageFilter> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(MediaRendererHostMessageFilter);
};

}  // namespace vivaldi

#endif  // EXTRAPARTS_MEDIA_RENDERER_HOST_MESSAGE_FILTER_H_
