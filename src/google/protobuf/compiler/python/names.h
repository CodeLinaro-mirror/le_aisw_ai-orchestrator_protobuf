#ifndef GOOGLE_PROTOBUF_COMPILER_PYTHON_NAMES_H__
#define GOOGLE_PROTOBUF_COMPILER_PYTHON_NAMES_H__

#include <string>

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {

class FileDescriptor;

namespace compiler {
namespace python {

// Returns the Python module name expected for a given .proto filename.
PROTOC_EXPORT std::string ModuleName(const FileDescriptor* file);

// Returns the stripped Python module name expected for a given .proto filename.
PROTOC_EXPORT std::string StrippedModuleName(const FileDescriptor* file);

}  // namespace python
}  // namespace compiler
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_COMPILER_PYTHON_NAMES_H__
