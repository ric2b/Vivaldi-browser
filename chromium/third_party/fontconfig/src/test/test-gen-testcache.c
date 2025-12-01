/* Copyright (C) 2025 fontconfig Authors */
/* SPDX-License-Identifier: HPND */
#include "fcint.h"
#include "src/fcint.h"

int
main (int argc, char **argv)
{
    FcConfig  *config;
    FcFontSet *fs;
    FcStrSet  *dirs;
    FcCache   *cache;
    struct stat st;
    int ret = -1;

    if (argc <= 1) {
	fprintf (stderr, "Usage: %s <font path>\n", argv[0]);
	return 1;
    }
    FcInit();
    config = FcConfigGetCurrent();
    fs = FcFontSetCreate();
    dirs = FcStrSetCreateEx (FCSS_GROW_BY_64);
    if (FcStatChecksum ((const FcChar8 *)argv[1], &st) < 0)
	goto bail;
    if (!FcDirScanConfig (fs, dirs, (const FcChar8 *)argv[1], FcTrue, config))
	goto bail2;
    cache = FcDirCacheBuild (fs, (const FcChar8 *)argv[1], &st, dirs);
    if (!cache)
	goto bail2;
    cache->fc_version = ((FC_VERSION_MAJOR + 1) << 24) +
                       (FC_VERSION_MINOR << 12) +
                       FC_VERSION_MICRO;
    FcDirCacheWrite (cache, config);
    ret = 0;
 bail2:
    FcStrSetDestroy (dirs);
 bail:
    FcFontSetDestroy (fs);
    FcConfigDestroy (config);

    FcFini();

    return ret;
}
