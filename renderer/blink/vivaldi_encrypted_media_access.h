// Copyright (c) 2024 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_BLINK_VIVALDI_ENCRYPTED_MEDIA_ACCESS_H
#define RENDERER_BLINK_VIVALDI_ENCRYPTED_MEDIA_ACCESS_H

namespace blink {

class WebString;
class LocalDOMWindow;

}  // namespace blink

namespace vivaldi {

void NotifyEncryptedMediaAccessRequest(const blink::WebString& key_system,
                                       blink::LocalDOMWindow* window);

}  // namespace vivaldi

#endif /* RENDERER_BLINK_VIVALDI_ENCRYPTED_MEDIA_ACCESS_H */
