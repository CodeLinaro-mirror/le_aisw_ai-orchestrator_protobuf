"""Tests for output directory of the C++ proto compile action."""

load("@rules_testing//lib:truth.bzl", "matching")
load("//bazel/tests/cc_proto_library_tests:test_utils.bzl", "get_cc_info_artifacts")

def _test_output_directory_for_proto_compile_action(env, target):
    artifacts = get_cc_info_artifacts(target)
    h_file = [
        f
        for f in artifacts.compilation_context_headers_in_package
        if f.basename == "bar.pb.h"
    ][0]
    action = env.expect.that_target(target).action_generating("{package}/bar.pb.h")
    action.argv().contains_predicate(
        matching.any(
            matching.str_matches("--cpp_out=" + h_file.root.path),
            matching.str_matches("--cpp_out=*:" + h_file.root.path),
        ),
    )

TESTS = {
    ":bar_proto": [_test_output_directory_for_proto_compile_action],
}
