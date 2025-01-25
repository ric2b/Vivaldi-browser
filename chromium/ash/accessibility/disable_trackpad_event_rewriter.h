// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_DISABLE_TRACKPAD_EVENT_REWRITER_H_
#define ASH_ACCESSIBILITY_DISABLE_TRACKPAD_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// EventRewriter that cancels events from the built-in trackpad.
class ASH_EXPORT DisableTrackpadEventRewriter : public ui::EventRewriter {
 public:
  DisableTrackpadEventRewriter();
  DisableTrackpadEventRewriter(const DisableTrackpadEventRewriter&) = delete;
  DisableTrackpadEventRewriter& operator=(const DisableTrackpadEventRewriter&) =
      delete;
  ~DisableTrackpadEventRewriter() override;

  void SetEnabled(bool enabled);
  bool IsEnabled();

 private:
  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  bool enabled_ = false;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_DISABLE_TRACKPAD_EVENT_REWRITER_H_
