/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "gioi.h"

/*----< GIO_read_at() >----------------------------------------------------*/
/* This is an independent call. */
GIO_Count
GIO_read_at(GIO_File         fh,
              void              *buf,
              GIO_Count        file_npairs,
              const GIO_Count *file_offs,
              const GIO_Count *file_lens,
              GIO_Count        buf_npairs,
              const GIO_Count *buf_offs,
              const GIO_Count *buf_lens)
{
    int err = GIO_NOERR;
    GIO_Count r_len;

    err = SANITY_CHECK(fh, file_npairs, file_offs, file_lens,
                           buf_npairs,  buf_offs,  buf_lens);

    if (err != GIO_NOERR)
        return err;

    if (file_npairs == 0 || buf_npairs == 0) /* zero-sized request */
        return 0;

    r_len = GIO_UFS_read_indep(fh, buf);

    return r_len; /* When r_len < 0, it is an NC error code */
}

