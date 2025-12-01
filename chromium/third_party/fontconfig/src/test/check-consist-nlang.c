/* Copyright (C) 2025 fontconfig Authors */
/* SPDX-License-Identifier: HPND */

#include <fontconfig/fontconfig.h>
#include <fontconfig/fcfreetype.h>

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>

int
check_consistency (char *file)
{
    int ret, id = 0, nfaces = 0, i, err = 0;
    FcFontSet *fs;

    do {
        fs = FcFontSetCreate();
        ret = FcFreeTypeQueryAll (file, id, NULL, &nfaces, fs);
        if (!ret) {
            break;
        }
        for (i = 0; i < fs->nfont; i++) {
            FcPattern *pat = fs->fonts[i];
            FcPatternIter iter;
            int nobj, nlang;

            //fprintf (stderr, "Checking %s:%u[%d]\n", file, id, i);
            FcPatternIterStart (pat, &iter);
            /* check for family */
            if (!FcPatternFindIter (pat, &iter, FC_FAMILY)) {
                fprintf (stderr, "No family in %s:%u\n", file, id);
                err++;
                break;
            }
            nobj = FcPatternIterValueCount (pat, &iter);
            if (!FcPatternFindIter (pat, &iter, FC_FAMILYLANG)) {
                fprintf (stderr, "No familylang in %s:%u\n", file, id);
                err++;
                break;
            }
            nlang = FcPatternIterValueCount (pat, &iter);
            if (nobj != nlang) {
                fprintf (stderr, "%s:%u: nfamily: %d nfamilylang: %d\n", file, id, nobj, nlang);
                err++;
            }
            /* check for style */
            if (!FcPatternFindIter (pat, &iter, FC_STYLE)) {
                fprintf (stderr, "No style in %s:%u\n", file, id);
                err++;
                break;
            }
            nobj = FcPatternIterValueCount (pat, &iter);
            if (!FcPatternFindIter (pat, &iter, FC_STYLELANG)) {
                fprintf (stderr, "No stylelang in %s:%u\n", file, id);
                err++;
                break;
            }
            nlang = FcPatternIterValueCount (pat, &iter);
            if (nobj != nlang) {
                fprintf (stderr, "%s:%u: nstyle: %d nstylelang: %d\n", file, id, nobj, nlang);
                err++;
            }
            /* check for fullname */
            if (!FcPatternFindIter (pat, &iter, FC_FULLNAME)) {
                fprintf (stderr, "No fullname in %s:%u\n", file, id);
                err++;
                break;
            }
            nobj = FcPatternIterValueCount (pat, &iter);
            if (!FcPatternFindIter (pat, &iter, FC_FULLNAMELANG)) {
                fprintf (stderr, "No fullnamelang in %s:%u\n", file, id);
                err++;
                break;
            }
            nlang = FcPatternIterValueCount (pat, &iter);
            if (nobj != nlang) {
                fprintf (stderr, "%s:%u: nfullname: %d nfullnamelang: %d\n", file, id, nobj, nlang);
                err++;
            }
        }
        id++;
        FcFontSetDestroy (fs);
    } while (id < nfaces);

    return err;
}

int
scan (char *name)
{
    int ret = 0;

    if (FcFileIsDir (name)) {
        DIR *d;
        struct dirent *e;
        char fullname[PATH_MAX];
        size_t len = strlen (name);

        strcpy (fullname, name);
        fullname[len] = '/';
        len++;
        d = opendir (name);
        if (!d) {
            fprintf (stderr, "Unable to open a directory: %s\n", name);
        } else {
            while ((e = readdir (d))) {
                if (e->d_name[0] == '.' &&
                    (e->d_name[1] == 0 ||
                     (e->d_name[1] == '.' && e->d_name[2] == 0)))
                    continue;
                strcpy (&fullname[len], e->d_name);
                ret += scan (fullname);
            }
        }
    } else {
        ret += check_consistency (name);
    }

    return ret;
}

int
main (int argc, char **argv)
{
    int i, ret = 0;

    for (i = 1; i < argc; i++) {
        ret += scan (argv[i]);
    }

    return ret;
}
