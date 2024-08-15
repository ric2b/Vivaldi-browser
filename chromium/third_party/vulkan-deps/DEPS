# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': '9f37ad360ea1c32162f0cc1799b1b292594fc771',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '8b246ff75c6615ba4532fe4fde20f1be090c3764',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'c6615779ef8a1eb5ef48fef77bf3eb2cf4f5438d',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': '577baa05033cf1d9236b3d078ca4b3269ed87a2b',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '61a9c50248e09f3a0e0be7ce6f8bb1663855f979',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': '733fd2b522918f81f9c4669350dafd066f99c5d5',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': 'a4140c5fd47dcf3a030726a60b293db61cfb54a3',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '5b8af92af052c83444ac560ff1e28c1c322424ef',
}

deps = {
  'glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@{glslang_revision}',
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
