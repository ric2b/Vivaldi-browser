# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True
git_dependencies = 'SYNC'

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': 'ebb0a8b28e5ea7a13146a24b3ad87b1966dbdfa3',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': '2de1265fca722929785d9acdec4ab728c47a0254',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': '88bc5e321c2839707df8b1ab534e243e00744177',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': '5084f58e5d187b16f84d2af936ff94ea2f46a00c',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'f4bfcd885214675a6a0d7d4df07f52b511e6ea16',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '7b7d15bc9f1d782e4d6ea34e61fd9a36a7952208',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': 'f7017f23337b90a2b2ceb65a4e1050e8ad89e065',

  # Current revision of Khronos Vulkan-Utility-Libraries.
  'vulkan_utility_libraries_revision': 'dcfce25b439a2785f2c90b184e1964898070b4f1',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '281e1c1c6b51bb6b33f88c18ed4da58ac3024311',
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
