/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "gioi.h"

/*----< GIO_write_at_all() >-----------------------------------------------*/
/* This is a collective call. */
GIO_Count
GIO_write_at_all(GIO_File         fh,
                   const void        *buf,
                   GIO_Count        file_npairs,
                   const GIO_Count *file_offs,
                   const GIO_Count *file_lens,
                   GIO_Count        buf_npairs,
                   const GIO_Count *buf_offs,
                   const GIO_Count *buf_lens)
{
    int err = GIO_NOERR;
    GIO_Count w_len;

    err = SANITY_CHECK(fh, file_npairs, file_offs, file_lens,
                           buf_npairs,  buf_offs,  buf_lens);

    if (err != GIO_NOERR)
        /* When an error occurs above, make this request zero-sized. */
        fh->fview.npairs = fh->bview.npairs = 0;

    /* Must participate collective opertion even an error occurs above */
    if (fh->fstype == GIO_FS_LUSTRE)
        w_len = GIO_Lustre_write_coll(fh, buf);
    else if (fh->fstype == GIO_FS_UFS)
        w_len = GIO_UFS_write_coll(fh, buf);
    else
        err = GIO_EFSTYPE;

    return (err == GIO_NOERR) ? w_len : err;
}

