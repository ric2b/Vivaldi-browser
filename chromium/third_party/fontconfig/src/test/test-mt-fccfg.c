/* Copyright (C) 2025 fontconfig Authors */
/* SPDX-License-Identifier: HPND */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <fontconfig/fontconfig.h>

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define NTHR 100

struct thr_arg_s {
    int thr_num;
};

#ifdef _WIN32
int
setenv (const char *name, const char *value, int o)
{
    size_t len = strlen (name) + strlen (value) + 1;
    char  *s = malloc (len + 1);
    int    ret;

    snprintf (s, len, "%s=%s", name, value);
    ret = _putenv (s);
    free (s);
    return ret;
}
#endif

static void *
run_test_in_thread (void *arg)
{
    FcPattern *pat, *m;
    FcResult   result;

    FcInit();
    pat = FcNameParse ((const FcChar8 *)"sans-serif");
    FcConfigSubstitute (NULL, pat, FcMatchPattern);
    FcConfigSetDefaultSubstitute (NULL, pat);
    m = FcFontMatch (NULL, pat, &result);
    FcPatternDestroy (pat);
    FcPatternDestroy (m);
    FcFini();

    return NULL;
}

int
test (void)
{
    pthread_t        threads[NTHR];
    struct thr_arg_s thr_args[NTHR];
    FcConfig        *c1, *c2, *c3;
    int              i, j;

    c1 = FcConfigGetCurrent();
    for (i = 0; i < NTHR; i++) {
	int result;
	thr_args[i].thr_num = i;

	result = pthread_create (&threads[i], NULL, run_test_in_thread, (void *)&thr_args[i]);
	if (result != 0) {
	    fprintf (stderr, "Cannot create thread %d\n", i);
	    break;
	}
    }
    for (j = 0; j < i; j++) {
	pthread_join (threads[j], NULL);
    }
    FcFini();
    c3 = FcConfigCreate(); /* To avoid allocation at the same place */
    c2 = FcConfigGetCurrent();
    FcConfigDestroy (c3);
    printf ("cur: %p\n", c2);
    if (c1 == c2)
	return 1;
    /* To make visible if we have any references */
    setenv ("FC_DEBUG", "16", 1);
    FcFini();

    return 0;
}

int
main (int argc, char **argv)
{
    return test();
}
