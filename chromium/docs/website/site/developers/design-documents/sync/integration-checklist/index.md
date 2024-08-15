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
    map your type to the pref in [`GetPrefNameForType`][GetPrefName]. Add the
    toggle UI for each supported platform:
    * [Desktop][toggles_desktop]
    * [Android][toggles_android]
    * [iOS][toggles_ios]
*   Otherwise, if your type should be included in an existing toggle in sync
    settings, add it in
    [`GetUserSelectableTypeInfo`][GetUserSelectableTypeInfo].
*   Register a [`ModelTypeController`][ModelTypeController] for your type in
    [`SyncApiComponentFactoryImpl::CreateCommonModelTypeControllers`][CreateCommonModelTypeControllers]
    or platform-specific equivalent in
    [`ChromeSyncClient::CreateModelTypeControllers`][CreateModelTypeControllers].
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
[EntitySpecifics]: https://source.chromium.org/chromium/chromium/src/+/main:components/sync/protocol/entity_specifics.proto
[ModelTypeController]: https://cs.chromium.org/chromium/src/components/sync/service/model_type_controller.h
[CreateCommonModelTypeControllers]: https://cs.chromium.org/search/?q="SyncApiComponentFactoryImpl::CreateCommonModelTypeControllers"
[CreateModelTypeControllers]: https://cs.chromium.org/search/?q="ChromeSyncClient::CreateModelTypeControllers"
[SyncServiceFactory]: https://cs.chromium.org/search/?q=:SyncServiceFactory%5C(%5C)
[Bridge]: https://source.chromium.org/search?q=%5CbModelTypeSyncBridge%5Cb
[NigoriSpecifics]: https://cs.chromium.org/chromium/src/components/sync/protocol/nigori_specifics.proto
[UserSelectableType]: https://cs.chromium.org/chromium/src/components/sync/base/user_selectable_type.h?type=cs&q="enum+class+UserSelectableType"
[pref_names]: https://cs.chromium.org/chromium/src/components/sync/base/pref_names.h
[GetPrefName]: https://cs.chromium.org/search/?q=GetPrefNameForType+file:sync_prefs.cc
[toggles_desktop]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/settings/people_page/sync_controls.html
[toggles_android]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/java/src/org/chromium/chrome/browser/sync/settings/ManageSyncSettings.java
[toggles_ios]: https://source.chromium.org/chromium/chromium/src/+/main:ios/chrome/browser/ui/settings/google_services/manage_sync_settings_mediator.mm
[GetUserSelectableTypeInfo]: https://cs.chromium.org/chromium/src/components/sync/base/user_selectable_type.cc?type=cs&q="UserSelectableTypeInfo+GetUserSelectableTypeInfo"+f:components/sync/base/user_selectable_type.cc
[enums]: https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/sync/enums.xml
[histograms]: https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/histograms/metadata/sync/histograms.xml
[DataTypeHistogram]: https://cs.chromium.org/chromium/src/components/sync/base/data_type_histogram.h
[sync-integration-feedback]: http://go/sync-integration-feedback
