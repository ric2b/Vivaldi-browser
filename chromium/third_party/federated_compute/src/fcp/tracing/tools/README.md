# Tracing Traits Generator

This directory contains an implementation of a simple codegen tool
`tracing_traits_generator` which uses FlatBuffers reflection to inspect
user-defined tracing schema and produces a C++ header file containing additional
traits needed for `TracingSpan::Log<T>()` and `TracingSpan::CreateChild<T>()` to
function properly.

These traits allow for compile-time lookup of user-defined tags, additional
attributes needed for tracking backend components to handle the data.

This tool is automatically invoked during a build process with the help of
`tracing_schema_cc_library` rule in `build/build_defs.bzl`.

Examples of codegen output for various input FlatBuffers schemas can be found in
`testdata/*.baseline` files.
