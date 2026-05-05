/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "gioi.h"

/*----< GIO_write_all() >----------------------------------------------------*/
/* This is a collective call. */
MPI_Offset
GIO_write_all(GIO_File         fh,
              const void      *buf,
              MPI_Offset        file_npairs,
              const MPI_Offset *file_offs,
              const MPI_Offset *file_lens,
              MPI_Offset        buf_npairs,
              const MPI_Offset *buf_offs,
              const MPI_Offset *buf_lens)
{
    int err = GIO_NOERR;
    MPI_Offset w_len;

    err = SANITY_CHECK(fh, file_npairs, file_offs, file_lens,
                           buf_npairs,  buf_offs,  buf_lens);

    if (err != GIO_NOERR)
        /* When an error occurs above, make this request zero-sized. */
        fh->fview.npairs = fh->bview.npairs = 0;

    /* Must participate collective operation even an error occurs above */
    if (fh->fstype == GIO_FS_LUSTRE)
        w_len = GIO_Lustre_write_coll(fh, buf);
    else if (fh->fstype == GIO_FS_UFS)
        w_len = GIO_UFS_write_coll(fh, buf);
    else
        err = GIO_EFSTYPE;

    return (err == GIO_NOERR) ? w_len : err;
}

