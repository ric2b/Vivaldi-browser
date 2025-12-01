// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <string>
#include <thread>  // NOLINT(build/c++11)

#include "gtest/gtest.h"
#include "fcp/tracing/test/tracing_schema.h"
#include "fcp/tracing/test_tracing_recorder.h"
#include "fcp/tracing/tracing_severity.h"
#include "flatbuffers/flatbuffers.h"

namespace fcp {
namespace {

using flatbuffers::FlatBufferBuilder;
using flatbuffers::GetRoot;
using testing::_;
using testing::ElementsAre;
using testing::Eq;
using testing::Gt;
using testing::Not;
using testing::SizeIs;
using testing::UnorderedElementsAre;

TEST(Tracing, TraitsTag) {
  EXPECT_EQ(TracingTraits<SpanWithId>::kTag.str(), "SWID");
  EXPECT_EQ(TracingTraits<EventBar>::kTag.str(), "EBAR");
}

TEST(Tracing, TracingSeverity) {
  EXPECT_EQ(TracingTraits<SpanWithId>::kSeverity, fcp::TracingSeverity::kInfo);
  EXPECT_EQ(TracingTraits<ErrorEvent>::kSeverity, fcp::TracingSeverity::kError);
  EXPECT_EQ(TracingTraits<EventWithNoData>::kSeverity,
            fcp::TracingSeverity::kWarning);
}

TEST(Tracing, TraitsCreate) {
  FlatBufferBuilder fbb_foo;
  fbb_foo.Finish(TracingTraits<EventFoo>::Create(222, 333, &fbb_foo));
  auto foo = GetRoot<EventFoo>(fbb_foo.GetBufferPointer());
  EXPECT_EQ(foo->first(), 222);
  EXPECT_EQ(foo->second(), 333);

  // Creating a flat buffer involving a string field has different codegen
  // path, testing this as well:
  FlatBufferBuilder fbb_bar;
  fbb_bar.Finish(
      TracingTraits<EventBar>::Create(444, "Hello world!", &fbb_bar));
  auto bar = GetRoot<EventBar>(fbb_bar.GetBufferPointer());
  EXPECT_EQ(bar->first(), 444);
  EXPECT_EQ(bar->second()->str(), "Hello world!");

  // Also make sure that a flatbuf involving a string field can be created using
  // a std::string.
  FlatBufferBuilder fbb_baz;
  std::string hello_str = "Hello world!";
  fbb_baz.Finish(TracingTraits<EventBar>::Create(444, hello_str, &fbb_baz));
  auto baz = GetRoot<EventBar>(fbb_baz.GetBufferPointer());
  EXPECT_EQ(baz->first(), 444);
  EXPECT_EQ(baz->second()->str(), "Hello world!");
}

TEST(Tracing, TraitsCreateFieldOrder) {
  int first_i = -333;
  int second_i = 444;
  FlatBufferBuilder fbb_foo;
  fbb_foo.Finish(
      TracingTraits<FieldOrder>::Create(first_i, second_i, "hello", &fbb_foo));
  auto foo = GetRoot<FieldOrder>(fbb_foo.GetBufferPointer());
  EXPECT_EQ(foo->fieldz(), first_i);
  EXPECT_EQ(foo->fieldy(), second_i);
  EXPECT_EQ(foo->fieldx()->str(), "hello");

  FlatBufferBuilder fbb_bar;
  fbb_bar.Finish(TracingTraits<OrderWithIds>::Create("hello", first_i, second_i,
                                                     &fbb_bar));
  auto bar = GetRoot<OrderWithIds>(fbb_bar.GetBufferPointer());
  EXPECT_EQ(bar->fieldz(), first_i);
  EXPECT_EQ(bar->fieldy(), second_i);
  EXPECT_EQ(bar->fieldx()->str(), "hello");
}

TEST(Tracing, TraitsCreateAllTypes) {
  FlatBufferBuilder fbb;
  std::int8_t byte = -1;
  std::uint8_t ubyte = 1;
  std::int16_t short_i = -256;
  std::uint16_t ushort_i = 256;
  int i = -333;
  unsigned int ui = 444;
  float f = 1.1;
  std::int64_t li = -4294967296;
  std::uint64_t uli = 4294967296;
  double d = 12312318.99999999;
  fbb.Finish(TracingTraits<AllTypes>::Create(byte, ubyte, true, short_i,
                                             ushort_i, i, ui, f, li, uli, d,
                                             "hello", &fbb));
  auto foo = GetRoot<AllTypes>(fbb.GetBufferPointer());
  EXPECT_EQ(foo->fieldz(), byte);
  EXPECT_EQ(foo->fieldy(), ubyte);
  EXPECT_EQ(foo->fieldx(), true);
  EXPECT_EQ(foo->fieldw(), short_i);
  EXPECT_EQ(foo->fieldv(), ushort_i);
  EXPECT_EQ(foo->fieldu(), i);
  EXPECT_EQ(foo->fieldt(), ui);
  EXPECT_EQ(foo->fields(), f);
  EXPECT_EQ(foo->fieldr(), li);
  EXPECT_EQ(foo->fieldq(), uli);
  EXPECT_EQ(foo->fieldp(), d);
  EXPECT_EQ(foo->fieldo()->str(), "hello");
}

TEST(Tracing, TraitsCreateEnum) {
  FlatBufferBuilder fbb;
  fbb.Finish(TracingTraits<ColorEnum>::Create(Color_Blue, &fbb));
  auto foo = GetRoot<ColorEnum>(fbb.GetBufferPointer());
  EXPECT_EQ(foo->color(), Color_Blue);
}

TEST(Tracing, TraitsCreateDeprecatedField) {
  FlatBufferBuilder fbb_foo;
  fbb_foo.Finish(TracingTraits<DeprecatedInt>::Create(222, &fbb_foo));
  auto foo = GetRoot<EventFoo>(fbb_foo.GetBufferPointer());
  EXPECT_EQ(foo->second(), 222);
}

TEST(Tracing, LookupTraitByTag) {
  EXPECT_EQ(TracingTraitsBase::Lookup(TracingTag("SWID"))->Name(),
            "SpanWithId");
  EXPECT_EQ(TracingTraitsBase::Lookup(TracingTag("EBAR"))->Name(), "EventBar");
}

TEST(Tracing, IntegrationTest) {
  TestTracingRecorder tracing_recorder;
  {
    TracingSpan<SpanWithId> inner(111);
    Trace<EventFoo>(222, 333);
    Trace<EventBar>(444, "Hello world!");
    Trace<EventFoo>(555, 666);
  }
  {
    TracingSpan<SpanWithNoData> inner;
    Trace<EventWithNoData>();
  }
  EXPECT_THAT(
      tracing_recorder.root(),
      ElementsAre(AllOf(IsSpan<SpanWithId>(),
                        ElementsAre(IsEvent<EventFoo>(222, 333),
                                    IsEvent<EventBar>(444, "Hello world!"),
                                    IsEvent<EventFoo>(555, 666))),
                  AllOf(IsSpan<SpanWithNoData>(),
                        ElementsAre(IsEvent<EventWithNoData>()))))
      << "Tracing span/events structure and content must match";
}

TEST(Tracing, UnscopedSpanIntegrationTest) {
  TestTracingRecorder tracing_recorder;
  auto outer = std::make_unique<UnscopedTracingSpan<SpanWithId>>(111);
  auto inner =
      std::make_unique<UnscopedTracingSpan<SpanWithId>>(outer->Ref(), 222);
  {
    TracingSpan<SpanWithNoData> child_of_inner(inner->Ref());
    Trace<EventFoo>(333, 444);
  }
  {
    TracingSpan<SpanWithNoData> another_child_of_inner(inner->Ref());
    Trace<EventFoo>(555, 666);
    Trace<EventBar>(inner->Ref(), 1, "Trace in unscoped span!");
    Trace<EventBar>(another_child_of_inner.Ref(), 1,
                    "Trace in explicitly specified tracing span!");
  }
  TracingSpan<SpanWithNoData> unrelated_span;
  Trace<EventBar>(777, "Hello world!");

  EXPECT_THAT(
      tracing_recorder.root(),
      ElementsAre(
          AllOf(IsSpan<SpanWithId>(111),
                ElementsAre(AllOf(
                    IsSpan<SpanWithId>(222),
                    ElementsAre(
                        AllOf(IsSpan<SpanWithNoData>(),
                              ElementsAre(IsEvent<EventFoo>(333, 444))),
                        AllOf(IsSpan<SpanWithNoData>(),
                              ElementsAre(IsEvent<EventFoo>(555, 666),
                                          IsEvent<EventBar>(
                                              1,
                                              "Trace in explicitly specified "
                                              "tracing span!"))),
                        IsEvent<EventBar>(1, "Trace in unscoped span!"))))),
          AllOf(IsSpan<SpanWithNoData>(),
                ElementsAre(IsEvent<EventBar>(777, "Hello world!")))))
      << "Tracing span/events structure and content must match";
}

TEST(Tracing, ThreadingUnscopedIntegrationTest) {
  TestTracingRecorder tracing_recorder;
  auto outer = std::make_unique<UnscopedTracingSpan<SpanWithId>>(111);
  std::thread thread1([ref = outer->Ref()]() {
    TracingSpan<SpanWithNoData> child_of_outer(ref);
    Trace<EventFoo>(333, 444);
  });
  std::thread thread2([ref = outer->Ref()]() {
    TracingSpan<SpanWithNoData> another_child_of_outer(ref);
    Trace<EventFoo>(555, 666);
    Trace<EventBar>(ref, 1, "Trace in unscoped span!");
    Trace<EventBar>(another_child_of_outer.Ref(), 1, "Trace in local span!");
  });
  TracingSpan<SpanWithNoData> unrelated_span;
  Trace<EventBar>(777, "Hello world!");
  thread1.join();
  thread2.join();

  EXPECT_THAT(
      tracing_recorder.root(),
      ElementsAre(AllOf(IsSpan<SpanWithId>(111),
                        UnorderedElementsAre(
                            AllOf(IsSpan<SpanWithNoData>(),
                                  ElementsAre(IsEvent<EventFoo>(333, 444))),
                            AllOf(IsSpan<SpanWithNoData>(),
                                  ElementsAre(IsEvent<EventFoo>(555, 666),
                                              IsEvent<EventBar>(
                                                  1, "Trace in local span!"))),
                            IsEvent<EventBar>(1, "Trace in unscoped span!"))),
                  AllOf(IsSpan<SpanWithNoData>(),
                        ElementsAre(IsEvent<EventBar>(777, "Hello world!")))))
      << "Tracing span/events structure and content must match";
}

TEST(Tracing, ThreadingScopedIntegrationTest) {
  TestTracingRecorder tracing_recorder;
  TracingSpan<SpanWithId> outer(111);
  std::thread thread1([ref = outer.Ref()]() {
    TracingSpan<SpanWithNoData> child_of_outer(ref);
    Trace<EventFoo>(333, 444);
  });
  std::thread thread2([ref = outer.Ref()]() {
    TracingSpan<SpanWithNoData> another_child_of_outer(ref);
    Trace<EventFoo>(555, 666);
    Trace<EventBar>(ref, 1, "Trace in unscoped span!");
    Trace<EventBar>(1, "Trace in local span!");
  });
  TracingSpan<SpanWithNoData> unrelated_span;
  Trace<EventBar>(777, "Hello world!");
  thread1.join();
  thread2.join();

  EXPECT_THAT(
      tracing_recorder.root(),
      ElementsAre(AllOf(
          IsSpan<SpanWithId>(111),
          UnorderedElementsAre(
              AllOf(IsSpan<SpanWithNoData>(),
                    ElementsAre(IsEvent<EventFoo>(333, 444))),
              AllOf(IsSpan<SpanWithNoData>(),
                    ElementsAre(IsEvent<EventFoo>(555, 666),
                                IsEvent<EventBar>(1, "Trace in local span!"))),
              IsEvent<EventBar>(1, "Trace in unscoped span!"),
              AllOf(IsSpan<SpanWithNoData>(),
                    ElementsAre(IsEvent<EventBar>(777, "Hello world!")))))))
      << "Tracing span/events structure and content must match";
}

TEST(Tracing, AdvancedMatching) {
  TestTracingRecorder tracing_recorder;
  {
    TracingSpan<SpanWithId> span(111);
    Trace<EventBar>(222, "Hello world!");
  }

  auto span = tracing_recorder.root()[0];
  auto event = span[0];
  EXPECT_THAT(span, IsSpan<SpanWithId>());
  EXPECT_THAT(event, IsEvent<EventBar>());
  EXPECT_THAT(span,
              AllOf(IsSpan<SpanWithId>(), ElementsAre(IsEvent<EventBar>())));
  EXPECT_THAT(span, IsSpan<SpanWithId>(_));
  EXPECT_THAT(span, IsSpan<SpanWithId>(Eq(111)));
  EXPECT_THAT(span, IsSpan<SpanWithId>(Gt(100)));
  EXPECT_THAT(event, IsEvent<EventBar>(Eq(222), Eq("Hello world!")));
  EXPECT_THAT(event, IsEvent<EventBar>(_, Eq("Hello world!")));
  EXPECT_THAT(event, IsEvent<EventBar>(_, _));
  EXPECT_THAT(event, IsEvent<EventBar>(Eq(222), _));
  EXPECT_THAT(event, IsEvent<EventBar>(Not(Eq(666)), _));
  EXPECT_THAT(event, Not(IsEvent<EventBar>(Eq(666), _)));
  EXPECT_THAT(event, Not(IsEvent<EventFoo>()));
}

TEST(Tracing, MultipleRecorders) {
  // NOTE: it is not a recommended scenario to have multiple instances of
  // TestTracingRecorder per unit test, but this code path is enforced to
  // ensure correct behavior of cleaning up global state so it is not carried
  // over between tests.
  {
    TestTracingRecorder tracing_recorder;
    Trace<EventFoo>(222, 333);
    EXPECT_THAT(tracing_recorder.root()[0], IsEvent<EventFoo>(222, 333));
  }
  {
    TestTracingRecorder tracing_recorder;
    Trace<EventFoo>(444, 555);
    EXPECT_THAT(tracing_recorder.root()[0], IsEvent<EventFoo>(444, 555));
  }
}

TEST(Tracing, TraceError) {
  TestTracingRecorder tracing_recorder;
  tracing_recorder.ExpectError<ErrorEvent>();
  {
    TracingSpan<SpanWithId> inner(111);
    Error err = TraceError<ErrorEvent>("there was a bug");
    (void)err;
  }
}

TEST(Tracing, FindOnlySpan) {
  TestTracingRecorder tracing_recorder;
  {
    TracingSpan<SpanWithNoData> outer;
    { TracingSpan<SpanWithId> inner(111); }
    EXPECT_EQ(tracing_recorder.FindOnlySpan<SpanWithId>().data()->id(), 111);
  }
}

TEST(Tracing, FindAllSpans) {
  TestTracingRecorder tracing_recorder;
  {
    TracingSpan<SpanWithNoData> outer;
    {
      TracingSpan<SpanWithId> inner1(111);
      { TracingSpan<SpanWithId> inner2(222); }
    }
    EXPECT_THAT(tracing_recorder.FindAllSpans<SpanWithId>(),
                ElementsAre(IsSpan<SpanWithId>(), IsSpan<SpanWithId>()));
    EXPECT_THAT(tracing_recorder.FindAllSpans<SpanNeverLogged>(), SizeIs(0));
  }
}

TEST(Tracing, FindOnlyEvent) {
  TestTracingRecorder tracing_recorder;
  {
    TracingSpan<SpanWithNoData> outer;
    { Trace<EventFoo>(111, 222); }
    EXPECT_EQ(tracing_recorder.FindOnlyEvent<EventFoo>().data()->first(), 111);
    EXPECT_EQ(tracing_recorder.FindOnlyEvent<EventFoo>().data()->second(), 222);
  }
}

TEST(Tracing, FindAllEvents) {
  TestTracingRecorder tracing_recorder;
  {
    TracingSpan<SpanWithNoData> outer;
    {
      Trace<EventFoo>(111, 222);
      TracingSpan<SpanWithNoData> inner;
      { Trace<EventFoo>(333, 444); }
    }
    EXPECT_THAT(tracing_recorder.FindAllEvents<EventFoo>(),
                ElementsAre(IsEvent<EventFoo>(), IsEvent<EventFoo>()));
    EXPECT_THAT(tracing_recorder.FindAllEvents<EventNeverLogged>(), SizeIs(0));
  }
}
TEST(Tracing, CreateJsonString) {
  FlatBufferBuilder fbb_foo;
  fbb_foo.Finish(TracingTraits<EventFoo>::Create(222, 333, &fbb_foo));
  auto foo_buf = fbb_foo.GetBufferPointer();
  auto foo = GetRoot<EventFoo>(fbb_foo.GetBufferPointer());
  EXPECT_EQ(foo->first(), 222);
  EXPECT_EQ(foo->second(), 333);

  TracingTraits<EventFoo> tracing_traits;
  std::string expected = "{\n  first: 222,\n  second: 333\n}\n";
  std::string json_gen = tracing_traits.JsonStringFormat(foo_buf);
  EXPECT_EQ(expected, json_gen);
}

// TODO(team) Add Testing for when the flatbuf has a package name

}  // namespace
}  // namespace fcp
