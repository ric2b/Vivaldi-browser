---
breadcrumbs:
- - /developers
  - For Developers
- - /developers/design-documents
  - Design Documents
- - /developers/design-documents/sync
  - Sync
page_name: unique-positions
title: Chrome Sync Unique Position API
---

### Overview

Some features integrating with Sync, such as Bookmarks and Tabs, require some
notion of ordering between entities, which is expected to sync across devices.

The recommend way to achieve this by leaning on the concept of 'unique position'
which is both a protocol buffer message as well as a C++ class. The data type's
implementation (usually bridge) becomes responsible for exercising APIs to deal
with position changes, both local and remote.

Adopting unique positions has the following advantages over more naive
approaches such as using numerical positions:
* Unique positions are representing a position relative to each other rather
  than the target index.
* It's possible to generate unique position between any elements which would be
  difficult to achieve using integers.
* Unique positions require only one element to be committed to the server if
  only one item is being moved. With integers (index of item in the model) it
  might require uploading up to all of the elements if all their indexes were
  updated (e.g. if an item was moved to the beginning). This reduces the traffic
  to the server and also decreases the likelihood of conflicts.

Unique positions are stored locally in sync metadata and the model can keep
items in memory in any convenient format (e.g. in a vector). The bridge should
take care of transforming model's positions / indexes into unique positions.
Sync processor will update unique positions in metadata during processing local
changes or remote updates.

Note that not all sync entities are required to have unique positions, only
those which need to be ordered (e.g. tab groups may not have positions while
tabs within them should be ordered against each other).

### How to enable unique positions

1. Add `sync_pb::UniquePosition` to data type specifics.

1. Implement the following methods in the bridge to tell the processor that the
bridge supports unique positions and to extract them from specifics:

    ```cpp
    class Bridge : public DataTypeSyncBridge {
      // ...

      // Tell the processor that unique positions are enabled for this bridge.
      bool SupportsUniquePositions() const override { return true; }

      // Extracts unique position from data type specifics. Returns default
      // (empty) UniquePosition if entity does not require ordering.
      sync_pb::UniquePosition GetUniquePosition(
          const sync_pb::EntitySpecifics& specifics) const override {
        return specifics.data_type_specifics().unique_position();
      }
    };
    ```

1. Implement generating unique positions using
[processor API][UniquePositionAPI] and populating it in specifics
([example CL](https://crrev.com/c/5769238)).

1. Implement applying remote updates. Example CLs:
    * https://crrev.com/c/5768675 - basic implementation of applying remote
        updates, requires the following CL to work properly.
    * https://crrev.com/c/5842231 - full implementation of applying remote
        updates.

    This step is complicated because of the mental model which is slightly
    different from the normal updates and it requires reviewers to be thorough.
    It's recommended to add a reviewer from the Sync team and to have extensive
    unit-testing.

    Sync metadata changes are applied in the processor **before**
    `ApplyIncrementalSyncChanges` is called. This means that all the previous
    data about unique positions is gone, and the processor represents the final
    state which should be represented in the model. It leads to the difference
    between the order in the processor, and in the model. The second CL above
    resolves this inconsistency. See `Applying remote updates` below for more
    details.

1. Add two-client sync integration tests to verify that the eventual state of
both clients is the same, especially in case changes are happening on both
clients simultaneously.

### Applying remote updates

For types supporting unique positions, applying remote updates is hard to get
right. This section covers the recommended approaches and the various pitfalls.

#### Recommended approach

1. Sort incoming updates by their unique positions in the reversed order (from
right to left).

1. Track entities being updated in the model.

1. Apply updates one by one. When finding the position to put an updated item to
the model, skip the entities which are being updated (and not applied yet).

  * To find the position in the model for an updated item, iterate over the
    model and compare item's unique position with the updated entity (skipping
    items being updated). The updated entity should be placed before the first
    item with a greater unique position.

Example (excluding unrelated details, see example CLs for the full code):
```cpp
void Bridge::ApplyIncrementalSyncChanges(
        syncer::EntityChangeList entity_changes) {
  // Step 1 - sort incoming changes in the reversed order.
  ranges::sort(entity_changes, &UniquePositionComparison);
  ranges::reverse(entity_changes);

  // Step 2 - track entities being updated.
  std::set<std::string> storage_keys_to_update;
  for (auto& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_UPDATE) {
      storage_keys_to_update.insert(change->storage_key());
    }
  }

  // Step 3 - apply updates.
  for (auto& change : entity_changes) {
    ApplyUpdateToModel(change, storage_keys_to_update);
    storage_keys_to_update.erase(change->storage_key());
  }
}

void ApplyUpdateToModel(const syncer::EntityChange& change,
                        const std::set<std::string>& storage_keys_to_update) {
  // Find the place to insert an updated or added item.
  Model::Item* insert_before = nullptr;
  for (const Model::Item& item: model_->items()) {
    if (storage_keys_to_update.contains(item.key())) {
      continue;
    }
    UniquePosition position_for_change = UniquePositionFromChange(change);
    UniquePosition position_for_item = Parse(processor()->GetUniquePositionForStorageKey(item.key()));
    if (position_for_change.LessThan(position_for_item)) {
      insert_before = &item;
      break;
    }
  }

  model_->UpdateAndMoveItem(insert_before, ItemFromChange(change));
}
```

#### Alternative approach

A simpler but still correct solution could be to apply updates keeping the
existing order of items in the model. New items could be placed to the end by
default. In the end of `ApplyIncrementalSyncChanges` method, the bridge may use
unique positions from the processor to order items in the model (as was
mentioned above, the processor represents the final state). This approach is
reliable but it has the following downsides:
   * The model requires separate method to reorder items.
   * There could be no changes in the order which should be detected separately.
   * Update to only one item may require complicated sorting.
   * Complexity: O(NlogN), N - number of items in the model.

The recommended solution above is harder to verify but it has a better linear
complexity (from the number of incoming updates).

#### Why other solutions run into pitfalls

##### Inconsistent positions

While applying remote updates in the bridge, there is an inconsistency between
the order in the model and in the sync processor.

Example:
* `1[A], 2[B], 3[C]` - representing 3 different items in the model with their
    corresponding unique positions (represented as a string with
    lexicographical ordering).
* Remote updates: `1[D], 2[E]` - items `1` and `2` are moved to the end.

The final (expected) state in the model after applying updates is the following:
`3[C], 1[D], 2[E]`. However in the beginning of `ApplyIncrementalSyncChanges`
there is the following state: `1[D], 2[E], 3[C]` - the order of unique positions
is incorrect. In this case iterating over the model items and comparing with
their unique positions would also be incorrect.

The recommended approach guarantees that the final state will be expected
because it works similar to removing the items which are being updated from the
model, and adding them again placing into the correct position.

##### Temporary reordering while applying updates

There is another caveat which may cause temporary reordering items even when
their unique positions were not changed. The sorting step in the recommended
approach in the decreasing order resolves this issue: the order in the model
won't change if unique positions were not updated.

Note that deletions can be applied in any order.

### Generating UniquePosition API

Sync Processor provides the following [API][UniquePositionAPI]:
* `UniquePositionForInitialEntity` - generates the initial unique position, used
  for generating positions for the first or the only entity.
* `UniquePositionAfter` - generates the position after (greater) the other
  entity.
* `UniquePositionBefore` - generates the position before (less) the other
  entity.
* `UniquePositionBetween` - generates the position between two entities.

Sync processor uses [storage keys](../model-api#identifiers) to refer to
neighbour entities when required to generate unique positions relative to them
(before, after an entity or between two entities).

Client tag hash is required to generate positions that are globally unique and
to guarantee a consistent, well-defined ordering across clients (otherwise, if
two positions are equal, their relative ordering is undefined). It also allows
to keep the existing unique position to prevent generating a new unique position
if entity's current position is already correct. This may reduce the traffic to
the server or prevent indefinite growth of the unique position in specifics.

[UniquePositionAPI]: https://source.chromium.org/search?q=class:%5CbDataTypeLocalChangeProcessor%5Cb%20UniquePosition&ss=chromium%2Fchromium%2Fsrc