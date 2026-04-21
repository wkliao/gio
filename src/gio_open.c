/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>      /* open(), O_CREAT */
#include <sys/types.h>  /* open(), umask(), fstat() */

#if defined(HAVE_SYS_STAT_H) && HAVE_SYS_STAT_H == 1
#include <sys/stat.h>   /* fstat() */
#endif
#include <unistd.h>     /* fstat() */

#include <assert.h>
#include <sys/errno.h>

#include <gioi.h>

/*----< GIO_open() >-------------------------------------------------------*/
/* This is a collective call. */
int
GIO_open(MPI_Comm    comm,
         const char *filename,
         int         amode, /* O_CREAT|O_RDWR, O_RDWR, or O_RDONLY */
         MPI_Info    info,
         GIO_File   *handle)
{
    int err, min_err, status=GIO_NOERR;

    GIOI_File *fh = GIOI_Malloc(sizeof(GIOI_File));

    *handle = fh;

    fh->comm      = comm;
    fh->filename  = filename; /* without file system type name prefix */
    fh->fd_sys    = -1;       /* file has not yet been opened */
    fh->atomicity = 0;
    fh->is_open   = 0;    /* this rank has opened the file */
    fh->is_agg    = 0;    /* whether this rank is an I/O aggregator */
    fh->amode     = amode;
    fh->io_buf    = NULL; /* collective buffer used by aggregators only */

    /* allocate and initialize info object */
    fh->hints = (GIO_Hints*) GIOI_Calloc(1, sizeof(GIO_Hints));
    status = GIO_set_info(fh, info);
    if (status != GIO_NOERR && status != GIO_EMULTIDEFINE_HINTS) {
        /* Inconsistent I/O hints is not a fatal error.
         * In GIO_set_info(), root's hints overwrite local's.
         */
        goto err_out;
    }

    /* Find the file system type of the file to be opened. */
    fh->fstype = GIO_FileSysType(filename);

    /* Now, create/open the file. Note fh->is_agg, indicating whether this rank
     * is an I/O aggregator,  will be set at the end of create/open calls.
     */
    if (fh->fstype == GIO_FS_LUSTRE) {
        if (amode & O_CREAT)
            err = GIO_Lustre_create(fh);
        else
            err = GIO_Lustre_open(fh);
    }
    else if (fh->fstype == GIO_FS_UFS) {
        /* GIO_UFS_open uses fh->amode to tell if create or open */
        err = GIO_UFS_open(fh);
    }
    else
        err = GIO_EFSTYPE;

    if (err != GIO_NOERR) { /* Failer to open the file is a fatal error */
        status = err;
        goto err_out;
    }

    /* collective buffer is used only by I/O aggregators only */
    if (fh->is_agg) {
        fh->io_buf = GIOI_Calloc(1, fh->hints->cb_buffer_size);
        if (fh->io_buf == NULL) /* fatal error */
            status = GIO_ENOMEM;
    }

err_out:
    MPI_Allreduce(&status, &min_err, 1, MPI_INT, MPI_MIN, comm);
    /* All NC errors are < 0 */

    if (min_err != GIO_NOERR) {
        if (status == GIO_NOERR && fh->is_open)
            /* close file if opened successfully */
            close(fh->fd_sys);
        GIOI_Free(fh->hints);
        if (fh->info != MPI_INFO_NULL)
            MPI_Info_free(&(fh->info));
        if (fh->io_buf != NULL)
            GIOI_Free(fh->io_buf);

        GIOI_Free(fh);
        *handle = NULL;
    }

    return status;
}

