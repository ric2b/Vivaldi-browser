
When building libraries (such as custom op libraries) against the TensorFlow pip
package, care must be taken to ensure those libraries build against that
package's headers and with the same compiler and linker flags as that package
was compiled with. These utilities help ensure that's the case.

This package assumes Tensorflow is available in the `@pypi_tensorflow` package,
with the additional build content specified in `TF_ADDITIVE_BUILD_CONTENT`:

```
load("@com_google_fcp//tensorflow/pip_tf:defs.bzl", "TF_ADDITIVE_BUILD_CONTENT")

pip_parse(
    name = "pypi",
    annotations = {
        "tensorflow": package_annotation(
            additive_build_content = TF_ADDITIVE_BUILD_CONTENT,
        ),
    },
    ...
)
```

NOTE: The `gpu_srcs` and `gpu_deps` parameters supported by TensorFlow's version
of `tf_custom_op_library` are not supported by this version.
