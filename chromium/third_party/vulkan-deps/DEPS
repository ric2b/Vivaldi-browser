# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': 'e57f993cff981c8c3ffd38967e030f04d13781a9',

  # Current revision of Lunarg VulkanTools
  'lunarg_vulkantools_revision': '6f355619b1b1bfce2b5fb1dcf2aca0d55f9606de',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '8c88e0c4c94a21de825efccba5f99a862b049825',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': '2e83ad7e6f2cc51f7eaff3ffeb10e34351b3c157',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': '78c359741d855213e8685278eb81bb62599f8e56',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '723d6b4aa35853315c6e021ec86388b3a2559fae',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': '289efccc7560f2b970e2b4e0f50349da87669311',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': '0d5b49b80f17bca25e7f9321ad4e671a56f70887',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '73d7d74bc979c8a16c823c4eae4ee881153e000a',
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
