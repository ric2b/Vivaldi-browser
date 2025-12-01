# Federated Computations Client

This directory contains the portable client implementation of Google's platform
for federated and local computations. A final build of the client will consist
of

1.  The portable core functionality provided by this directory - `:fl_runner`
    and`:lc_runner` for running federated and local computations. This code
    contains the network stack and model / query interpreter.
1.  Platform-dependent implementations of the `:interfaces` target. This allows
    to inject dependencies for e.g. telemetry, attestation, flag-guarding,
    access to example stores etc.

The stand-alone binary `:client_runner_main` provides a bare bones example of a
federated computation client. Most practical implementations will wrap the calls
to the client in a scheduler that respects device constraints and the returned
retry window.
