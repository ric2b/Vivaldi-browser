// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#ifndef RENDERER_VIVALDI_THROTTLE_GUARD_H_
#define RENDERER_VIVALDI_THROTTLE_GUARD_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"

namespace safe_browsing {
  class RendererURLLoaderThrottle;
}

namespace blink {
  class URLLoaderThrottle;
}

namespace vivaldi {

class ThrottleGuard {
 public:
  ThrottleGuard();
  virtual ~ThrottleGuard();
  void AddObserver(blink::URLLoaderThrottle *);
  void RemoveObserver(blink::URLLoaderThrottle *);
  base::WeakPtr<ThrottleGuard> GetWeakPtr();

 private:
  base::WeakPtrFactory<ThrottleGuard> weak_factory_{this};
  base::ObserverList<blink::URLLoaderThrottle>::Unchecked observers_;
};

} // namespace vivaldi
#endif
