# Updating boringssl

1. Roll the dependency following the [instructions](../../docs/roll_deps.md).
2. Run `gclient sync` to sync local copy of boringssl with upstream.
3. Attempt to build locally.  Often adjustments to our `BUILD.gn` must also be
   made.  Look at the Chromium copy of the [boringssl BUILD.gn](https://chromium.googlesource.com/chromium/src/+/refs/heads/main/third_party/boringssl/BUILD.gn)
   for hints.
4. Attempt a tryjob.
