// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OSP_IMPL_QUIC_QUIC_ALARM_FACTORY_IMPL_H_
#define OSP_IMPL_QUIC_QUIC_ALARM_FACTORY_IMPL_H_

#include "platform/api/task_runner.h"
#include "quiche/quic/core/quic_alarm.h"
#include "quiche/quic/core/quic_alarm_factory.h"
#include "quiche/quic/core/quic_clock.h"

namespace openscreen::osp {

class QuicAlarmFactoryImpl : public quic::QuicAlarmFactory {
 public:
  QuicAlarmFactoryImpl(TaskRunner& task_runner, const quic::QuicClock* clock);
  QuicAlarmFactoryImpl(const QuicAlarmFactoryImpl&) = delete;
  QuicAlarmFactoryImpl& operator=(const QuicAlarmFactoryImpl&) = delete;
  ~QuicAlarmFactoryImpl() override;

  // quic::QuicAlarmFactory overrides.
  quic::QuicAlarm* CreateAlarm(quic::QuicAlarm::Delegate* delegate) override;
  quic::QuicArenaScopedPtr<quic::QuicAlarm> CreateAlarm(
      quic::QuicArenaScopedPtr<quic::QuicAlarm::Delegate> delegate,
      quic::QuicConnectionArena* arena) override;

 private:
  TaskRunner& task_runner_;
  const quic::QuicClock* clock_;
};

}  // namespace openscreen::osp

#endif  // OSP_IMPL_QUIC_QUIC_ALARM_FACTORY_IMPL_H_
