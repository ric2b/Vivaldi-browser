// Copyright (c) 2025 Vivaldi Technologies AS. All rights reserved

#include <string>

/** Filters LD_PRELOAD for spawned external processes so that our preloads don't
 * leak out. If VIVALDI_PRELOADS env var is found, it uses values in that set to
 * filter out the LD_PRELOAD, otherwise it removes all 'libffmpeg.so' string
 * containing preloads.
 */
std::string VivaldiFilterLDPreload();
