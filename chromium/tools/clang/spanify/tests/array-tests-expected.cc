// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <array>

// No rewrite expected.
extern const int kPropertyVisitedIDs[];

namespace ns1 {

struct Type1 {
  int value;
};

}  // namespace ns1

void fct() {
  // Expected rewrite:
  // std::array<int, 4> buf = {1, 2, 3, 4};
  std::array<int, 4> buf = {1, 2, 3, 4};
  int index = 0;
  buf[index] = 11;

  // Expected rewrite:
  // std::array<int, 5> buf2 = {1, 1, 1, 1, 1};
  std::array<int, 5> buf2 = {1, 1, 1, 1, 1};
  buf2[index] = 11;

  constexpr int size = 5;
  // Expected rewrite:
  // constexpr std::array<int, size> buf3 = {1, 1, 1, 1, 1};
  constexpr std::array<int, size> buf3 = {1, 1, 1, 1, 1};
  (void)buf3[index];

  // Expected rewrite:
  // std::array<int, buf3[0]> buf4;
  std::array<int, buf3[0]> buf4;
  buf4[index] = 11;

  // Expected rewrite:
  // std::array<ns1::Type, 5> buf5 = {{1}, {1}, {1}, {1}, {1}};
  std::array<ns1::Type1, 5> buf5 = {{1}, {1}, {1}, {1}, {1}};
  buf5[index].value = 11;

  // Expected rewrite:
  // std::array<uint16_t, 3> buf6;
  std::array<uint16_t, 3> buf6 = {1, 1, 1};
  buf6[index] = 1;

  // Expected rewrite:
  // std::array<int (*)(int), 16> buf7 = {nullptr};
  std::array<int (*)(int), 16> buf7 = {nullptr};
  buf7[index] = nullptr;

  // Expected rewrite:
  // std::array<int (**)[], 16> buf8 = {nullptr};
  std::array<int(**)[], 16> buf8 = {nullptr};
  buf8[index] = nullptr;

  using Arr = int(**)[];
  // Expected rewrite:
  // std::array<Arr, buf3[0]> buf9 = {nullptr};
  std::array<Arr, buf3[0]> buf9 = {nullptr};
  buf9[index] = nullptr;

  index = kPropertyVisitedIDs[index];
}
