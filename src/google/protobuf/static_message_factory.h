#ifndef GOOGLE_PROTOBUF_STATIC_MESSAGE_FACTORY_H__
#define GOOGLE_PROTOBUF_STATIC_MESSAGE_FACTORY_H__

#include <concepts>
#include <utility>

#include "absl/log/absl_check.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/message_lite.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {

namespace internal {

#if PROTOBUF_HAVE_CPP_CONCEPTS
template <typename T>
concept StaticMessageFactory = requires(const T a) {
  { a.New(std::declval<Arena*>()) } -> std::convertible_to<MessageLite*>;
  { a.Default() } -> std::convertible_to<const MessageLite&>;
};
#else
#define StaticMessageFactory typename
#endif  // PROTOBUF_HAVE_CPP_CONCEPTS

class ByPrototype {
 public:
  explicit ByPrototype(const MessageLite* prototype) : prototype_(prototype) {}

  MessageLite* New(Arena* arena) const { return prototype_->New(arena); }

  const MessageLite& Default() const { return *prototype_; }

 private:
  const MessageLite* prototype_;
};

template <typename MessageType>
class ByTemplate {
 public:
  // Only `Get()` needs access to the default element, but we don't want to
  // force instantiation of `MessageType::default_instance()` because it
  // doesn't exist in all configurations.
  explicit ByTemplate() : ByTemplate(nullptr) {}
  explicit ByTemplate(const MessageType* default_instance)
      : default_instance_(default_instance) {}

  MessageLite* New(Arena* arena) const {
    return reinterpret_cast<MessageLite*>(
        Arena::DefaultConstruct<MessageType>(arena));
  }

  const MessageLite& Default() const {
    ABSL_DCHECK(default_instance_ != nullptr);
    return *reinterpret_cast<const MessageLite*>(default_instance_);
  }

 private:
  const MessageType* default_instance_;
};

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_STATIC_MESSAGE_FACTORY_H__
