// Copyright 2023 The Khronos Group Inc.
// Copyright 2023 Valve Corporation
// Copyright 2023 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0
#include <vulkan/utility/vk_format_utils.h>

// Need two translation units to test if header file behaves correctly.
bool check_format_utils_2() {
    vkuGetPlaneIndex(VK_IMAGE_ASPECT_PLANE_1_BIT);
    vkuFormatHasGreen(VK_FORMAT_R8G8B8A8_UNORM);
    vkuFormatElementSizeWithAspect(VK_FORMAT_ASTC_5x4_SRGB_BLOCK, VK_IMAGE_ASPECT_STENCIL_BIT);
    struct VKU_FORMAT_INFO f = vkuGetFormatInfo(VK_FORMAT_R8G8B8A8_SRGB);
    if (f.component_count != 4) {
        return false;
    }
    return true;
}
