"""A rule to textually replace {{VERSION}} with the Rust release version in files."""

load("//:protobuf_version.bzl", "PROTOBUF_GOOGLE_RUST_VERSION", "PROTOBUF_RUST_VERSION")

def substitute_rust_release_version(src, out, is_new_crate = False, name = None):
    version = PROTOBUF_GOOGLE_RUST_VERSION if is_new_crate else PROTOBUF_RUST_VERSION
    native.genrule(
        name = name or ("gen_%s" % out),
        srcs = [src],
        outs = [out],
        cmd = "cat $(SRCS) | sed -e 's/{{VERSION}}/{0}/g' -e 's/{{PROTOBUF_VERSION}}/{1}/g' > $(OUTS)".format(version, PROTOBUF_RUST_VERSION),
    )
