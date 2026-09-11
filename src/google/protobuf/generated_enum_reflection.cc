#include "google/protobuf/generated_enum_reflection.h"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/types/span.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message_lite.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google::protobuf::internal {
// Internal helper routine for NameOfChunkyEnum in the header file.
// Allocates and fills a simple array of string pointers, based on
// reflection information about the names of the enums.  This routine
// allocates entries based on the provided chunks.
ABSL_ATTRIBUTE_COLD PROTOBUF_NOINLINE const std::string * PROTOBUF_NONNULL *
    PROTOBUF_NONNULL InitializeFastEnumCache(
        FastEnumCache* PROTOBUF_NONNULL fast_enum_cache,
        const EnumDescriptor* PROTOBUF_NONNULL desc,
        absl::Span<const ChunkInfo> chunks) {
  // Use call_once to avoid a race condition in initializing the cache.
  absl::call_once(fast_enum_cache->loaded, [&]() {
    // Maximum of 8 chunks, plus one to hold the overall total.
    size_t chunk_offsets[9];
    chunk_offsets[0] = 0;
    for (size_t i = 0; i < chunks.size(); ++i) {
      chunk_offsets[i + 1] =
          chunk_offsets[i] +
          static_cast<size_t>(chunks[i].max_val - chunks[i].min_val) + 1;
    }
    const std::string** str_ptrs =
        new const std::string*[chunk_offsets[chunks.size()]];
    // Initialize all entries to the empty string.
    std::fill_n(str_ptrs, chunk_offsets[chunks.size()],
                &GetEmptyStringAlreadyInited());
    const int count = desc->value_count();
    // Iterate in reverse order to ensure that if there are aliases, the first
    // one wins.
    for (int i = count - 1; i >= 0; --i) {
      const int num = desc->value(i)->number();
      for (size_t j = 0; j < chunks.size(); ++j) {
        const ChunkInfo& chunk = chunks[j];
        if (num >= chunk.min_val && num <= chunk.max_val) {
          str_ptrs[chunk_offsets[j] +
                   static_cast<uint32_t>(num - chunk.min_val)] =
              &internal::NameOfEnumAsString(desc->value(i));
          break;
        }
      }
    }
    // Atomically publish the cache. Doing this inside the call_once ensures
    // that no thread sees the uninitialized or partially initialized cache.
    fast_enum_cache->flat_cache.store(str_ptrs, std::memory_order_release);
  });
  return fast_enum_cache->flat_cache.load(std::memory_order_acquire);
}
}  // namespace protobuf
}  // namespace google::internal

#include "google/protobuf/port_undef.inc"
