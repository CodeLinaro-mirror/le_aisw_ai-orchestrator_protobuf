// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/file_path_validation.h"

#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/charset.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace internal {

bool IsValidProtoFilePath(absl::string_view path, std::string* error) {
  if (path.empty()) {
    if (error != nullptr) {
      *error = "File path cannot be empty.";
    }
    return false;
  }
  // Characters that could break out of comments, string literals, or shell
  // commands if embedded in generated code or build files.
  static constexpr absl::CharSet kBannedPathChars =
      absl::CharSet("'\"`;#$<>|*?:&(){}[]!%^\\");
  for (char c : path) {
    unsigned char uc = static_cast<unsigned char>(c);
    if (uc < 0x20 || uc == 0x7F) {
      if (error != nullptr) {
        *error = absl::StrCat("Invalid control character 0x",
                              absl::Hex(uc, absl::kZeroPad2), " in file path.");
      }
      return false;
    }
    if (kBannedPathChars.contains(c)) {
      if (error != nullptr) {
        if (c == '\'') {
          *error = "Invalid character \"'\" in file path.";
        } else {
          *error = absl::StrCat("Invalid character '", absl::string_view(&c, 1),
                                "' in file path.");
        }
      }
      return false;
    }
  }
  // Reject empty segments (leading '/', trailing '/', or "//"), which would
  // make the path absolute or non-canonical, "." segments, and ".." segments
  // (but not e.g. "foo..bar.proto"), which could escape the output directory
  // or reference files outside the source tree. This matches the canonical
  // form required by GeneratorContext::Open() and
  // DiskSourceTree::OpenVirtualFile().
  for (absl::string_view segment : absl::StrSplit(path, '/')) {
    if (segment.empty()) {
      if (error != nullptr) {
        *error =
            "File path must be relative and must not contain empty path "
            "segments.";
      }
      return false;
    }
    if (segment == ".") {
      if (error != nullptr) {
        *error = "File path must not contain \".\" path segments.";
      }
      return false;
    }
    if (segment == "..") {
      if (error != nullptr) {
        *error = "File path must not contain \"..\" path segments.";
      }
      return false;
    }
  }
  return true;
}

bool ValidateFilePathsForCodeGeneration(
    const std::vector<const FileDescriptor*>& files, std::string* error) {
  std::string reason;
  absl::flat_hash_set<const FileDescriptor*> seen;
  std::vector<const FileDescriptor*> to_visit;
  auto visit = [&](const FileDescriptor* file) {
    if (seen.insert(file).second) to_visit.push_back(file);
  };
  for (const FileDescriptor* file : files) {
    if (!IsValidProtoFilePath(file->name(), &reason)) {
      if (error != nullptr) {
        *error = absl::StrCat(absl::CEscape(file->name()), ": ", reason);
      }
      return false;
    }
    visit(file);
  }

  // Every other reachable file is validated as the import of a visited file,
  // so that the error message can name the importing file.
  auto check_import = [&](const FileDescriptor* file, absl::string_view kind,
                          absl::string_view name) {
    if (IsValidProtoFilePath(name, &reason)) return true;
    if (error != nullptr) {
      *error = absl::StrCat(absl::CEscape(file->name()), ": ", kind, " \"",
                            absl::CEscape(name), "\": ", reason);
    }
    return false;
  };

  // Mirror the set of files that protoc emits into CodeGeneratorRequests,
  // dependency manifests, and descriptor sets (see
  // CommandLineInterface::GetTransitiveOptionDependencies): option imports of
  // `files` are followed one level, together with their regular imports. Option
  // imports of other files are never emitted, and may legitimately be missing
  // (looking them up would report an error), so only their names are checked.
  for (const FileDescriptor* file : files) {
    for (int i = 0; i < file->option_dependency_count(); ++i) {
      absl::string_view name = file->option_dependency_name(i);
      if (!check_import(file, "Option import", name)) return false;
      const FileDescriptor* dep = file->pool()->FindFileByName(name);
      if (dep != nullptr) visit(dep);
    }
  }

  while (!to_visit.empty()) {
    const FileDescriptor* file = to_visit.back();
    to_visit.pop_back();
    for (int i = 0; i < file->dependency_count(); ++i) {
      const FileDescriptor* dep = file->dependency(i);
      if (dep == nullptr) continue;
      if (!check_import(file, "Import", dep->name())) return false;
      visit(dep);
    }
    for (int i = 0; i < file->option_dependency_count(); ++i) {
      if (!check_import(file, "Option import", file->option_dependency_name(i)))
        return false;
    }
  }
  return true;
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"
