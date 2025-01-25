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

CREATE PERFETTO TABLE _input_message_sent
AS
SELECT
  STR_SPLIT(STR_SPLIT(slice.name, '=', 3), ')', 0) AS event_type,
  STR_SPLIT(STR_SPLIT(slice.name, '=', 2), ',', 0) AS event_seq,
  STR_SPLIT(STR_SPLIT(slice.name, '=', 1), ',', 0) AS event_channel,
  thread.tid,
  thread.name AS thread_name,
  process.pid,
  process.name AS process_name,
  slice.ts,
  slice.dur,
  slice.track_id
FROM slice
JOIN thread_track
  ON thread_track.id = slice.track_id
JOIN thread
  USING (utid)
JOIN process
  USING (upid)
WHERE slice.name GLOB 'sendMessage(*';

CREATE PERFETTO TABLE _input_message_received
AS
SELECT
  STR_SPLIT(STR_SPLIT(slice.name, '=', 3), ')', 0) AS event_type,
  STR_SPLIT(STR_SPLIT(slice.name, '=', 2), ',', 0) AS event_seq,
  STR_SPLIT(STR_SPLIT(slice.name, '=', 1), ',', 0) AS event_channel,
  thread.tid,
  thread.name AS thread_name,
  process.pid,
  process.name AS process_name,
  slice.ts,
  slice.dur,
  slice.track_id
FROM slice
JOIN thread_track
  ON thread_track.id = slice.track_id
JOIN thread
  USING (utid)
JOIN process
  USING (upid)
WHERE slice.name GLOB 'receiveMessage(*';

-- All input events with round trip latency breakdown. Input delivery is socket based and every
-- input event sent from the OS needs to be ACK'ed by the app. This gives us 4 subevents to measure
-- latencies between:
-- 1. Input dispatch event sent from OS.
-- 2. Input dispatch event received in app.
-- 3. Input ACK event sent from app.
-- 4. Input ACk event received in OS.
CREATE PERFETTO TABLE android_input_events (
  -- Duration from input dispatch to input received.
  dispatch_latency_dur INT,
  -- Duration from input received to input ACK sent.
  handling_latency_dur INT,
  -- Duration from input ACK sent to input ACK recieved.
  ack_latency_dur INT,
  -- Duration from input dispatch to input event ACK received.
  total_latency_dur INT,
  -- Tid of thread receiving the input event.
  tid INT,
  -- Name of thread receiving the input event.
  thread_name STRING,
  -- Pid of process receiving the input event.
  pid INT,
  -- Name of process receiving the input event.
  process_name STRING,
  -- Input event type. See InputTransport.h: InputMessage#Type
  event_type STRING,
  -- Input event sequence number, monotonically increasing for an event channel and pid.
  event_seq STRING,
  -- Input event channel name.
  event_channel STRING,
  -- Thread track id of input event dispatching thread.
  dispatch_track_id INT,
  -- Timestamp input event was dispatched.
  dispatch_ts INT,
  -- Duration of input event dispatch.
  dispatch_dur INT,
  -- Thread track id of input event receiving thread.
  receive_track_id INT,
  -- Timestamp input event was received.
  receive_ts INT,
  -- Duration of input event receipt.
  receive_dur INT
  )
AS
SELECT
  receive.ts - dispatch.ts AS dispatch_latency_dur,
  finish.ts - receive.ts AS handling_latency_dur,
  finish_ack.ts - finish.ts AS ack_latency_dur,
  finish_ack.ts - dispatch.ts AS total_latency_dur,
  finish.tid AS tid,
  finish.thread_name AS thread_name,
  finish.pid AS pid,
  finish.process_name AS process_name,
  dispatch.event_type,
  dispatch.event_seq,
  dispatch.event_channel,
  dispatch.track_id AS dispatch_track_id,
  dispatch.ts AS dispatch_ts,
  dispatch.dur AS dispatch_dur,
  receive.ts AS receive_ts,
  receive.dur AS receive_dur,
  receive.track_id AS receive_track_id
FROM (SELECT * FROM _input_message_sent WHERE thread_name = 'InputDispatcher') dispatch
JOIN (SELECT * FROM _input_message_received WHERE event_type NOT IN ('0x2', 'FINISHED')) receive
  ON
    REPLACE(receive.event_channel, '(client)', '(server)') = dispatch.event_channel
    AND dispatch.event_seq = receive.event_seq
JOIN (SELECT * FROM _input_message_sent WHERE thread_name != 'InputDispatcher') finish
  ON
    REPLACE(finish.event_channel, '(client)', '(server)') = dispatch.event_channel
    AND dispatch.event_seq = finish.event_seq
JOIN (SELECT * FROM _input_message_received WHERE event_type IN ('0x2', 'FINISHED')) finish_ack
  ON finish_ack.event_channel = dispatch.event_channel AND dispatch.event_seq = finish_ack.event_seq;

-- Key events processed by the Android framework (from android.input.inputevent data source).
CREATE PERFETTO VIEW android_key_events(
  -- ID of the trace entry
  id INT,
  -- The randomly-generated ID associated with each input event processed
  -- by Android Framework, used to track the event through the input pipeline
  event_id INT,
  -- The timestamp of when the input event was processed by the system
  ts INT,
  -- Details of the input event parsed from the proto message
  arg_set_id INT
) AS
SELECT
  id,
  event_id,
  ts,
  arg_set_id
FROM __intrinsic_android_key_events;

-- Motion events processed by the Android framework (from android.input.inputevent data source).
CREATE PERFETTO VIEW android_motion_events(
  -- ID of the trace entry
  id INT,
  -- The randomly-generated ID associated with each input event processed
  -- by Android Framework, used to track the event through the input pipeline
  event_id INT,
  -- The timestamp of when the input event was processed by the system
  ts INT,
  -- Details of the input event parsed from the proto message
  arg_set_id INT
) AS
SELECT
  id,
  event_id,
  ts,
  arg_set_id
FROM __intrinsic_android_motion_events;

-- Input event dispatching information in Android (from android.input.inputevent data source).
CREATE PERFETTO VIEW android_input_event_dispatch(
  -- ID of the trace entry
  id INT,
  -- Event ID of the input event that was dispatched
  event_id INT,
  -- Extra args parsed from the proto message
  arg_set_id INT,
  -- Vsync ID that identifies the state of the windows during which the dispatch decision was made
  vsync_id INT,
  -- Window ID of the window receiving the event
  window_id INT
) AS
SELECT
  id,
  event_id,
  arg_set_id,
  vsync_id,
  window_id
FROM __intrinsic_android_input_event_dispatch;
