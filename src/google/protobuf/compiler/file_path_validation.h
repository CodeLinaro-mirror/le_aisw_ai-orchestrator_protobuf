// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

// Validation of .proto file paths before code generation. This is used by
// protoc and plugin hosts; individual code generators should not need to call
// it, since every file they receive has already been validated.

#ifndef GOOGLE_PROTOBUF_COMPILER_FILE_PATH_VALIDATION_H__
#define GOOGLE_PROTOBUF_COMPILER_FILE_PATH_VALIDATION_H__

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace internal {

// Validates that a .proto file path is safe to embed in generated code, build
// files, and output paths: it must be non-empty, must not contain control
// characters, backslashes, or characters that could enable code or shell
// injection, and must be a canonical relative path: no leading or trailing
// '/', no consecutive slashes, and no "." or ".." path segments.
//
// Returns true if `path` is valid. Otherwise returns false and, if `error` is
// non-null, populates it with a human-readable description of the problem.
PROTOC_EXPORT bool IsValidProtoFilePath(absl::string_view path,
                                        std::string* error);

// Applies IsValidProtoFilePath() to the names of `files` and of every file
// whose descriptor protoc may emit alongside them: their transitive imports,
// plus the direct option imports of `files` and those files' transitive
// imports (matching what protoc puts in CodeGeneratorRequests, dependency
// manifests, and descriptor sets). Option import names of every visited file
// are also checked, without loading those files. Called by protoc and plugin
// hosts before invoking any code generator, so that individual generators may
// assume file names are safe. Descriptors built from --descriptor_set_in or a
// CodeGeneratorRequest never pass through the .proto parser or DiskSourceTree,
// so this is the only check they receive.
//
// Returns true if all names are valid. Otherwise returns false and, if `error`
// is non-null, populates it with a human-readable description of the problem.
PROTOC_EXPORT bool ValidateFilePathsForCodeGeneration(
    const std::vector<const FileDescriptor*>& files, std::string* error);

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"

#endif  // GOOGLE_PROTOBUF_COMPILER_FILE_PATH_VALIDATION_H__
