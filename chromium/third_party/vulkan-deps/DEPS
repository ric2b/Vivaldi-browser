# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': '57d86ab763da7b2cd1e00ecec8aa697403a8fd20',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'b82536766d1b81631b126d1ddbe49baf42929bd3',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '7b0309708da5126b89e4ce6f19835f36dc912f2f',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'c96fe8b943564fbab3424219d924d21cac2e877a',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': '217e93c664ec6704ec2d8c36fa116c1a4a1e2d40',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '0b2b71306aebf1e11304b9f961f9a29ab0234756',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': '7c6d640a5ca3ab73c1f42d22312f672b54babfaf',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': '4cfc176e3242b4dbdfd3f6c5680c5d8f2cb7db45',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': 'd26b50b03815ff226e6df478b4ddc4b98d8deaee',
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
