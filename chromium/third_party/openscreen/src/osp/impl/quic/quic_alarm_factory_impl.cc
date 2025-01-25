// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osp/impl/quic/quic_alarm_factory_impl.h"

#include <memory>
#include <utility>

#include "util/alarm.h"
#include "util/chrono_helpers.h"
#include "util/osp_logging.h"

namespace openscreen::osp {

namespace {

class QuicAlarmImpl : public quic::QuicAlarm {
 public:
  QuicAlarmImpl(const quic::QuicClock* clock,
                TaskRunner& task_runner,
                quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate> delegate)
      : quic::QuicAlarm(std::move(delegate)),
        clock_(clock),
        alarm_(std::make_unique<Alarm>(Clock::now, task_runner)) {}
  QuicAlarmImpl(const QuicAlarmImpl&) = delete;
  QuicAlarmImpl& operator=(const QuicAlarmImpl&) = delete;
  QuicAlarmImpl(QuicAlarmImpl&&) noexcept = delete;
  QuicAlarmImpl& operator=(QuicAlarmImpl&&) noexcept = delete;
  ~QuicAlarmImpl() override = default;

 protected:
  void SetImpl() override {
    OSP_CHECK(deadline().IsInitialized());
    const int64_t delay_us = (deadline() - clock_->Now()).ToMicroseconds();
    alarm_->Schedule([this] { OnAlarm(); },
                     Clock::now() + microseconds(delay_us));
  }

  void CancelImpl() override {
    OSP_CHECK(!deadline().IsInitialized());
    alarm_->Cancel();
  }

 private:
  void OnAlarm() {
    OSP_CHECK(deadline().IsInitialized());
    OSP_CHECK_LE(deadline(), clock_->Now());
    Fire();
  }

  const quic::QuicClock* clock_;
  std::unique_ptr<Alarm> alarm_;
};

}  // namespace

QuicAlarmFactoryImpl::QuicAlarmFactoryImpl(TaskRunner& task_runner,
                                           const quic::QuicClock* clock)
    : task_runner_(task_runner), clock_(clock) {}

QuicAlarmFactoryImpl::~QuicAlarmFactoryImpl() = default;

quic::QuicArenaScopedPtr<quic::QuicAlarm> QuicAlarmFactoryImpl::CreateAlarm(
    quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate> delegate,
    quic::QuicConnectionArena* arena) {
  if (arena) {
    return arena->New<QuicAlarmImpl>(clock_, task_runner_, std::move(delegate));
  } else {
    return quic::QuicArenaScopedPtr<quic::QuicAlarm>(
        new QuicAlarmImpl(clock_, task_runner_, std::move(delegate)));
  }
}

quic::QuicAlarm* QuicAlarmFactoryImpl::CreateAlarm(
    quic::QuicAlarm::Delegate* delegate) {
  return new QuicAlarmImpl(
      clock_, task_runner_,
      quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate>(delegate));
}

}  // namespace openscreen::osp
