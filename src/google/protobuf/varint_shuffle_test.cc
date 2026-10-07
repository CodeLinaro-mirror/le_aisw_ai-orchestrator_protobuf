// Protocol Buffers - Google's data interchange format
// Copyright 2023 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/varint_shuffle.h"

#include <array>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "google/protobuf/protobuf_test.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

using testing::Eq;
using testing::IsNull;
using testing::NotNull;
using testing::Range;

namespace google {
namespace protobuf {
namespace internal {
namespace {

uint64_t ToInt64(char c) { return static_cast<uint8_t>(c); }
uint32_t ToInt32(char c) { return static_cast<uint8_t>(c); }

int32_t Shl(uint32_t v, int bits) { return static_cast<int32_t>(v << bits); }
int32_t Shl(int32_t v, int bits) { return Shl(static_cast<uint32_t>(v), bits); }

int64_t Shl(uint64_t v, int bits) { return static_cast<int64_t>(v << bits); }
int64_t Shl(int64_t v, int bits) { return Shl(static_cast<uint64_t>(v), bits); }

int NaiveParse(const char* p, int32_t& res) {
  int len = 0;
  auto r = ToInt32(*p);
  while (*p++ & 0x80) {
    if (++len == 10) return 11;
    if (len < 5) r += Shl(ToInt32(*p) - 1, len * 7);
  }
  res = r;
  return ++len;
}

// A naive, easy to verify implementation for test purposes.
int NaiveParse(const char* p, int64_t& res) {
  int len = 0;
  auto r = ToInt64(*p);
  while (*p++ & 0x80) {
    if (++len == 10) return 11;
    r += Shl(ToInt64(*p) - 1, len * 7);
  }
  res = r;
  return ++len;
}

// A naive, easy to verify implementation for test purposes.
int NaiveSerialize(char* p, uint64_t value) {
  int n = 0;
  while (value > 127) {
    p[n++] = 0x80 | static_cast<char>(value);
    value >>= 7;
  }
  p[n++] = static_cast<char>(value);
  return n;
}

template <int limit = 10>
const char* Parse(const char* data, int32_t& res) {
  int64_t res64;
  const char* ret = ShiftMixParseVarint<int32_t, limit>(data, res64);
  res = res64;
  return ret;
}

template <int limit = 10>
const char* Parse(const char* data, int64_t& res) {
  return ShiftMixParseVarint<int64_t, limit>(data, res);
}

template <int N>
constexpr auto Limits() {
  std::array<int, N> res{};
  for (int i = 0; i < N; ++i) res[i] = i + 1;
  return res;
}

PB_TEST(ShiftMixParseVarintTest, AllLengths) {
  using T = PB_TEST_CHOOSE_TYPE((int32_t, int64_t));
  int len = PB_TEST_CHOOSE_VALUE(Range(1, 11), "len");
  std::vector<char> bytes;
  for (int i = 1; i < len; ++i) {
    bytes.push_back(static_cast<char>(0xC1 + (i << 1)));
  }
  bytes.push_back('\x01');
  const char* data = bytes.data();

  T expected;
  ASSERT_THAT(NaiveParse(data, expected), Eq(len));

  T result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

PB_TEST(ShiftMixParseVarintTest, NonCanonicalValue) {
  using T = PB_TEST_CHOOSE_TYPE((int32_t, int64_t));
  int len = PB_TEST_CHOOSE_VALUE(Range(1, 11), "len");
  char data[] = {'\xc3', '\xc5', '\xc7', '\xc9', '\xcb',
                 '\xcd', '\xcf', '\xd1', '\xd3', '\x7E'};
  if (len < 10) data[len++] = 0;

  T expected;
  ASSERT_THAT(NaiveParse(data, expected), Eq(len));

  T result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

PB_TEST(ShiftMixParseVarintTest, NonCanonicalZero) {
  using T = PB_TEST_CHOOSE_TYPE((int32_t, int64_t));
  int len = PB_TEST_CHOOSE_VALUE(Range(1, 11), "len");
  char data[] = {'\x80', '\x80', '\x80', '\x80', '\x80',
                 '\x80', '\x80', '\x80', '\x80', '\x7E'};
  if (len < 10) data[len++] = 0;

  T expected;
  ASSERT_THAT(NaiveParse(data, expected), Eq(len));
  ASSERT_THAT(expected, Eq(0));

  T result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(len));
  ASSERT_THAT(result, Eq(expected));
}

PB_TEST(ShiftMixParseVarintTest, HittingLimit) {
  using T = PB_TEST_CHOOSE_TYPE((int32_t, int64_t));
  constexpr int limit = PB_TEST_CHOOSE_CONSTEXPR(Limits<10>(), "limit");

  T res = static_cast<T>(0x9897969594939291ULL);
  char data[10];
  int serialized_len = NaiveSerialize(data, res);
  ASSERT_THAT(serialized_len, Eq(10));

  T result;
  const char* p = Parse<limit>(data, result);
  ASSERT_THAT(p, testing::NotNull());
  ASSERT_THAT(p - data, Eq(limit));
  constexpr int kMaxLimit = std::is_same_v<T, int32_t> ? 5 : 10;
  if (limit < kMaxLimit) {
    res |= Shl(T{-1}, limit * 7);
  }
  ASSERT_THAT(result, Eq(res));
}

PB_TEST(ShiftMixParseVarintTest, AtOrBelowLimit) {
  using T = PB_TEST_CHOOSE_TYPE((int32_t, int64_t));
  constexpr int kMaxLimit = std::is_same_v<T, int32_t> ? 5 : 10;
  constexpr int limit = PB_TEST_CHOOSE_CONSTEXPR(Limits<kMaxLimit>(), "limit");

  T res = std::is_same_v<T, int32_t>
              ? (0x94939291ULL >> (35 - 7 * limit))
              : (0x9897969594939291ULL >> (70 - 7 * limit));
  char data[10];
  int serialized_len = NaiveSerialize(data, res);
  ASSERT_THAT(serialized_len, Eq(limit == kMaxLimit ? 10 : limit));

  T result;
  const char* p = Parse<limit>(data, result);
  ASSERT_THAT(p, testing::NotNull());
  ASSERT_THAT(p - data, Eq(limit));
  ASSERT_THAT(result, Eq(res));
}

PB_TEST(ShiftMixParseVarintTest, OverLong) {
  using T = PB_TEST_CHOOSE_TYPE((int32_t, int64_t));
  char data[] = {'\xc3', '\xc5', '\xc7', '\xc9', '\xcb',
                 '\xcd', '\xcf', '\xd1', '\xd3', '\x81'};
  T result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, IsNull());
}

PB_TEST(ShiftMixParseVarintTest, DroppingOverlongBits) {
  using T = PB_TEST_CHOOSE_TYPE((int32_t, int64_t));
  constexpr int kMaxLen = std::is_same_v<T, int32_t> ? 5 : 10;
  char data[] = {'\xc3', '\xc5', '\xc7', '\xc9', '\xcb',
                 '\xcd', '\xcf', '\xd1', '\xd3', '\x7F'};
  data[kMaxLen - 1] = '\x7F';
  T expected;
  ASSERT_THAT(NaiveParse(data, expected), Eq(kMaxLen));

  T result;
  const char* p = Parse(data, result);
  ASSERT_THAT(p, NotNull());
  ASSERT_THAT(p - data, Eq(kMaxLen));
  ASSERT_THAT(result, Eq(expected));
}


}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"
