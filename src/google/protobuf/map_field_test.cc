// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/map_field.h"

#include <cstdint>
#include <memory>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/strings/str_format.h"
#include "absl/synchronization/barrier.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/types/optional.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/arena_test_util.h"
#include "google/protobuf/map.h"
#include "google/protobuf/map_test_util.h"
#include "google/protobuf/map_unittest.pb.h"
#include "google/protobuf/message.h"
#include "google/protobuf/protobuf_test.h"
#include "google/protobuf/repeated_field.h"
#include "google/protobuf/unittest.pb.h"
#include "google/protobuf/wire_format_lite.h"


// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {

namespace internal {

using proto2_unittest::TestAllTypes;

struct MapFieldTestPeer {
  static auto GetArena(const RepeatedPtrFieldBase& v) { return v.GetArena(); }
  template <typename T>
  static auto& GetMap(T& t) {
    return t.map_;
  }
};

constexpr const auto* kEntryGlobals =
    &proto2_unittest::TestMap_MapInt32Int32Entry_DoNotUse_globals_;

using TestMapField =
    ::google::protobuf::internal::MapField<kEntryGlobals, ::int32_t, ::int32_t>;

class MapFieldBasePrimitiveTest : public testing::Test {
 protected:
  typedef MapField<kEntryGlobals, int32_t, int32_t> MapFieldType;

  MapFieldBasePrimitiveTest()
      : arena_(PB_TEST_CHOOSE_VALUE(testing::Bool(), "arena") ? new Arena()
                                                              : nullptr),
        map_field_(arena_.get()),
        map_field_base_(map_field_.get()) {
    // Get descriptors
    map_descriptor_ = proto2_unittest::TestMap::descriptor()
                          ->FindFieldByName("map_int32_int32")
                          ->message_type();
    key_descriptor_ = map_descriptor_->map_key();
    value_descriptor_ = map_descriptor_->map_value();

    // Build map field
    map_field_base_ = map_field_.get();
    map_ = map_field_->MutableMap();
    initial_value_map_[0] = 100;
    initial_value_map_[1] = 101;
    map_->insert(initial_value_map_.begin(), initial_value_map_.end());
    EXPECT_EQ(2, map_->size());
  }

  std::unique_ptr<Arena> arena_;
  ArenaHolder<MapFieldType> map_field_;
  MapFieldBase* map_field_base_;
  Map<int32_t, int32_t>* map_;
  const Descriptor* map_descriptor_;
  const FieldDescriptor* key_descriptor_;
  const FieldDescriptor* value_descriptor_;
  absl::flat_hash_map<int32_t, int32_t>
      initial_value_map_;  // copy of initial values inserted
};

PB_TEST_F(MapFieldBasePrimitiveTest, SpaceUsedExcludingSelf) {
  EXPECT_LT(0, map_field_base_->SpaceUsedExcludingSelf());
}

PB_TEST_F(MapFieldBasePrimitiveTest, GetRepeatedField) {
  const RepeatedPtrField<Message>& repeated =
      reinterpret_cast<const RepeatedPtrField<Message>&>(
          map_field_base_->GetRepeatedField());
  EXPECT_EQ(2, repeated.size());
  for (int i = 0; i < repeated.size(); i++) {
    const Message& message = repeated.Get(i);
    int key = message.GetReflection()->GetInt32(message, key_descriptor_);
    int value = message.GetReflection()->GetInt32(message, value_descriptor_);
    EXPECT_EQ(value, initial_value_map_[key]);
  }
}

PB_TEST_F(MapFieldBasePrimitiveTest, MutableRepeatedField) {
  RepeatedPtrField<Message>* repeated =
      reinterpret_cast<RepeatedPtrField<Message>*>(
          map_field_base_->MutableRepeatedField());
  EXPECT_EQ(2, repeated->size());
  for (int i = 0; i < repeated->size(); i++) {
    const Message& message = repeated->Get(i);
    int key = message.GetReflection()->GetInt32(message, key_descriptor_);
    int value = message.GetReflection()->GetInt32(message, value_descriptor_);
    EXPECT_EQ(value, initial_value_map_[key]);
  }
}

PB_TEST_F(MapFieldBasePrimitiveTest, Arena) {
  // Allocate a large initial block to avoid mallocs during hooked test.
  std::vector<char> arena_block(128 * 1024);
  ArenaOptions options;
  options.initial_block = &arena_block[0];
  options.initial_block_size = arena_block.size();
  Arena arena(options);

  {
    // TODO: Re-write the test to ensure the memory for the map and
    // repeated fields are allocated from arenas.
    // NoHeapChecker no_heap;

    MapFieldType* map_field = Arena::Create<MapFieldType>(&arena);

    // Set content in map
    (*map_field->MutableMap())[100] = 101;

    // Trigger conversion to repeated field.
    (void)map_field->GetRepeatedField();
  }

  {
    // TODO: Re-write the test to ensure the memory for the map and
    // repeated fields are allocated from arenas.
    // NoHeapChecker no_heap;

    TestMapField* map_field = Arena::Create<TestMapField>(&arena);

    // Trigger conversion to repeated field.
    EXPECT_TRUE(map_field->MutableRepeatedField() != nullptr);

    EXPECT_EQ(MapFieldTestPeer::GetArena(map_field->GetRepeatedField()),
              &arena);
  }
}

PB_TEST_F(MapFieldBasePrimitiveTest, EnforceNoArena) {
  std::unique_ptr<TestMapField> map_field(Arena::Create<TestMapField>(nullptr));
  EXPECT_EQ(MapFieldTestPeer::GetArena(map_field->GetRepeatedField()), nullptr);
}

namespace {
enum State { CLEAN, MAP_DIRTY, REPEATED_DIRTY };
}  // anonymous namespace

class MapFieldStateTest : public testing::Test {
 protected:
  typedef MapField<kEntryGlobals, int32_t, int32_t> MapFieldType;
  MapFieldStateTest()
      : arena_(PB_TEST_CHOOSE_VALUE(testing::Bool(), "arena") ? new Arena()
                                                              : nullptr),
        map_field_(arena_.get()),
        map_field_base_(map_field_.get()),
        state_(PB_TEST_CHOOSE_VALUE((CLEAN, MAP_DIRTY, REPEATED_DIRTY))) {
    // Build map field
    Expect(map_field_.get(), MAP_DIRTY, 0, 0);
    SetState(map_field_.get(), state_);
  }

  void SetState(MapFieldType* map_field, State state) {
    switch (state) {
      case CLEAN:
        AddOneStillClean(map_field);
        break;
      case MAP_DIRTY:
        MakeMapDirty(map_field);
        break;
      case REPEATED_DIRTY:
        MakeRepeatedDirty(map_field);
        break;
      default:
        break;
    }
  }

  void ExpectState(MapFieldType* map_field, State state) {
    switch (state) {
      case CLEAN:
        Expect(map_field, CLEAN, 1, 1);
        break;
      case MAP_DIRTY:
        Expect(map_field, MAP_DIRTY, 1, 0);
        break;
      case REPEATED_DIRTY:
        Expect(map_field, REPEATED_DIRTY, 0, 1);
        break;
      default:
        break;
    }
  }

  void AddOneStillClean(MapFieldType* map_field) {
    MapFieldBase* map_field_base = map_field;
    Map<int32_t, int32_t>* map = map_field->MutableMap();
    (*map)[0] = 0;
    (void)map_field_base->GetRepeatedField();
    Expect(map_field, CLEAN, 1, 1);
  }

  void MakeMapDirty(MapFieldType* map_field) {
    Map<int32_t, int32_t>* map = map_field->MutableMap();
    (*map)[0] = 0;
    Expect(map_field, MAP_DIRTY, 1, 0);
  }

  void MakeRepeatedDirty(MapFieldType* map_field) {
    MakeMapDirty(map_field);
    MapFieldBase* map_field_base = map_field;
    (void)map_field_base->MutableRepeatedField();
    // We use map_ because we don't want to disturb the syncing
    map_field->map_.clear();

    Expect(map_field, REPEATED_DIRTY, 0, 1);
  }

  void Expect(MapFieldType* map_field, State state, int map_size,
              int repeated_size) {
    // We use map_ because we don't want to disturb the syncing
    Map<int32_t, int32_t>* map = &map_field->map_;

    switch (state) {
      case MAP_DIRTY:
        EXPECT_FALSE(map_field->state() != MapFieldType::STATE_MODIFIED_MAP);
        EXPECT_TRUE(map_field->state() !=
                    MapFieldType::STATE_MODIFIED_REPEATED);
        break;
      case REPEATED_DIRTY:
        EXPECT_TRUE(map_field->state() != MapFieldType::STATE_MODIFIED_MAP);
        EXPECT_FALSE(map_field->state() !=
                     MapFieldType::STATE_MODIFIED_REPEATED);
        break;
      case CLEAN:
        EXPECT_TRUE(map_field->state() != MapFieldType::STATE_MODIFIED_MAP);
        EXPECT_TRUE(map_field->state() !=
                    MapFieldType::STATE_MODIFIED_REPEATED);
        break;
      default:
        FAIL();
    }

    EXPECT_EQ(map_size, map->size());
    EXPECT_EQ(repeated_size,
              map_field->maybe_payload() == nullptr
                  ? 0
                  : map_field->maybe_payload()->repeated_field().size());
  }

  std::unique_ptr<Arena> arena_;
  ArenaHolder<MapFieldType> map_field_;
  MapFieldBase* map_field_base_;
  State state_;
};

PB_TEST_F(MapFieldStateTest, GetMap) {
  map_field_->GetMap();
  if (state_ != MAP_DIRTY) {
    Expect(map_field_.get(), CLEAN, 1, 1);
  } else {
    Expect(map_field_.get(), MAP_DIRTY, 1, 0);
  }
}

PB_TEST_F(MapFieldStateTest, MutableMap) {
  map_field_->MutableMap();
  if (state_ != MAP_DIRTY) {
    Expect(map_field_.get(), MAP_DIRTY, 1, 1);
  } else {
    Expect(map_field_.get(), MAP_DIRTY, 1, 0);
  }
}

PB_TEST_F(MapFieldStateTest, MergeFrom) {
  State other_state =
      PB_TEST_CHOOSE_VALUE((CLEAN, MAP_DIRTY, REPEATED_DIRTY), "other");
  ArenaHolder<MapFieldType> other(arena_.get());
  SetState(other.get(), other_state);

  map_field_->MergeFrom(*other);

  if (state_ != MAP_DIRTY) {
    Expect(map_field_.get(), MAP_DIRTY, 1, 1);
  } else {
    Expect(map_field_.get(), MAP_DIRTY, 1, 0);
  }

  ExpectState(other.get(), other_state == MAP_DIRTY ? MAP_DIRTY : CLEAN);
}

PB_TEST_F(MapFieldStateTest, Swap) {
  State other_state =
      PB_TEST_CHOOSE_VALUE((CLEAN, MAP_DIRTY, REPEATED_DIRTY), "other");
  ArenaHolder<MapFieldType> other(arena_.get());
  SetState(other.get(), other_state);

  map_field_->Swap(/*arena=*/arena_.get(), other.get(),
                   /*other_arena=*/arena_.get());

  ExpectState(map_field_.get(), other_state);
  ExpectState(other.get(), state_);
}

PB_TEST_F(MapFieldStateTest, Clear) {
  map_field_->Clear();

  Expect(map_field_.get(), MAP_DIRTY, 0, 0);
}

PB_TEST_F(MapFieldStateTest, SpaceUsedExcludingSelf) {
  map_field_base_->SpaceUsedExcludingSelf();

  ExpectState(map_field_.get(), state_);
}

PB_TEST_F(MapFieldStateTest, GetMapField) {
  (void)map_field_base_->GetRepeatedField();

  if (state_ != REPEATED_DIRTY) {
    Expect(map_field_.get(), CLEAN, 1, 1);
  } else {
    Expect(map_field_.get(), REPEATED_DIRTY, 0, 1);
  }
}

PB_TEST_F(MapFieldStateTest, MutableMapField) {
  (void)map_field_base_->MutableRepeatedField();

  if (state_ != REPEATED_DIRTY) {
    Expect(map_field_.get(), REPEATED_DIRTY, 1, 1);
  } else {
    Expect(map_field_.get(), REPEATED_DIRTY, 0, 1);
  }
}

using MyMapField = MapField<kEntryGlobals, int32_t, int32_t>;

TEST(MapFieldTest, ConstInit) {
  // This tests that `MapField` and all its base classes can be constant
  // initialized.
  PROTOBUF_CONSTINIT static MyMapField field;  // NOLINT
  EXPECT_EQ(field.size(), 0);
}

TEST(MapFieldTest, MutableMapDoesNotAllocatePayload) {
  struct MaybePayload : MapFieldBase {
    // Use a derived type to get access to the protected method.
    // We steal the function pointer here to use below to inspect the instance.
    static constexpr auto getter() { return &MaybePayload::maybe_payload; }
  };
  MyMapField field;
  EXPECT_FALSE((field.*MaybePayload::getter())());
  field.MutableMap();
  EXPECT_FALSE((field.*MaybePayload::getter())());
}


}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"
