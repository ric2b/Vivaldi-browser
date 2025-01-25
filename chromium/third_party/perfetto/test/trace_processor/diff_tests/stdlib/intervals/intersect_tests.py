#!/usr/bin/env python3
# Copyright (C) 2024 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License a
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from python.generators.diff_tests.testing import Path, DataPath, Metric
from python.generators.diff_tests.testing import Csv, Json, TextProto
from python.generators.diff_tests.testing import DiffTestBlueprint
from python.generators.diff_tests.testing import TestSuite


class IntervalsIntersect(TestSuite):

  def test_simple_interval_intersect(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        #      0 1 2 3 4 5 6 7
        # A:   _ - - - - - - _
        # B:   - - _ - - _ - -
        # res: _ - _ - - _ - _
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 6)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 2),
            (1, 3, 2),
            (2, 6, 2)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!(A, B, ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        1,1,0,0
        3,2,0,1
        6,1,0,2
        """))

  def test_simple_interval_intersect_rev(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        #      0 1 2 3 4 5 6 7
        # A:   _ - - - - - - _
        # B:   - - _ - - _ - -
        # res: _ - _ - - _ - _
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 6)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 2),
            (1, 3, 2),
            (2, 6, 2)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!(B, A, ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        1,1,0,0
        3,2,1,0
        6,1,2,0
        """))

  def test_no_overlap(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   __-
        # B:   -__
        # res: ___
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 2, 1)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!(A, B, ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_no_overlap_rev(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   __-
        # B:   -__
        # res: ___
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 2, 1)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!(B, A, ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_no_empty(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   __-
        # B:   -__
        # res: ___
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 2, 1)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
        SELECT * FROM A LIMIT 0;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!(A, B, ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_no_empty_rev(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   __-
        # B:   -__
        # res: ___
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 2, 1)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
        SELECT * FROM A LIMIT 0;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!(B, A, ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_single_point_overlap(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   _-
        # B:   -_
        # res: __
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 1)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!(A, B, ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_single_point_overlap_rev(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        # A:   _-
        # B:   -_
        # res: __
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 1)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 1)
          )
          SELECT * FROM data;

        SELECT ts, dur, id_0, id_1
        FROM _interval_intersect!(B, A, ())
        ORDER BY ts;
        """,
        out=Csv("""
        "ts","dur","id_0","id_1"
        """))

  def test_single_interval(self):
    return DiffTestBlueprint(
        trace=TextProto(""),
        #      0 1 2 3 4 5 6 7
        # A:   _ - - - - - - _
        # B:   - - _ - - _ - -
        # res: _ - _ - - _ - _
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE A AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 1, 6)
          )
          SELECT * FROM data;

        CREATE PERFETTO TABLE B AS
          WITH data(id, ts, dur) AS (
            VALUES
            (0, 0, 2),
            (1, 3, 2),
            (2, 6, 2)
          )
          SELECT * FROM data;

        SELECT *
        FROM _interval_intersect_single!(1, 6, B)
        ORDER BY ts;
        """,
        out=Csv("""
        "id","ts","dur"
        0,1,1
        1,3,2
        2,6,1
        """))

  def test_sanity_check(self):
    return DiffTestBlueprint(
        trace=DataPath('example_android_trace_30s.pb'),
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE trace_interval AS
        SELECT
          0 AS id,
          TRACE_START() AS ts,
          TRACE_DUR() AS dur;

        CREATE PERFETTO TABLE non_overlapping AS
        SELECT
          id, ts, dur
        FROM thread_state
        WHERE utid = 1 AND dur != -1;

        WITH ii AS (
          SELECT *
          FROM _interval_intersect!(trace_interval, non_overlapping, ())
        )
        SELECT
          (SELECT count(*) FROM ii) AS ii_count,
          (SELECT count(*) FROM non_overlapping) AS thread_count,
          (SELECT sum(dur) FROM ii) AS ii_sum,
          (SELECT sum(dur) FROM non_overlapping) AS thread_sum;
        """,
        out=Csv("""
        "ii_count","thread_count","ii_sum","thread_sum"
        313,313,27540674879,27540674879
        """))

  def test_sanity_check_single_interval(self):
    return DiffTestBlueprint(
        trace=DataPath('example_android_trace_30s.pb'),
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        CREATE PERFETTO TABLE non_overlapping AS
        SELECT
          id, ts, dur
        FROM thread_state
        WHERE utid = 1 AND dur != -1;

        WITH ii AS (
          SELECT *
          FROM _interval_intersect_single!(
            TRACE_START(),
            TRACE_DUR(),
            non_overlapping)
        )
        SELECT
          (SELECT count(*) FROM ii) AS ii_count,
          (SELECT count(*) FROM non_overlapping) AS thread_count,
          (SELECT sum(dur) FROM ii) AS ii_sum,
          (SELECT sum(dur) FROM non_overlapping) AS thread_sum;
        """,
        out=Csv("""
        "ii_count","thread_count","ii_sum","thread_sum"
        313,313,27540674879,27540674879
        """))

  def test_sanity_multiple_partitions(self):
    return DiffTestBlueprint(
        trace=DataPath('example_android_trace_30s.pb'),
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        SELECT * FROM _interval_intersect!(
          (SELECT id, ts, dur, utid, cpu FROM sched WHERE dur > 0 LIMIT 10),
          (SELECT id, ts, dur, utid, cpu FROM sched WHERE dur > 0 LIMIT 10),
          (utid, cpu)
        );
        """,
        out=Csv("""
        "ts","dur","id_0","id_1","utid","cpu"
        70730062200,125364,0,0,1,0
        70730187564,20297242,1,1,0,0
        70731483398,24583,9,9,10,3
        70731458606,24792,8,8,9,3
        70731393294,42396,5,5,6,3
        70731435690,22916,6,6,7,3
        70731161731,35000,3,3,4,3
        70731196731,196563,4,4,5,3
        70731438502,55261,7,7,8,6
        70731135898,25833,2,2,2,3
        """))

  def test_sanity_single_partitions(self):
    return DiffTestBlueprint(
        trace=DataPath('example_android_trace_30s.pb'),
        query="""
        INCLUDE PERFETTO MODULE intervals.intersect;

        SELECT * FROM _interval_intersect!(
          (SELECT id, ts, dur, utid, cpu FROM sched WHERE dur > 0 LIMIT 10),
          (SELECT id, ts, dur, utid, cpu FROM sched WHERE dur > 0 LIMIT 10),
          (utid)
        );
        """,
        out=Csv("""
        "ts","dur","id_0","id_1","utid"
        70731458606,24792,8,8,9
        70731161731,35000,3,3,4
        70731393294,42396,5,5,6
        70730187564,20297242,1,1,0
        70731135898,25833,2,2,2
        70731438502,55261,7,7,8
        70731483398,24583,9,9,10
        70731196731,196563,4,4,5
        70731435690,22916,6,6,7
        70730062200,125364,0,0,1
        """))
