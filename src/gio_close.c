/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <unistd.h> /* close() */

#include "gioi.h"

#if GIO_PROFILING_MODE == 1

#include <stdio.h>

static
void print_profiled(MPI_Comm comm)
{
    int i, rank;
    double max_t[NTIMERS];
    MPI_Count max_c[NTIMERS];

    MPI_Comm_rank(comm, &rank);

    /* collect two-phase I/O timers */
    MPI_Reduce(gio_wr_time, max_t, NTIMERS, MPI_DOUBLE, MPI_MAX, 0, comm);
    for (i=0; i<NTIMERS; i++) gio_wr_time[i] = max_t[i];

    MPI_Reduce(gio_rd_time, max_t, NTIMERS, MPI_DOUBLE, MPI_MAX, 0, comm);
    for (i=0; i<NTIMERS; i++) gio_rd_time[i] = max_t[i];

    MPI_Reduce(gio_wr_count, max_c, NTIMERS, MPI_COUNT, MPI_MAX, 0, comm);
    for (i=0; i<NTIMERS; i++) gio_wr_count[i] = max_c[i];

    MPI_Reduce(gio_rd_count, max_c, NTIMERS, MPI_COUNT, MPI_MAX, 0, comm);
    for (i=0; i<NTIMERS; i++) gio_rd_count[i] = max_c[i];

    /* print 2-phase write timers */
    if (rank == 0 && gio_wr_count[0] > 0) {
        printf("GIO write time: init %.2f pwrite %.2f pread %.2f post %.2f hsort %.2f comm %.2f total %.2f\n",
        gio_wr_time[1], gio_wr_time[2], gio_rd_time[2], gio_wr_time[4], gio_wr_time[5], gio_wr_time[3], gio_wr_time[0]);
        printf("GIO write count: ntimes %lld check_hole %lld (npairs %lld nrecv %lld) no check %lld (npairs %lld nrecv %lld) num_memcpy %lld\n",
        gio_wr_count[0], gio_wr_count[1], gio_wr_count[2], gio_wr_count[3], gio_wr_count[4], gio_wr_count[5], gio_wr_count[6], gio_wr_count[8]);
    }

    /* print 2-phase read timers */
    if (rank == 0 && gio_rd_count[0] > 0)
        printf("GIO read time: init %.2f pread %.2f post %.2f wait %.2f total %.2f (ntimes %lld)\n",
        gio_rd_time[1], gio_rd_time[2], gio_rd_time[3], gio_rd_time[4], gio_rd_time[0], gio_rd_count[0]);
}
#endif

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

