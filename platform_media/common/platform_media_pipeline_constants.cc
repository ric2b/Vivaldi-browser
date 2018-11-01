// -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
//
// Copyright (C) 2015 Opera Software ASA.  All rights reserved.
//
// This file is an original work developed by Opera Software ASA

#include "platform_media/common/platform_media_pipeline_constants.h"

#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>  // Required by <GL/gl.h> on Windows.
#include <GL/gl.h>
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include <OpenGL/gl.h>
#endif  // defined(OS_MACOSX)

#if defined(OS_LINUX)
#include <GL/gl.h>
#include <GL/glext.h>
#endif  // defined(OS_LINUX)

namespace media {

const GLenum kPlatformMediaPipelineTextureFormat = GL_BGRA_EXT;
const uint32_t kPlatformMediaPipelineTextureTarget = GL_TEXTURE_2D;

}  // namespace media
