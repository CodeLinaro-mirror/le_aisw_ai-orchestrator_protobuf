"""Tests that strict deps is enabled when compiling proto generated Java code."""

def _get_direct_dependencies(argv):
    direct_deps = []
    in_direct_deps = False
    for arg in argv:
        if arg == "--direct_dependencies":
            in_direct_deps = True
            continue
        if in_direct_deps:
            if arg.startswith("--"):
                in_direct_deps = False
            else:
                direct_deps.append(arg)
    return direct_deps

def _test_generated_code_is_compiled_with_strict_deps(env, target):
    action = env.expect.that_target(target).action_generating("{package}/libfoo_proto-speed.jar")
    action.argv().contains_at_least([
        "--direct_dependencies",
        "{bindir}/{package}/libbar_proto-speed-hjar.jar",
    ]).in_order()

    action.argv().contains("{bindir}/{package}/libbaz_proto-speed-hjar.jar")

    direct_deps = _get_direct_dependencies(action.actual.argv)
    in_pkg_direct_deps = [d for d in direct_deps if target.label.package in d]
    env.expect.that_collection(in_pkg_direct_deps).contains_exactly([
        action.meta.format_str("{bindir}/{package}/libbar_proto-speed-hjar.jar"),
    ])
    runtime_direct_deps = [d for d in direct_deps if target.label.package not in d]
    env.expect.that_collection(runtime_direct_deps).is_not_empty()

TESTS = {
    ":foo_proto": [_test_generated_code_is_compiled_with_strict_deps],
}
