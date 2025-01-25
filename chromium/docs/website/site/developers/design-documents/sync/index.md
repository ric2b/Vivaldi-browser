---
breadcrumbs:
- - /developers
  - For Developers
- - /developers/design-documents
  - Design Documents
page_name: sync
title: Sync
---

**Docs**

[Sync integration checklist](/developers/design-documents/sync/integration-checklist)

[Model API](/developers/design-documents/sync/model-api)

[ClientTagBasedDataTypeProcessor](/developers/design-documents/sync/client-tag-based-data-type-processor)

[Protection against data override by legacy clients](/developers/design-documents/sync/old-sync-clients-data-override-protection)

[Sync diagnostics](/developers/design-documents/sync/diagnostics)

[Sync Data Best
Practices](/developers/design-documents/sync/sync-data-best-practices)

[Unified Sync And Storage
proposal](/developers/design-documents/sync/unified-sync-and-storage-overview)
 (Outdated, kept for historical purposes)

***What***

A library that implements the client side of our sync protocol, as well as the
Google server-side infrastructure to serve Google Chrome users and synchronize
data to their Google Account.

The goals for this protocol include:

*   Store/sync different kinds of "data types", e.g. bookmarks.
*   Allow the user to connect to the server from multiple clients
            simultaneously.
*   Changes the user makes in one client should be immediately reflected
            in other clients connected to the server.
*   Allow the user to make changes to their data even if the server is
            unreachable, such that changes made while offline are synced with
            the server at a later time.
*   Resolve data conflicts on the client without prompting the user.

***Where***

Most sync code (except for UI and integration tests) lives in
`components/sync/`. Some of the more important subfolders are:
*   `base`: Various "util" stuff; doesn't depend on any other sync folders.
*   `engine`: The core sync "engine", i.e. the parts that run on the sync
    sequence, talk to the server, and propagate changes to and from the various
    data types.
*   `invalidations`: The subsystem responsible for handling invalidations, i.e.
    messages from the server informing the client that new data is available.
    Implemented as a layer on top of FCM.
*   `model`: Contains the APIs that each data type needs to implement, and sync
    classes that live on the model sequence.
*   `protocol`: Contains the `.proto` definitions and some related utils.
*   `service`: Contains the central `SyncService`, and most other sync stuff
    that runs on the UI thread.
*   `test`: Various test doubles and utils.

Dependencies / layering:
*   `service` is the highest layer; it depends on everything below, and most
    external code depends on this, e.g. to query the sync status.
*   `model` is the layer below; all data type implementations depend on this.
*   `engine` is even lower and mostly internal - external code should generally
    not (directly) depend on this.
*   `protocol` is depended on by most other sync code, and by all data type
    implementations.
*   `base` is the lowest layer; everything else may depend on this.

Additionally, there is `components/browser_sync/`, which pulls together dependencies from most data types in other `components/` subfolders, and
injects them into core sync in the form of `DataTypeController`s.
