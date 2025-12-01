/* Copyright (C) 2025 fontconfig Authors */
/* SPDX-License-Identifier: HPND */

/* Internal API test case */
#include "fcint.h"
#include <stdio.h>

int
main (void)
{
    FcPtrList *p;
    FcPtrListIter iter;
    int i, n;

    p = FcPtrListCreate (NULL);
    FcPtrListIterInitAtLast (p, &iter);
    FcPtrListIterAdd (p, &iter, (intptr_t *)0);
    FcPtrListIterAdd(p, &iter, (intptr_t *)1);
    FcPtrListIterAdd(p, &iter, (intptr_t *)2);
    FcPtrListIterAdd (p, &iter, (intptr_t *)3);

    FcPtrListIterInit (p, &iter);
    for (i = 0; FcPtrListIterIsValid (p, &iter); FcPtrListIterNext (p, &iter), i++) {
	n = (intptr_t)(void *)FcPtrListIterGetValue (p, &iter);
	if (i != n) {
	    printf("mispatch the order: %d(%d)\n", i, n);
	    return 1;
	}
    }
    FcPtrListDestroy (p);

    return 0;
}
