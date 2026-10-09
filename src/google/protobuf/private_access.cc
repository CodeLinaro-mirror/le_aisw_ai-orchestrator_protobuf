#include "google/protobuf/private_access.h"

#include "google/protobuf/generated_message_tctable_decl.h"
#include "google/protobuf/message_lite.h"

namespace google {
namespace protobuf {
namespace internal {

internal::ExtensionSet* PrivateAccess::GetExtensionSet(MessageLite* msg) {
  return const_cast<internal::ExtensionSet*>(
      GetExtensionSet(static_cast<const MessageLite*>(msg)));
}

const internal::ExtensionSet* PrivateAccess::GetExtensionSet(
    const MessageLite* msg) {
  auto* tc_table = msg->GetTcParseTable();
  if (tc_table->extension_offset == 0) return nullptr;
  return reinterpret_cast<const internal::ExtensionSet*>(
      reinterpret_cast<const char*>(msg) + tc_table->extension_offset);
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
