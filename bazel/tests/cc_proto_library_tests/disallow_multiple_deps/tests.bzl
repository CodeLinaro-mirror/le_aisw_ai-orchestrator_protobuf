"""Tests that cc_proto_library disallows zero or multiple deps."""

load("@rules_testing//lib:truth.bzl", "matching")

def _test_multiple_deps(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.any(
            matching.str_matches("*'deps' attribute must contain exactly one label*"),
            matching.str_matches("*more than one deps attribute found; expected only one.*"),
        ),
    )

def _test_empty_deps(env, target):
    env.expect.that_target(target).failures().contains_predicate(
        matching.any(
            matching.str_matches("*'deps' attribute must contain exactly one label*"),
            matching.str_matches("*no deps attribute found; expected one.*"),
        ),
    )

TESTS = {
    ":multiple_deps_cc_proto": [_test_multiple_deps],
    ":empty_deps_cc_proto": [_test_empty_deps],
}
