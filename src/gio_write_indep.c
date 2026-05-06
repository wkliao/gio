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
MPI_Offset
GIO_write(GIO_File          fh,
          const void       *buf,
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
        return err;

    if (file_npairs == 0 || buf_npairs == 0) /* zero-sized request */
        return 0;

    if (fh->fstype == GIOI_FS_UFS) {
        if (!fh->is_open) {
            /* If file has not been opened (only happen to non-aggregators),
             * open it now and obtain hint striping_unit.
             */
            err = GIOI_UFS_open_on_demand(fh);
            if (err != GIO_NOERR)
                return err;
        }

        w_len = GIOI_UFS_write_indep(fh, buf);
    }
    else if (fh->fstype == GIOI_FS_LUSTRE) {
        if (!fh->is_open) {
            /* If file has not been opened (only happen to non-aggregators),
             * open it now and obtain hint striping_unit.
             */
            err = GIOI_Lustre_open_on_demand(fh);
            if (err != GIO_NOERR)
                return err;
        }

        w_len = GIOI_UFS_write_indep(fh, buf);
    }
    else
        return GIO_EFSTYPE;

    return w_len; /* when w_len < 0, it is an NetCDF error code */
}

