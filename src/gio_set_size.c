/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h> /* ftruncate(), lseek() */
#endif

#include "gioi.h"

/*----< GIO_set_size() >---------------------------------------------------*/
int GIO_set_size(GIO_File   fh,
                   MPI_Offset size)
{
    int err = GIO_NOERR, rank;

    MPI_Comm_rank(fh->comm, &rank);

    if (rank == 0) {
        err = ftruncate(fh->fd_sys, (off_t) size);
        if (err != 0)
            err = GIOI_error_posix("ftruncate");
    }

    MPI_Bcast(&err, 1, MPI_INT, 0, fh->comm);

    return err;
}

/*----< GIO_get_size() >---------------------------------------------------*/
int GIO_get_size(GIO_File    fh,
                   MPI_Offset *size)
{
    int err = GIO_NOERR, rank;
    MPI_Offset msg[2];

    MPI_Comm_rank(fh->comm, &rank);

    if (rank == 0) {
        *size = lseek(fh->fd_sys, 0, SEEK_END);
        if (*size == -1)
            err = GIOI_error_posix("lseek");
        msg[0] = err;
        msg[1] = *size;
    }

    MPI_Bcast(msg, 2, MPI_OFFSET, 0, fh->comm);
    err = (int)msg[0];
    *size = msg[1];

    return err;
}

