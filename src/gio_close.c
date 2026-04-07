/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <unistd.h> /* close() */

#include "gioi.h"

/*----< GIO_close() >------------------------------------------------------*/
int GIO_close(GIO_File *fh)
{
    int err = GIO_NOERR;

    if ((*fh)->is_open) {
        err = close((*fh)->fd_sys);
        if (err != 0)
            err = GIOI_error_posix("close");
    }

    if ((*fh)->hints != NULL) {
        if ((*fh)->hints->aggr_ranks != NULL)
            GIOI_Free((*fh)->hints->aggr_ranks);
        GIOI_Free((*fh)->hints);
    }

    if ((*fh)->info != MPI_INFO_NULL)
        MPI_Info_free(&((*fh)->info));

    if ((*fh)->io_buf != NULL)
        GIOI_Free((*fh)->io_buf);

#if GIO_PROFILING_MODE == 1
    print_profiled((*fh)->comm);
#endif

    GIOI_Free((*fh)->filename);

    MPI_Comm_free(&(*fh)->comm);

    GIOI_Free(*fh);
    *fh = NULL;

    return err;
}

