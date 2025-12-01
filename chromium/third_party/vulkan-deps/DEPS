# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': '38f6708b6b6f213010c51ffa8f577a7751e12ce7',

  # Current revision of Lunarg VulkanTools
  'lunarg_vulkantools_revision': 'ac2c388bf81603af09d0eb599322b2f3447c5812',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '97e96f9e9defeb4bba3cfbd034dec516671dd7a3',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': '3aeaaa088d37b86cff036eee1a9bf452abad7d9d',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'a01329f307fa6067da824de9f587f292d761680b',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': 'f2389e27734347c1d9f40e03be53f69f969976b1',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': 'f766b30b2de3ffe2cf6b656d943720882617ec58',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': 'b0a40d2e50310e9f84327061290a390a061125a3',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '6b1b8e3d259241a68c0944ca0a7bb5320d086191',
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
