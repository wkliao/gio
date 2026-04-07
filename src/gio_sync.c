/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h> /* fsync() */
#endif

#include "gioi.h"

/*----< GIO_File_sync() >---------------------------------------------------*/
int GIO_File_sync(GIO_File fh)
{
    int err = GIO_NOERR;

    if (fh->is_open > 0) {
        err = fsync(fh->fd_sys);
        if (err != 0)
            err = GIOI_error_posix("fsync");
    }

    return err;
}

