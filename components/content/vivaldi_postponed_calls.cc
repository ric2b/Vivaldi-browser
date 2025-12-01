// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved
//
#include "components/content/vivaldi_postponed_calls.h"
#include "base/logging.h"
namespace vivaldi {

VivaldiPostponedCalls::VivaldiPostponedCalls() {}
VivaldiPostponedCalls::~VivaldiPostponedCalls() {
  Args args;
  args.reason = Reason::DROPPED;
  callbacks_.Notify(args);
}

void VivaldiPostponedCalls::GuestAttached(content::WebContentsDelegate* guest) {
  Args args;
  args.reason = Reason::GUEST_ATTACHED;
  args.delegate = guest;
  callbacks_.Notify(args);
}

void VivaldiPostponedCalls::Add(CallFunction callback) {
  callbacks_.AddUnsafe(std::move(callback));
}

} // namespace vivaldi
