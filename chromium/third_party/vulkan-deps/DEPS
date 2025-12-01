# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': 'a57276bf558f5cf94d3a9854ebdf5a2236849a5a',

  # Current revision of Lunarg VulkanTools
  'lunarg_vulkantools_revision': 'e03d7c4236462df0de85935c3e9a247b3f1ca654',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'b8fcf307f1f347089e3c46eb4451d27f32ebc8d3',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '01e0577914a75a2569c846778c2f93aa8e6feddd',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'd5d5b61e2d5ae9b98ef403b3f3f922711812888a',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'a4f8ada9f4f97c45b8c89c57997be9cebaae65d2',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': 'f703f919c30c5b67958d35d40a4297cb3823ed78',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': 'd643b80d6ba8c191bc289fdda52867c3bb3c190b',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': '4322db5906e67b57ec9c327e6afe3d98ed893df7',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '83d96b88de7b85c3f29545170857e54e348421b2',
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
