/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "gioi.h"

/*----< GIO_write() >--------------------------------------------------------*/
/* This is an independent call. */
GIO_Count
GIO_write(GIO_File         fh,
          const void      *buf,
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
        return err;

    if (file_npairs == 0 || buf_npairs == 0) /* zero-sized request */
        return 0;

    if (fh->fstype == GIO_FS_UFS) {
        if (!fh->is_open) {
            /* If file has not been opened (only happen to non-aggregators),
             * open it now and obtain hint striping_unit.
             */
            err = GIOI_UFS_open_on_demand(fh);
            if (err != GIO_NOERR)
                return err;
        }

        w_len = GIO_UFS_write_indep(fh, buf);
    }
    else if (fh->fstype == GIO_FS_LUSTRE) {
        if (!fh->is_open) {
            /* If file has not been opened (only happen to non-aggregators),
             * open it now and obtain hint striping_unit.
             */
            err = GIOI_Lustre_open_on_demand(fh);
            if (err != GIO_NOERR)
                return err;
        }

        w_len = GIO_UFS_write_indep(fh, buf);
    }
    else
        return GIO_EFSTYPE;

    return w_len; /* when w_len < 0, it is an NetCDF error code */
}

