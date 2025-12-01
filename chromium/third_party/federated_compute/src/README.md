# Federated Compute Platform

This repository hosts code for executing federated programs and computations.

## Definitions

A *federated computation* is a set of processing steps that run on a server and
set of clients, where each step is either

*   local processing of values on the client or server or
*   transport which moves values via
    *   broadcast (server-to-clients)
    *   select (client-server response)
    *   aggregate (clients-to-server). A federated computation invoked by a
        central coordinator returns one or more aggregates.

A *federated program* is a set of processing steps run by a central coordinator
that include one or more invocations of federated computation, transformations
of computation results, and releases of aggregate values to the engineer/analyst
who invoked the program.

To learn more about these concepts, check out:

-   [Federated learning comic book from Google AI](http://g.co/federated)
-   [Federated Learning: Collaborative Machine Learning without Centralized
    Training
    Data](https://ai.googleblog.com/2017/04/federated-learning-collaborative.html)
-   [Federated Analytics: Collaborative Data Science without Data Collection](https://ai.googleblog.com/2020/05/federated-analytics-collaborative-data.html)
-   [Towards Federated Learning at Scale: System Design](https://arxiv.org/abs/1902.01046)
    (SysML 2019)
-   [Federated Program API in TFF](https://github.com/tensorflow/federated/blob/main/tensorflow_federated/python/program/README.md)

## Infrastructure

At Google, federated programs and computations are authored in
[TensorFlow Federated](http://tensorflow.org/federated), compiled to deployable
artifacts, and run in a distributed system consisting of a central coordinator,
and a set of devices such as phones. The TFF repository contains infrastructure
for authoring and simulating federated programs and computations.

This repository hosts infrastructure for compiling and running federated
programs and computations in the cross-device setting. We are actively working
on open sourcing the core components of our production infrastructure, with a
focus on privacy-sensitive code-paths such as the pipeline for compiling
deployable artifacts from TFF computations, client-side processing, and
server-side aggregation logic.

As of 12/7/2022, parts of the repository - in particular, code in the `client/`
directory, and the service & data format definitions in `proto/` - are used in
production in Google's federated learning infrastructure. Other parts - notably,
production server side infrastructure - have not yet been open sourced due to
its dependencies on proprietary infrastructure, and we instead provide a
reference / example server implementation in `demo/` for demonstration purposes.

The best way to get started is to run the end-to-end demo
`//fcp/demo:federated_program_test`, which will spin up example services,
clients, and run a federated program; this test will cover the majority of the
code in this repository.

## Remote attestation of server-side, TEE-hosted applications

As described in the
[Confidential Federated Computations paper](https://arxiv.org/abs/2404.10764),
this repository hosts a client-side implementation of the
`ConfidentialAggregations` protocol, which uses remotely attested server-side
applications that run in a Trusted Execution Environment (TEE).

Clients using this library and which participate in this protocol will verify
attestation evidence for the ledger application hosted in the
[Confidential Federated Compute](https://github.com/google-parfait/confidential-federated-compute)
repository. They will also verify the data access policy that the ledger will
enforce, which will specify one or more allowed data transformation applications
which are built from that same repository.

Please see [fcp/client/attestation/README.md](fcp/client/attestation/README.md)
for more details on this process.

## Getting Started

Please refer to the instructions in GETTING_STARTED.md.

## Disclaimer

This is not an officially supported Google product.
