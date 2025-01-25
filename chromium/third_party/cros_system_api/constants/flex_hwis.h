// Copyright 2024 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYSTEM_API_CONSTANTS_FLEX_HWIS_H_
#define SYSTEM_API_CONSTANTS_FLEX_HWIS_H_

namespace flex_hwis {

// There are several places where we pass key-value (string-string) pairs of
// hardware data for surfacing to SWEs/QE/(T)P(g)Ms. We'd like the formatting
// and handling to be consistent. This header provides consistent key names and
// a shared storage location. See platform2/flex_hwis for additional details.

// Leave off the initial '/' because many projects do testing injection with
// base::FilePath.Append which doesn't like absolute paths.
inline constexpr char kFlexHardwareCacheDir[] = "run/flex_hardware/";

inline constexpr char kFlexProductNameKey[] = "chromeosflex_product_name";
inline constexpr char kFlexProductVendorKey[] = "chromeosflex_product_vendor";
inline constexpr char kFlexProductVersionKey[] = "chromeosflex_product_version";

inline constexpr char kFlexTotalMemoryKey[] = "chromeosflex_total_memory";

inline constexpr char kFlexBiosVersionKey[] = "chromeosflex_bios_version";
inline constexpr char kFlexSecurebootKey[] = "chromeosflex_secureboot";
inline constexpr char kFlexUefiKey[] = "chromeosflex_uefi";

inline constexpr char kFlexCpuNameKey[] = "chromeosflex_cpu_name";

inline constexpr char kFlexBluetoothDriverKey[] =
    "chromeosflex_bluetooth_driver";
inline constexpr char kFlexBluetoothIdKey[] = "chromeosflex_bluetooth_id";
inline constexpr char kFlexBluetoothNameKey[] = "chromeosflex_bluetooth_name";

inline constexpr char kFlexEthernetDriverKey[] = "chromeosflex_ethernet_driver";
inline constexpr char kFlexEthernetIdKey[] = "chromeosflex_ethernet_id";
inline constexpr char kFlexEthernetNameKey[] = "chromeosflex_ethernet_name";

inline constexpr char kFlexWirelessDriverKey[] = "chromeosflex_wireless_driver";
inline constexpr char kFlexWirelessIdKey[] = "chromeosflex_wireless_id";
inline constexpr char kFlexWirelessNameKey[] = "chromeosflex_wireless_name";

inline constexpr char kFlexGpuDriverKey[] = "chromeosflex_gpu_driver";
inline constexpr char kFlexGpuIdKey[] = "chromeosflex_gpu_id";
inline constexpr char kFlexGpuNameKey[] = "chromeosflex_gpu_name";

inline constexpr char kFlexGlExtensionsKey[] = "chromeosflex_gl_extensions";
inline constexpr char kFlexGlRendererKey[] = "chromeosflex_gl_renderer";
inline constexpr char kFlexGlShadingVersionKey[] =
    "chromeosflex_gl_shading_version";
inline constexpr char kFlexGlVendorKey[] = "chromeosflex_gl_vendor";
inline constexpr char kFlexGlVersionKey[] = "chromeosflex_gl_version";

inline constexpr char kFlexTpmAllowListedKey[] =
    "chromeosflex_tpm_allow_listed";
inline constexpr char kFlexTpmDidVidKey[] = "chromeosflex_tpm_did_vid";
inline constexpr char kFlexTpmManufacturerKey[] =
    "chromeosflex_tpm_manufacturer";
inline constexpr char kFlexTpmOwnedKey[] = "chromeosflex_tpm_owned";
inline constexpr char kFlexTpmSpecLevelKey[] = "chromeosflex_tpm_spec_level";
inline constexpr char kFlexTpmVersionKey[] = "chromeosflex_tpm_version";

inline constexpr char kFlexTouchpadStackKey[] = "chromeosflex_touchpad";

}  // namespace flex_hwis

#endif  // SYSTEM_API_CONSTANTS_FLEX_HWIS_H_
