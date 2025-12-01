/*
 * Copyright 2019 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fcp/tensorflow/host_object.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace fcp {

using ::testing::Eq;

class WidgetInterface {
 public:
  virtual ~WidgetInterface() = default;
  virtual void Poke(int value) = 0;
};

class WidgetImpl : public WidgetInterface {
 public:
  void Poke(int value) final {
    counter_ += value;
  }

  int counter() const {
    return counter_;
  }
 private:
  int counter_ = 0;
};

TEST(HostObjectTest, LookupFailure) {
  std::optional<std::shared_ptr<WidgetInterface>> p =
      HostObjectRegistry<WidgetInterface>::TryLookup(RandomToken::Generate());
  EXPECT_THAT(p, Eq(std::nullopt));
}

TEST(HostObjectTest, LookupSuccess) {
  std::shared_ptr<WidgetImpl> obj = std::make_shared<WidgetImpl>();
  HostObjectRegistration reg =
      HostObjectRegistry<WidgetInterface>::Register(obj);

  std::optional<std::shared_ptr<WidgetInterface>> p =
      HostObjectRegistry<WidgetInterface>::TryLookup(reg.token());
  EXPECT_TRUE(p.has_value());

  (*p)->Poke(123);
  EXPECT_THAT(obj->counter(), Eq(123));
  EXPECT_THAT(p->get(), Eq(obj.get()));
}

TEST(HostObjectTest, Unregister) {
  std::shared_ptr<WidgetImpl> obj = std::make_shared<WidgetImpl>();

  std::optional<RandomToken> token;
  {
    HostObjectRegistration reg =
        HostObjectRegistry<WidgetInterface>::Register(obj);
    token = reg.token();
  }

  std::optional<std::shared_ptr<WidgetInterface>> p =
      HostObjectRegistry<WidgetInterface>::TryLookup(*token);
  EXPECT_THAT(p, Eq(std::nullopt));
}

}  // namespace fcp
