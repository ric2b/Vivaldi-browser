// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RESOURCE_CONTROLLER_H_
#define CONTENT_PUBLIC_BROWSER_RESOURCE_CONTROLLER_H_

#include "content/common/content_export.h"

namespace content {

// Used to either resume a deferred resource load or cancel a resource load at
// any time.  CancelAndIgnore is a variation of Cancel that also causes the
// requester of the resource to act like the request was never made.  By
// default, load is cancelled with ERR_ABORTED code. CancelWithError can be used
// to cancel load with any other error code.
class CONTENT_EXPORT ResourceController {
 public:
  virtual void Cancel() = 0;
  virtual void CancelAndIgnore() = 0;
  virtual void CancelWithError(int error_code) = 0;
  /* NOTE(yngve): Risk of infinite loop, should only be a problem for us,
   * if we add new subclasses, chromium will still use the abstract definition.
   */
  virtual void Resume();
  virtual void Resume(bool open_when_done,
                      bool ask_for_target=false);
 protected:
  virtual ~ResourceController() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RESOURCE_CONTROLLER_H_
