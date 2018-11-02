// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/InteractiveDetector.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/Histogram.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/wtf/Time.h"
#include "public/platform/WebInputEvent.h"

namespace blink {

static constexpr char kSupplementName[] = "InteractiveDetector";

InteractiveDetector* InteractiveDetector::From(Document& document) {
  InteractiveDetector* detector = static_cast<InteractiveDetector*>(
      Supplement<Document>::From(document, kSupplementName));
  if (!detector) {
    detector = new InteractiveDetector(document,
                                       new NetworkActivityChecker(&document));
    Supplement<Document>::ProvideTo(document, kSupplementName, detector);
  }
  return detector;
}

const char* InteractiveDetector::SupplementName() {
  return "InteractiveDetector";
}

InteractiveDetector::InteractiveDetector(
    Document& document,
    NetworkActivityChecker* network_activity_checker)
    : Supplement<Document>(document),
      network_activity_checker_(network_activity_checker),
      time_to_interactive_timer_(
          document.GetTaskRunner(TaskType::kUnspecedTimer),
          this,
          &InteractiveDetector::TimeToInteractiveTimerFired) {}

InteractiveDetector::~InteractiveDetector() {
  LongTaskDetector::Instance().UnregisterObserver(this);
}

void InteractiveDetector::SetNavigationStartTime(double navigation_start_time) {
  // Should not set nav start twice.
  DCHECK(page_event_times_.nav_start == 0.0);

  // Don't record TTI for OOPIFs (yet).
  // TODO(crbug.com/808086): enable this case.
  if (!GetSupplementable()->IsInMainFrame())
    return;

  LongTaskDetector::Instance().RegisterObserver(this);
  page_event_times_.nav_start = navigation_start_time;
  double initial_timer_fire_time =
      navigation_start_time + kTimeToInteractiveWindowSeconds;

  active_main_thread_quiet_window_start_ = navigation_start_time;
  active_network_quiet_window_start_ = navigation_start_time;
  StartOrPostponeCITimer(initial_timer_fire_time);
}

int InteractiveDetector::NetworkActivityChecker::GetActiveConnections() {
  DCHECK(document_);
  ResourceFetcher* fetcher = document_->Fetcher();
  return fetcher->BlockingRequestCount() + fetcher->NonblockingRequestCount();
}

int InteractiveDetector::ActiveConnections() {
  return network_activity_checker_->GetActiveConnections();
}

void InteractiveDetector::StartOrPostponeCITimer(double timer_fire_time) {
  // This function should never be called after Time To Interactive is
  // reached.
  DCHECK(interactive_time_ == 0.0);

  // We give 1ms extra padding to the timer fire time to prevent floating point
  // arithmetic pitfalls when comparing window sizes.
  timer_fire_time += 0.001;

  // Return if there is an active timer scheduled to fire later than
  // |timer_fire_time|.
  if (timer_fire_time < time_to_interactive_timer_fire_time_)
    return;

  double delay = timer_fire_time - CurrentTimeTicksInSeconds();
  time_to_interactive_timer_fire_time_ = timer_fire_time;

  if (delay <= 0.0) {
    // This argument of this function is never used and only there to fulfill
    // the API contract. nullptr should work fine.
    TimeToInteractiveTimerFired(nullptr);
  } else {
    time_to_interactive_timer_.StartOneShot(delay, FROM_HERE);
  }
}

double InteractiveDetector::GetInteractiveTime() const {
  // TODO(crbug.com/808685) Simplify FMP and TTI input invalidation.
  return page_event_times_.first_meaningful_paint_invalidated
             ? 0.0
             : interactive_time_;
}

double InteractiveDetector::GetInteractiveDetectionTime() const {
  // TODO(crbug.com/808685) Simplify FMP and TTI input invalidation.
  return page_event_times_.first_meaningful_paint_invalidated
             ? 0.0
             : interactive_detection_time_;
}

double InteractiveDetector::GetFirstInvalidatingInputTime() const {
  return page_event_times_.first_invalidating_input;
}

double InteractiveDetector::GetFirstInputDelay() const {
  return page_event_times_.first_input_delay;
}

double InteractiveDetector::GetFirstInputTimestamp() const {
  return page_event_times_.first_input_timestamp;
}

// This is called early enough in the pipeline that we don't need to worry about
// javascript dispatching untrusted input events.
void InteractiveDetector::HandleForFirstInputDelay(const WebInputEvent& event) {
  if (page_event_times_.first_input_delay != 0)
    return;

  DCHECK(event.GetType() != WebInputEvent::kTouchStart);

  // We can't report a pointerDown until the pointerUp, in case it turns into a
  // scroll.
  if (event.GetType() == WebInputEvent::kPointerDown) {
    pending_pointerdown_delay_ =
        CurrentTimeTicksInSeconds() - event.TimeStampSeconds();
    pending_pointerdown_timestamp_ =
        event.TimeStampSeconds();
    return;
  }

  bool event_is_meaningful =
      event.GetType() == WebInputEvent::kMouseDown ||
      event.GetType() == WebInputEvent::kKeyDown ||
      event.GetType() == WebInputEvent::kRawKeyDown ||
      // We need to explicitly include tap, as if there are no listeners, we
      // won't receive the pointer events.
      event.GetType() == WebInputEvent::kGestureTap ||
      event.GetType() == WebInputEvent::kPointerUp;

  if (!event_is_meaningful)
    return;

  double delay;
  double event_timestamp;
  if (event.GetType() == WebInputEvent::kPointerUp) {
    // It is possible that this pointer up doesn't match with the pointer down
    // whose delay is stored in pending_pointerdown_delay_. In this case, the
    // user gesture started by this event contained some non-scroll input, so we
    // consider it reasonable to use the delay of the initial event.
    delay = pending_pointerdown_delay_;
    event_timestamp = pending_pointerdown_timestamp_;
  } else {
    delay = CurrentTimeTicksInSeconds() - event.TimeStampSeconds();
    event_timestamp = event.TimeStampSeconds();
  }

  pending_pointerdown_delay_ = 0;
  pending_pointerdown_timestamp_ = 0;

  page_event_times_.first_input_delay = delay;
  page_event_times_.first_input_timestamp = event_timestamp;

  if (GetSupplementable()->Loader())
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
}

void InteractiveDetector::BeginNetworkQuietPeriod(double current_time) {
  // Value of 0.0 indicates there is no currently actively network quiet window.
  DCHECK(active_network_quiet_window_start_ == 0.0);
  active_network_quiet_window_start_ = current_time;

  StartOrPostponeCITimer(current_time + kTimeToInteractiveWindowSeconds);
}

void InteractiveDetector::EndNetworkQuietPeriod(double current_time) {
  DCHECK(active_network_quiet_window_start_ != 0.0);

  if (current_time - active_network_quiet_window_start_ >=
      kTimeToInteractiveWindowSeconds) {
    network_quiet_windows_.emplace_back(active_network_quiet_window_start_,
                                        current_time);
  }
  active_network_quiet_window_start_ = 0.0;
}

// The optional opt_current_time, if provided, saves us a call to
// CurrentTimeTicksInSeconds.
void InteractiveDetector::UpdateNetworkQuietState(
    double request_count,
    WTF::Optional<double> opt_current_time) {
  if (request_count <= kNetworkQuietMaximumConnections &&
      active_network_quiet_window_start_ == 0.0) {
    // Not using `value_or(CurrentTimeTicksInSeconds())` here because
    // arguments to functions are eagerly evaluated, which always call
    // CurrentTimeTicksInSeconds.
    double current_time = opt_current_time ? opt_current_time.value()
                                           : CurrentTimeTicksInSeconds();
    BeginNetworkQuietPeriod(current_time);
  } else if (request_count > kNetworkQuietMaximumConnections &&
             active_network_quiet_window_start_ != 0.0) {
    double current_time = opt_current_time ? opt_current_time.value()
                                           : CurrentTimeTicksInSeconds();
    EndNetworkQuietPeriod(current_time);
  }
}

void InteractiveDetector::OnResourceLoadBegin(
    WTF::Optional<double> load_begin_time) {
  if (!GetSupplementable())
    return;
  if (interactive_time_ != 0.0)
    return;
  // The request that is about to begin is not counted in ActiveConnections(),
  // so we add one to it.
  UpdateNetworkQuietState(ActiveConnections() + 1, load_begin_time);
}

// The optional load_finish_time, if provided, saves us a call to
// CurrentTimeTicksInSeconds.
void InteractiveDetector::OnResourceLoadEnd(
    WTF::Optional<double> load_finish_time) {
  if (!GetSupplementable())
    return;
  if (interactive_time_ != 0.0)
    return;
  UpdateNetworkQuietState(ActiveConnections(), load_finish_time);
}

void InteractiveDetector::OnLongTaskDetected(double start_time,
                                             double end_time) {
  // We should not be receiving long task notifications after Time to
  // Interactive has already been reached.
  DCHECK(interactive_time_ == 0.0);
  double quiet_window_length =
      start_time - active_main_thread_quiet_window_start_;
  if (quiet_window_length >= kTimeToInteractiveWindowSeconds) {
    main_thread_quiet_windows_.emplace_back(
        active_main_thread_quiet_window_start_, start_time);
  }
  active_main_thread_quiet_window_start_ = end_time;
  StartOrPostponeCITimer(end_time + kTimeToInteractiveWindowSeconds);
}

void InteractiveDetector::OnFirstMeaningfulPaintDetected(
    double fmp_time,
    FirstMeaningfulPaintDetector::HadUserInput user_input_before_fmp) {
  // Should not set FMP twice.
  DCHECK(page_event_times_.first_meaningful_paint == 0.0);
  page_event_times_.first_meaningful_paint = fmp_time;
  page_event_times_.first_meaningful_paint_invalidated =
      user_input_before_fmp == FirstMeaningfulPaintDetector::kHadUserInput;
  if (CurrentTimeTicksInSeconds() - fmp_time >=
      kTimeToInteractiveWindowSeconds) {
    // We may have reached TTCI already. Check right away.
    CheckTimeToInteractiveReached();
  } else {
    StartOrPostponeCITimer(page_event_times_.first_meaningful_paint +
                           kTimeToInteractiveWindowSeconds);
  }
}

void InteractiveDetector::OnDomContentLoadedEnd(double dcl_end_time) {
  // InteractiveDetector should only receive the first DCL event.
  DCHECK(page_event_times_.dom_content_loaded_end == 0.0);
  page_event_times_.dom_content_loaded_end = dcl_end_time;
  CheckTimeToInteractiveReached();
}

void InteractiveDetector::OnInvalidatingInputEvent(double timestamp_seconds) {
  if (page_event_times_.first_invalidating_input != 0.0)
    return;

  page_event_times_.first_invalidating_input = timestamp_seconds;
  if (GetSupplementable()->Loader())
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
}

void InteractiveDetector::OnFirstInputDelay(double delay) {
  if (page_event_times_.first_input_delay != 0)
    return;

  page_event_times_.first_input_delay = delay;
  if (GetSupplementable()->Loader())
    GetSupplementable()->Loader()->DidChangePerformanceTiming();
}

void InteractiveDetector::TimeToInteractiveTimerFired(TimerBase*) {
  if (!GetSupplementable() || interactive_time_ != 0.0)
    return;

  // Value of 0.0 indicates there is currently no active timer.
  time_to_interactive_timer_fire_time_ = 0.0;
  CheckTimeToInteractiveReached();
}

void InteractiveDetector::AddCurrentlyActiveQuietIntervals(
    double current_time) {
  // Network is currently quiet.
  if (active_network_quiet_window_start_ != 0.0) {
    if (current_time - active_network_quiet_window_start_ >=
        kTimeToInteractiveWindowSeconds) {
      network_quiet_windows_.emplace_back(active_network_quiet_window_start_,
                                          current_time);
    }
  }

  // Since this code executes on the main thread, we know that no task is
  // currently running on the main thread. We can therefore skip checking.
  // main_thread_quiet_window_being != 0.0.
  if (current_time - active_main_thread_quiet_window_start_ >=
      kTimeToInteractiveWindowSeconds) {
    main_thread_quiet_windows_.emplace_back(
        active_main_thread_quiet_window_start_, current_time);
  }
}

void InteractiveDetector::RemoveCurrentlyActiveQuietIntervals() {
  if (!network_quiet_windows_.empty() &&
      network_quiet_windows_.back().Low() ==
          active_network_quiet_window_start_) {
    network_quiet_windows_.pop_back();
  }

  if (!main_thread_quiet_windows_.empty() &&
      main_thread_quiet_windows_.back().Low() ==
          active_main_thread_quiet_window_start_) {
    main_thread_quiet_windows_.pop_back();
  }
}

double InteractiveDetector::FindInteractiveCandidate(double lower_bound) {
  // Main thread iterator.
  auto it_mt = main_thread_quiet_windows_.begin();
  // Network iterator.
  auto it_net = network_quiet_windows_.begin();

  while (it_mt < main_thread_quiet_windows_.end() &&
         it_net < network_quiet_windows_.end()) {
    if (it_mt->High() <= lower_bound) {
      it_mt++;
      continue;
    }
    if (it_net->High() <= lower_bound) {
      it_net++;
      continue;
    }

    // First handling the no overlap cases.
    // [ main thread interval ]
    //                                     [ network interval ]
    if (it_mt->High() <= it_net->Low()) {
      it_mt++;
      continue;
    }
    //                                     [ main thread interval ]
    // [   network interval   ]
    if (it_net->High() <= it_mt->Low()) {
      it_net++;
      continue;
    }

    // At this point we know we have a non-empty overlap after lower_bound.
    double overlap_start = std::max({it_mt->Low(), it_net->Low(), lower_bound});
    double overlap_end = std::min(it_mt->High(), it_net->High());
    double overlap_duration = overlap_end - overlap_start;
    if (overlap_duration >= kTimeToInteractiveWindowSeconds) {
      return std::max(lower_bound, it_mt->Low());
    }

    // The interval with earlier end time will not produce any more overlap, so
    // we move on from it.
    if (it_mt->High() <= it_net->High()) {
      it_mt++;
    } else {
      it_net++;
    }
  }

  // Time To Interactive candidate not found.
  return 0.0;
}

void InteractiveDetector::CheckTimeToInteractiveReached() {
  // Already detected Time to Interactive.
  if (interactive_time_ != 0.0)
    return;

  // FMP and DCL have not been detected yet.
  if (page_event_times_.first_meaningful_paint == 0.0 ||
      page_event_times_.dom_content_loaded_end == 0.0)
    return;

  const double current_time = CurrentTimeTicksInSeconds();
  if (current_time - page_event_times_.first_meaningful_paint <
      kTimeToInteractiveWindowSeconds) {
    // Too close to FMP to determine Time to Interactive.
    return;
  }

  AddCurrentlyActiveQuietIntervals(current_time);
  const double interactive_candidate =
      FindInteractiveCandidate(page_event_times_.first_meaningful_paint);
  RemoveCurrentlyActiveQuietIntervals();

  // No Interactive Candidate found.
  if (interactive_candidate == 0.0)
    return;

  interactive_time_ = std::max(
      {interactive_candidate, page_event_times_.dom_content_loaded_end});
  interactive_detection_time_ = CurrentTimeTicksInSeconds();
  OnTimeToInteractiveDetected();
}

void InteractiveDetector::OnTimeToInteractiveDetected() {
  LongTaskDetector::Instance().UnregisterObserver(this);
  main_thread_quiet_windows_.clear();
  network_quiet_windows_.clear();

  bool had_user_input_before_interactive =
      page_event_times_.first_invalidating_input != 0 &&
      page_event_times_.first_invalidating_input < interactive_time_;

  // We log the trace event even if there is user input, but annotate the event
  // with whether that happened.
  TRACE_EVENT_MARK_WITH_TIMESTAMP2(
      "loading,rail", "InteractiveTime",
      TraceEvent::ToTraceTimestamp(interactive_time_), "frame",
      GetSupplementable()->GetFrame(), "had_user_input_before_interactive",
      had_user_input_before_interactive);

  // We only send TTI to Performance Timing Observers if FMP was not invalidated
  // by input.
  // TODO(crbug.com/808685) Simplify FMP and TTI input invalidation.
  if (!page_event_times_.first_meaningful_paint_invalidated) {
    if (GetSupplementable()->Loader())
      GetSupplementable()->Loader()->DidChangePerformanceTiming();
  }
}

void InteractiveDetector::Trace(Visitor* visitor) {
  Supplement<Document>::Trace(visitor);
}

}  // namespace blink
