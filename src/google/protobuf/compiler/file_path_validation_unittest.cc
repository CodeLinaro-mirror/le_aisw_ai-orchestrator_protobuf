// Protocol Buffers - Google's data interchange format
// Copyright 2026 Google Inc.  All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include "google/protobuf/compiler/file_path_validation.h"

#include <string>
#include <vector>

#include "google/protobuf/descriptor.pb.h"
#include <gtest/gtest.h>
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/descriptor.h"

// Must be included last.
#include "google/protobuf/port_def.inc"

namespace google {
namespace protobuf {
namespace internal {
namespace {

TEST(IsValidProtoFilePathTest, AcceptsOrdinaryPaths) {
  std::string error;
  EXPECT_TRUE(IsValidProtoFilePath("foo.proto", &error));
  EXPECT_TRUE(IsValidProtoFilePath("foo/bar-baz_1.2/qux.proto", &error));
  EXPECT_TRUE(IsValidProtoFilePath("foo/bar/qux.proto", nullptr));
  EXPECT_TRUE(IsValidProtoFilePath("foo..bar.proto", &error));
  EXPECT_TRUE(IsValidProtoFilePath("foo/..bar/baz.proto", &error));
  EXPECT_TRUE(IsValidProtoFilePath("foo/bar../baz.proto", &error));
  EXPECT_EQ(error, "");
}

TEST(IsValidProtoFilePathTest, RejectsEmptyPath) {
  std::string error;
  EXPECT_FALSE(IsValidProtoFilePath("", &error));
  EXPECT_EQ(error, "File path cannot be empty.");
  EXPECT_FALSE(IsValidProtoFilePath("", nullptr));
}

TEST(IsValidProtoFilePathTest, RejectsControlCharacters) {
  std::string error;
  EXPECT_FALSE(IsValidProtoFilePath("foo\n.proto", &error));
  EXPECT_EQ(error, "Invalid control character 0x0a in file path.");
  EXPECT_FALSE(
      IsValidProtoFilePath(absl::string_view("foo\0.proto", 10), &error));
  EXPECT_EQ(error, "Invalid control character 0x00 in file path.");
  EXPECT_FALSE(IsValidProtoFilePath("foo\x7f.proto", &error));
  EXPECT_EQ(error, "Invalid control character 0x7f in file path.");
}

TEST(IsValidProtoFilePathTest, RejectsBannedCharacters) {
  std::string error;
  EXPECT_FALSE(IsValidProtoFilePath("foo;bar.proto", &error));
  EXPECT_EQ(error, "Invalid character ';' in file path.");
  EXPECT_FALSE(IsValidProtoFilePath("foo\\bar.proto", &error));
  EXPECT_EQ(error, "Invalid character '\\' in file path.");
  EXPECT_FALSE(IsValidProtoFilePath("foo'bar.proto", &error));
  EXPECT_EQ(error, "Invalid character \"'\" in file path.");
  EXPECT_FALSE(IsValidProtoFilePath("foo$bar.proto", nullptr));
}

TEST(IsValidProtoFilePathTest, RejectsParentReferences) {
  for (absl::string_view path :
       {"..", "../foo.proto", "foo/../bar.proto", "foo/.."}) {
    std::string error;
    EXPECT_FALSE(IsValidProtoFilePath(path, &error)) << path;
    EXPECT_EQ(error, "File path must not contain \"..\" path segments.");
  }
}

TEST(IsValidProtoFilePathTest, RejectsCurrentDirectoryReferences) {
  for (absl::string_view path :
       {".", "./foo.proto", "foo/./bar.proto", "foo/."}) {
    std::string error;
    EXPECT_FALSE(IsValidProtoFilePath(path, &error)) << path;
    EXPECT_EQ(error, "File path must not contain \".\" path segments.");
  }
  // Dots within a segment are fine.
  EXPECT_TRUE(IsValidProtoFilePath(".foo.proto", nullptr));
  EXPECT_TRUE(IsValidProtoFilePath("foo/.bar/baz.proto", nullptr));
}

TEST(IsValidProtoFilePathTest, RejectsAbsoluteAndNonCanonicalPaths) {
  for (absl::string_view path :
       {"/", "/tmp/evil.proto", "foo/", "foo//bar.proto", "//foo.proto"}) {
    std::string error;
    EXPECT_FALSE(IsValidProtoFilePath(path, &error)) << path;
    EXPECT_EQ(error,
              "File path must be relative and must not contain empty path "
              "segments.");
  }
}

class ValidateFilePathsTest : public ::testing::Test {
 protected:
  const FileDescriptor* AddFile(absl::string_view name,
                                std::vector<std::string> deps = {},
                                std::vector<std::string> option_deps = {}) {
    FileDescriptorProto proto;
    proto.set_name(name);
    for (auto& dep : deps) proto.add_dependency(dep);
    if (!option_deps.empty()) {
      proto.set_syntax("editions");
      proto.set_edition(EDITION_2024);
      for (auto& dep : option_deps) proto.add_option_dependency(dep);
    }
    const FileDescriptor* file = pool_.BuildFile(proto);
    ABSL_CHECK(file != nullptr) << name;
    return file;
  }

  std::string Validate(std::vector<const FileDescriptor*> files) {
    std::string error;
    if (ValidateFilePathsForCodeGeneration(files, &error)) {
      return "OK";
    }
    return error;
  }

  DescriptorPool pool_;
};

TEST_F(ValidateFilePathsTest, AcceptsValidTransitiveClosure) {
  AddFile("a.proto");
  AddFile("b.proto", {"a.proto"});
  EXPECT_EQ(Validate({AddFile("c.proto", {"a.proto", "b.proto"})}), "OK");
}

TEST_F(ValidateFilePathsTest, RejectsInvalidFileName) {
  EXPECT_EQ(Validate({AddFile("foo\nbar.proto")}),
            "foo\\nbar.proto: Invalid control character 0x0a in file path.");
}

TEST_F(ValidateFilePathsTest, RejectsInvalidDirectImport) {
  AddFile("../../evil.proto");
  EXPECT_EQ(Validate({AddFile("main.proto", {"../../evil.proto"})}),
            "main.proto: Import \"../../evil.proto\": File path must not "
            "contain \"..\" path segments.");
}

TEST_F(ValidateFilePathsTest, RejectsInvalidTransitiveImport) {
  AddFile("evil';.proto");
  AddFile("middle.proto", {"evil';.proto"});
  EXPECT_EQ(Validate({AddFile("main.proto", {"middle.proto"})}),
            "middle.proto: Import \"evil\\';.proto\": Invalid character "
            "\"'\" in file path.");
}

TEST_F(ValidateFilePathsTest, RejectsInvalidImportOfOptionImport) {
  // protoc emits option imports of the files being generated, and their
  // transitive imports, into plugin requests and dependency manifests.
  AddFile("evil;.proto");
  AddFile("opt.proto", {"evil;.proto"});
  EXPECT_EQ(Validate({AddFile("main.proto", {}, {"opt.proto"})}),
            "opt.proto: Import \"evil;.proto\": Invalid character ';' in "
            "file path.");
}

TEST_F(ValidateFilePathsTest, RejectsInvalidOptionImportName) {
  EXPECT_EQ(Validate({AddFile("main.proto", {}, {"../opt.proto"})}),
            "main.proto: Option import \"../opt.proto\": File path must not "
            "contain \"..\" path segments.");

  // Option import names of non-top-level files are checked too, even though
  // those files are never loaded.
  AddFile("dep.proto", {}, {"bad$.proto"});
  EXPECT_EQ(Validate({AddFile("main2.proto", {"dep.proto"})}),
            "dep.proto: Option import \"bad$.proto\": Invalid character '$' "
            "in file path.");
}

TEST_F(ValidateFilePathsTest, DoesNotFollowNestedOrMissingOptionImports) {
  // Only the direct option imports of the given files are emitted by protoc,
  // so files reached through nested option imports are not inspected.
  AddFile("evil;.proto");
  AddFile("deep.proto", {"evil;.proto"});
  AddFile("opt.proto", {}, {"deep.proto"});
  EXPECT_EQ(
      Validate({AddFile("main.proto", {}, {"opt.proto", "missing.proto"})}),
      "OK");
}

}  // namespace
}  // namespace internal
}  // namespace protobuf
}  // namespace google

#include "google/protobuf/port_undef.inc"
