// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef COMPONENTS_CONTENT_VIVALDI_POSTPONED_CALLS_H_
#define COMPONENTS_CONTENT_VIVALDI_POSTPONED_CALLS_H_

#include <functional>
#include <vector>

#include "base/callback_list.h"

namespace content {
  class WebContentsDelegate;
}

namespace vivaldi {

class VivaldiPostponedCalls {
public:
  VivaldiPostponedCalls();
  virtual ~VivaldiPostponedCalls();

  enum struct Reason {
    GUEST_ATTACHED,
    DROPPED,
    INVALID,
  };

  struct Args {
    Reason reason = Reason::INVALID;
    content::WebContentsDelegate * delegate = nullptr;
  };

  using CallFunction = base::OnceCallback<void(const Args& args)>;
  void Add(CallFunction callback);

  // WebViewGuest as it inherites WebContentsDelegate
  void GuestAttached(content::WebContentsDelegate * guest_delegate);

private:
  base::OnceCallbackList<void(const Args& args)> callbacks_;
};

} // namespace vivaldi

#endif // COMPONENTS_CONTENT_VIVALDI_POSTPONED_CALLS_H_
