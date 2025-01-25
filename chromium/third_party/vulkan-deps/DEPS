# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': 'dc9f6f61adaec755a09e1943cf7014c688443bcb',

  # Current revision of Lunarg VulkanTools
  'lunarg_vulkantools_revision': '9bd6f95db3076517205b01300c8d37043c5b2dd3',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': 'db5a00f8cebe81146cafabf89019674a3c4bf03d',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'a0817526b8e391732632e6a887134be256a20a18',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'fabe9e2672334fdb9a622d42a2e8f94578952082',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '5892ebe2d7505c2238a643288d9a5b2e68784a36',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': '2cee0d5b1d8c34e26fd6d9992d3d428ac4c5139d',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': '67522b34edde86dbb97e164280291f387ade55fc',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '919599474e6e6639527e7c3775ed1064665412d3',
}

deps = {
  'glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@{glslang_revision}',
  },

  'lunarg-vulkantools/src': {
    'url': '{chromium_git}/external/github.com/LunarG/VulkanTools@{lunarg_vulkantools_revision}',
  },

  'spirv-cross/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@{spirv_cross_revision}',
  },

  'spirv-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@{spirv_headers_revision}',
  },

  'spirv-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@{spirv_tools_revision}',
  },

  'vulkan-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@{vulkan_headers_revision}',
  },

  'vulkan-loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@{vulkan_loader_revision}',
  },

  'vulkan-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@{vulkan_tools_revision}',
  },

  'vulkan-utility-libraries/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Utility-Libraries@{vulkan_utility_libraries_revision}',
  },

  'vulkan-validation-layers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@{vulkan_validation_revision}',
  },
}
