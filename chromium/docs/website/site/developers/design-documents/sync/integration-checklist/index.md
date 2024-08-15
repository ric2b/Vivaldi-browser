---
breadcrumbs:
- - /developers
  - For Developers
- - /developers/design-documents
  - Design Documents
- - /developers/design-documents/sync
  - Sync
page_name: integration-checklist
title: Sync Integration Checklist
---

If you want to integrate with our infrastructure, please follow these steps:
*   Before starting any integration work, please reach out to us at
    chrome-sync-dev@google.com and share your plans / design doc! The Chrome
    Sync team will assign you a "champion" (primary contact person) to review
    your designs and code.
*   If you're a Googler, please also file a bug via
    [go/create-new-sync-data-type-bug][create-new-sync-data-type-bug]
*   Define your specifics proto in [`//components/sync/protocol/`][protocol].
*   Add it to the [proto value conversions][conversions] files.
*   Add a field for it to [`EntitySpecifics`][EntitySpecifics].
*   At this point, there are also some server-side integration steps to be done.
    They should be discussed in the thread or bug created above.
*   Add it to the [`ModelType`][ModelType] enum and
    [`kModelTypeInfoMap`][info_map].
*   Add to the `SyncModelTypes` enum in [`enums.xml`][enums] and to the
    `SyncModelType` suffix in [`histograms.xml`][histograms].
*   If your type should have its own toggle in sync settings, add an entry to
    the [`UserSelectableType`][UserSelectableType] enum, add a
    [preference][pref_names] for tracking whether your type is enabled, and
    map your type to the pref in [`GetPrefNameForType`][GetPrefName].
*   Otherwise, if your type should be included in an existing toggle in sync
    settings, add it in
    [`GetUserSelectableTypeInfo`][GetUserSelectableTypeInfo].
*   Register a [`ModelTypeController`][ModelTypeController] for your type in
    [`SyncApiComponentFactoryImpl::CreateCommonDataTypeControllers`][CreateCommonDataTypeControllers]
    or platform-specific equivalent in
    [`ChromeSyncClient::CreateDataTypeControllers`][CreateDataTypeControllers].
*   Add your KeyedService dependency to
    [`SyncServiceFactory`][SyncServiceFactory].
*   Implement the actual data type logic. This will mostly be an implementation
    of the [`ModelTypeSyncBridge`][Bridge] interface.
*   Write some [integration tests](../model-api/#automated-testing).
*   While rolling out your new data type, keep the Sync team in the loop! E.g.
    CC your assigned champion on all Finch CLs.
*   If you're a Googler, please fill out [go/sync-integration-feedback][sync-integration-feedback].

[create-new-sync-data-type-bug]: http://go/create-new-sync-data-type-bug
[protocol]: https://cs.chromium.org/chromium/src/components/sync/protocol/
[ModelType]: https://cs.chromium.org/chromium/src/components/sync/base/model_type.h
[info_map]: https://cs.chromium.org/search/?q="kModelTypeInfoMap%5B%5D"+file:model_type.cc
[conversions]: https://cs.chromium.org/chromium/src/components/sync/protocol/proto_value_conversions.h
[ModelTypeController]: https://cs.chromium.org/chromium/src/components/sync/service/model_type_controller.h
[CreateCommonDataTypeControllers]: https://cs.chromium.org/search/?q="SyncApiComponentFactoryImpl::CreateCommonDataTypeControllers"
[CreateDataTypeControllers]: https://cs.chromium.org/search/?q="ChromeSyncClient::CreateDataTypeControllers"
[SyncServiceFactory]: https://cs.chromium.org/search/?q=:SyncServiceFactory%5C(%5C)
[NigoriSpecifics]: https://cs.chromium.org/chromium/src/components/sync/protocol/nigori_specifics.proto
[UserSelectableType]: https://cs.chromium.org/chromium/src/components/sync/base/user_selectable_type.h?type=cs&q="enum+class+UserSelectableType"
[pref_names]: https://cs.chromium.org/chromium/src/components/sync/base/pref_names.h
[GetPrefName]: https://cs.chromium.org/search/?q=GetPrefNameForType+file:sync_prefs.cc
[GetUserSelectableTypeInfo]: https://cs.chromium.org/chromium/src/components/sync/base/user_selectable_type.cc?type=cs&q="UserSelectableTypeInfo+GetUserSelectableTypeInfo"+f:components/sync/base/user_selectable_type.cc
[enums]: https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/sync/enums.xml
[histograms]: https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/sync/histograms.xml
[DataTypeHistogram]: https://cs.chromium.org/chromium/src/components/sync/base/data_type_histogram.h
[sync-integration-feedback]: http://go/sync-integration-feedback
