--
-- Copyright 2024 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--

-- For a given counter timeline (e.g. a single counter track), returns
-- intervals of time where the counter has the same value.
--
-- Intervals are computed in a "forward-looking" way. That is, if a counter
-- changes value at some timestamp, it's assumed it *just* reached that
-- value and it should continue to have that value until the next
-- value change. The final value is assumed to hold until the very end of
-- the trace.
--
-- For example, suppose we have the following data:
-- ```
-- ts=0, value=10, track_id=1
-- ts=0, value=10, track_id=2
-- ts=10, value=10, track_id=1
-- ts=10, value=20, track_id=2
-- ts=20, value=30, track_id=1
-- [end of trace at ts = 40]
-- ```
--
-- Then this macro will generate the following intervals:
-- ```
-- ts=0, dur=20, value=10, track_id=1
-- ts=20, dur=10, value=30, track_id=1
-- ts=0, dur=10, value=10, track_id=2
-- ts=10, dur=30, value=20, track_id=2
-- ```
CREATE PERFETTO MACRO counter_leading_intervals(
  -- A table/view/subquery corresponding to a "counter-like" table.
  -- This table must have the columns "id" and "ts" and "track_id" and "value" corresponding
  -- to an id, timestamp, counter track_id and associated counter value.
  counter_table TableOrSubquery)
-- Table with the schema (id UINT32, ts UINT64, dur UINT64, track_id UINT64,
-- value DOUBLE, next_value DOUBLE, delta_value DOUBLE).
RETURNS TableOrSubquery AS
(
  WITH base AS (
    SELECT
      id,
      ts,
      track_id,
      value,
      LAG(value) OVER (PARTITION BY track_id ORDER BY ts) AS lag_value
    FROM $counter_table
  )
  SELECT
    id,
    ts,
    LEAD(ts, 1, trace_end()) OVER(PARTITION BY track_id ORDER BY ts) - ts AS dur,
    track_id,
    value,
    LEAD(value) OVER(PARTITION BY track_id ORDER BY ts) AS next_value,
    value - lag_value AS delta_value
  FROM base
  WHERE value != lag_value OR lag_value IS NULL
);