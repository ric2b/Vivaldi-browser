---
breadcrumbs:
- - /developers
  - For Developers
- - /developers/design-documents
  - Design Documents
- - /developers/design-documents/sync
  - Sync
page_name: model-api
title: Chrome Sync's Model API
---

Chrome Sync operates on discrete, explicitly defined data types (bookmarks,
preferences, tabs, etc). These data types are individually responsible for
implementing their own local storage and responding to remote changes. This
guide is for developers interested in syncing data for their data type to the
cloud using Chrome Sync. It describes the newest version of the API, known as
Unified Sync and Storage (USS). There is also the deprecated [SyncableService
API] (aka Directory), which as of early 2022 is still used by several legacy
data types, but "wrapped into" USS (see [SyncableServiceBasedBridge]).

[SyncableService API]: https://www.chromium.org/developers/design-documents/sync/syncable-service-api
[SyncableServiceBasedBridge]: https://cs.chromium.org/chromium/src/components/sync/model/syncable_service_based_bridge.h

[TOC]

## Overview

To correctly sync data, USS requires that sync metadata be stored alongside your
model data in a way such that they are written together atomically. **This is
very important!** Sync must be able to update the metadata for any local data
changes as part of the same write to disk. If you attempt to write data to disk
and only notify sync afterwards, a crash in between the two writes can result in
changes being dropped and never synced to the server, or data being duplicated
due to being committed more than once.

[`DataTypeSyncBridge`][Bridge] is the interface the model code must implement.
The bridge is usually owned by a [`KeyedService`][KeyedService].
The correct place for the bridge generally lies as close to where your model
data is stored as possible, as the bridge needs to be able to inject metadata
updates into any local data changes that occur.

The bridge owns a [`DataTypeLocalChangeProcessor`][DataTypeLocalChangeProcessor]
object, which it uses to communicate local changes to sync using the `Put` and
`Delete` methods. The processor will communicate remote changes from sync to the
bridge using the `MergeFullSyncData` and `ApplyIncrementalSyncChanges` methods,
respectively for the initial merge of remote and local data, and for incremental
changes coming from sync. [`MetadataChangeList`][MCL] is the way sync
communicates metadata changes to the storage mechanism. Note that it is
typically implemented on a per-storage basis, not a per-type basis.

[Bridge]: https://cs.chromium.org/chromium/src/components/sync/model/data_type_sync_bridge.h
[KeyedService]: https://cs.chromium.org/chromium/src/components/keyed_service/core/keyed_service.h
[DataTypeLocalChangeProcessor]: https://cs.chromium.org/chromium/src/components/sync/model/data_type_local_change_processor.h
[MCL]: https://cs.chromium.org/chromium/src/components/sync/model/metadata_change_list.h

## Data

### Specifics

Data types will define a proto that contains the necessary fields of the
corresponding native type (e.g. [`ReadingListSpecifics`][ReadingListSpecifics]
contains a URL, the "read" status, and a few other things) and include it as a
field in the generic [`EntitySpecifics`][EntitySpecifics] proto. This is the
form that all communications with sync will use. This proto form of the model
data is referred to as the specifics.

[ReadingListSpecifics]: https://cs.chromium.org/chromium/src/components/sync/protocol/reading_list_specifics.proto
[EntitySpecifics]: https://cs.chromium.org/search/?q="message+EntitySpecifics"+file:sync.proto

### Identifiers

There are two primary identifiers for entities: **storage key** and **client
tag**. The bridge will need to take an [`EntityData`][EntityData] object (which
contains the specifics) and be able generate both of these from it. For
non-legacy types without significant performance concerns, these will generally
be the same.

The storage key is meant to be the primary key in the local model/database.
It’s what’s used to refer to entities most of the time and, as its name implies,
the bridge needs to be able to look up local data and metadata entries in the
store using it. Because it is a local identifier, it can change as part of
database migrations, etc. This may be desirable for efficiency reasons.

The client tag is used to generate the **client tag hash**, which will identify
entities **across clients**. This means that its implementation can **never
change** once entities have begun to sync, without risking massive duplication
of entities. This means it must be generated using only immutable data in the
specifics. If your type does not have any immutable fields to use, you will need
to add one (e.g. a GUID, though be wary as they have the potential to conflict).
While the hash gets written to disk as part of the metadata, the tag itself is
never persisted locally.

[EntityData]: https://cs.chromium.org/chromium/src/components/sync/protocol/entity_data.h

## Storage

A crucial requirement of USS is that the model must add support for keeping
sync’s metadata in the same storage as its normal data. The metadata consists of
one [`EntityMetadata`][EntityMetadata] proto for each data entity, and one
[`DataTypeState`][DataTypeState] proto containing metadata pertaining to the
state of the entire type (the progress marker, for example). This typically
requires two extra tables in a database to do (one for each type of proto).

Since the processor doesn’t know anything about the store, the bridge provides
it with an implementation of the [`MetadataChangeList`][MCL] interface. The
change processor writes metadata through this interface when changes occur, and
the bridge simply has to ensure it gets passed along to the store and written
along with the data changes.

[EntityMetadata]: https://cs.chromium.org/chromium/src/components/sync/protocol/entity_metadata.proto
[DataTypeState]: https://cs.chromium.org/chromium/src/components/sync/protocol/data_type_state.proto

### DataTypeStore

While the data type may store its data however it chooses, many types use
[`DataTypeStore`][Store], which was created specifically to provide a
convenient persistence solution. It’s backed by a [LevelDB] to store serialized
protos to disk. `DataTypeStore` provides two `MetadataChangeList`
implementations for convenience; both accessed via
[`DataTypeStore::WriteBatch`][WriteBatch]. One passes metadata changes directly
into an existing `WriteBatch` and another caches them in memory until a
`WriteBatch` exists to consume them.

The store interface abstracts away the type and will handle setting up tables
for the type’s data, so multiple `DataTypeStore` objects for different types
can share the same LevelDB backend just by specifying the same path and task
runner. Sync provides a backend that can be shared by all types via the
[`DataTypeStoreService`][StoreService].

[Store]: https://cs.chromium.org/chromium/src/components/sync/model/data_type_store.h
[LevelDB]: https://github.com/google/leveldb/blob/master/doc/index.md
[WriteBatch]: https://cs.chromium.org/search/?q="class+WriteBatch"+file:data_type_store_base.h
[StoreService]: https://cs.chromium.org/chromium/src/components/sync/model/data_type_store_service.h

## Implementing DataTypeSyncBridge

The responsibility of the bridge is to accept incoming changes from Sync and
apply them to the local model (via `MergeFullSyncData` and
`ApplyIncrementalSyncChanges`), as well as watch for local changes and send them
to Sync (via the passed-in `DataTypeLocalChangeProcessor`'s `Put` and `Delete`
methods).

### Threading

The bridge should live on the "model thread", i.e. the thread on which the local
model itself lives. This can either be a background/database thread (typically
for types which have a pre-existing persistence layer, e.g. `TypedURLSyncBridge`
or `PasswordSyncBridge`), or it can be the UI thread (e.g. `SendTabToSelfBridge`
or `ReadingListStore`). Either way, the bridge must be able to *synchronously*
handle updates from Sync. If the bridge lives on the UI thread, then the actual
persistence (e.g. to `DataTypeStore`) will be asynchronous.

### Initialization

The bridge is required to load all of the metadata for its type from storage and
provide it to the processor via the [`ModelReadyToSync`][ModelReadyToSync]
method **before any local changes occur**. This can be tricky if the thread the
bridge runs on is different from the storage mechanism. No data will be synced
with the server if the processor is never informed that the model is ready.

Since the tracking of changes and updating of metadata is completely
independent, there is no need to wait for the sync engine to start before
changes can be made.

[ModelReadyToSync]: https://cs.chromium.org/search/?q=ModelReadyToSync+file:/data_type_local_change_processor.h

### MergeFullSyncData

This method is called only once, when a type is first enabled. Sync downloads
all the data it has for the type from the server and provides it to the bridge
using this method. Sync filters out any tombstones for this call, so
`EntityChange::type()` will never be `ACTION_DELETE` for the provided entities.
The bridge must then examine the sync data and the local data and merge them
together:

*   Any remote entities that don’t exist locally must be be written to local
    storage.
*   Any local entities that don’t exist remotely must be provided to sync via
    [`DataTypeLocalChangeProcessor::Put`][Put].
*   Any entities that appear in both sets must be merged and the model and sync
    informed accordingly. Decide which copy of the data to use (or a merged
    version or neither) and update the local store and sync as necessary to
    reflect the decision. How the decision is made can vary by data type.

The [`MetadataChangeList`][MCL] passed into the function is already populated
with metadata for all the data passed in (note that neither the data nor the
metadata have been committed to storage yet at this point). It must be given to
the processor for any `Put` or `Delete` calls so the relevant metadata can be
added/updated/deleted, and then passed to the store for persisting along with
the data.

Note that if sync gets disabled and the metadata cleared, entities that
originated from other clients will exist as “local” entities the next time sync
starts and merge is called. Since tombstones are not provided for merge, this
can result in reviving the entity if it had been deleted on another client in
the meantime.

[Put]: https://cs.chromium.org/search/?q=Put+file:/data_type_local_change_processor.h

### ApplyIncrementalSyncChanges

This method is called whenever new changes have been downloaded from the server.
These changes must be applied to the local model. See also
[Sync Best Practices][SyncBestPractices] for more details.

Here’s an example implementation of a type using `DataTypeStore`:

```cpp
std::optional<ModelError> DeviceInfoSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE:
        batch->WriteData(change->storage_key(),
                         change->data().specifics.your_type().SerializeAsString());
        break;
      case syncer::EntityChange::ACTION_DELETE:
        batch->DeleteData(change->storage_key());
        break;
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(std::move(batch), base::BindOnce(...));
  NotifyModelOfChanges();
  return {};
}
```

Note that it is recommended to apply remote changes to the local model as is
without custom conflict detection, and merging remote and local data should also
be avoided.

A conflict can occur when an entity has a pending local commit when an update
for the same entity comes from another client. In this case, the bridge’s
[`ResolveConflict`][ResolveConflict] method will have been called prior to the
`ApplyIncrementalSyncChanges` call in order to determine what should happen.
This method defaults to having the remote version overwrite the local version
unless the remote version is a tombstone, in which case the local version wins.

[EntityChange]: https://cs.chromium.org/chromium/src/components/sync/model/entity_change.h
[ResolveConflict]: https://cs.chromium.org/search/?q=ResolveConflict+file:/data_type_sync_bridge.h
[SyncBestPractices]: /developers/design-documents/sync/sync-data-best-practices/

### ApplyDisableSyncChanges

This method is called when sync is being disabled and the sync metadata needs to
cleared. The data type can choose to delete the data but **the metadata should
always be deleted** when this method is called. This means that the passed
`MetadataChangeList` must always be deleted from the storage (for example using
`ApplyIncrementalSyncChang(metadata_change_list)` as in default implementation),
if the store is not just fully erased.

### Local changes

The [`DataTypeLocalChangeProcessor`][DataTypeLocalChangeProcessor] must be
informed of any local changes via its `Put` and `Delete` methods. Since the
processor cannot do any useful metadata tracking until `MergeFullSyncData` is
called, the `IsTrackingMetadata` method is provided. It can be checked as an
optimization to prevent unnecessary processing preparing the parameters to a
`Put` or `Delete` call.

Here’s an example of handling a local write using `DataTypeStore`:

```cpp
void WriteLocalChange(std::string key, ModelData data) {
  std::unique_ptr<WriteBatch> batch = store_->CreateWriteBatch();
  if (change_processor()->IsTrackingMetadata()) {
    change_processor()->Put(key, ModelToEntityData(data),
                            batch->GetMetadataChangeList());
  }
  batch->WriteData(key, data.specifics->SerializeAsString());
  store_->CommitWriteBatch(std::move(batch), base::BindOnce(...));
}
```

### Forward Compatibility

Any new fields in a proto are unrecognized by older clients. Thus, any such
changes face an inherent risk of leading to data loss for a multi-client Sync
user. Refer to
[Protection against data override by old Sync clients](../old-sync-clients-data-override-protection)
for details on the problem and the solution.

### Unique Positions

Data types which require ordering may use unique positions to sync the order of
entities across devices. See [Unique Positions][UniquePositions] for the details
on how to use them in the bridge.

[UniquePositions]: /developers/design-documents/sync/unique-positions

## Error handling

If any errors occur during store operations that could compromise the
consistency of the data and metadata, the processor’s
[`ReportError`][ReportError] method should be called. The only exception to this
is errors during `MergeFullSyncData` or `ApplyIncrementalSyncChanges`, which
should just return a [`ModelError`][ModelError].

This will inform sync of the error, which will stop all communications with the
server so bad data doesn’t get synced. Since the metadata might no longer be
valid, the bridge will asynchronously receive an `ApplyStopSyncChanges` call
with a non-null `MetadataChangeList` parameter. All the metadata will be cleared
from the store (if possible), and the type will be started again from scratch on
the next client restart.

[ReportError]: https://cs.chromium.org/search/?q=ReportError+file:/data_type_local_change_processor.h
[ModelError]: https://cs.chromium.org/chromium/src/components/sync/model/model_error.h

## Sync Integration Checklist

This content was moved [here](../integration-checklist).

## Testing

### Manual testing / debugging

The `chrome://sync-internals` debugging page contains lots of information about
Sync's inner workings, and is often useful while developing. Of particular
interest is the "Sync Protocol Log" aka "Traffic Log", which shows all the
messages being sent between Chrome and the Sync server. If you tick the
"Capture Specifics" checkbox, or run Chrome with the `--sync-include-specifics`
command line param, then you can inspect the full protos being sent back and
forth.

### Automated testing

In addition to the usual unit tests, sync data types should be covered by
integration tests based on [`SyncTest`][SyncTest]. These are similar to
`browser_tests`, but live in a separate binary (`sync_integration_tests`)
and run against a fake sync server built into Chrome (and optionally also
against the real server, see macro `E2E_ENABLED`).

They come in two variants, single-client or two-client tests. Single-client
tests run a single syncing client against the fake sync server. Two-client
tests run two clients, syncing to the same account, against the fake server.

In many cases, single-client tests are sufficient to cover all relevant
scenarios, but sometimes two-client tests are required to cover some tricky
cases. There are [plenty of examples][integration_test_examples] for both in
the code base.

[SyncTest]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/sync/test/integration/sync_test.h
[integration_test_examples]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/sync/test/integration/
