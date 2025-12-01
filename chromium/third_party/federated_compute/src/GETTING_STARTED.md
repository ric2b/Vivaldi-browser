# Instructions for getting the Federated Compute Platform code up and running on your own machine.

## Download and install build dependencies

### Basic tools

There are some basic tools and packages you will need on your machine:

*   Git
*   Java Development Kit (JDK)
*   Python 3
*   libstdc++

For example, on Debian:

```
sudo apt install -y git default-jdk-headless python3 libstdc++-10-dev
```

> ⚠️ The project maintainers internally test with Clang only, so support for
> GCC-based builds is provided only on a best-effort basis and may at times be
> broken.
>
> If using GCC then we recommend using a recent version (e.g., at least as
> recent as what Debian stable uses, preferably newer than that).

### Install Bazelisk

Bazelisk is used to fetch the correct Bazel binaries necessary to build and run
Federated Compute code.

Please read https://github.com/bazelbuild/bazelisk#installation.

## Build and run the federated program test!

> ⚠️ Many Federated Compute targets depend on TensorFlow, which can take several
> hours to build for the first time. Consider running builds in `screen` or
> `tmux` if you're worried about your terminal closing during this time.
>
> While not required, Bazel's
> [remote build execution](https://bazel.build/remote/rbe) and
> [remote caching](https://bazel.build/remote/caching) features can speed up
> builds.

```
bazelisk test //fcp/demo:federated_program_test
```
