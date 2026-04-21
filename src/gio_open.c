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

#if GIO_PROFILING_MODE == 1
double    gio_wr_time[NTIMERS];
double    gio_rd_time[NTIMERS];
MPI_Count gio_wr_count[NTIMERS];
MPI_Count gio_rd_count[NTIMERS];
#endif

/*----< construct_NUMA_node_list() >-----------------------------------------*/
/* This subroutine is a collective call. It finds the affinity of MPI processes
 * to their shared-memory compute nodes (NUMA) and returns the followings:
 *   num_NUMAs: Number of NUMA nodes
 *   numa_ids[nprocs]: node IDs of each rank, must be freed by the caller.
 */
static int
construct_NUMA_node_list(MPI_Comm   comm,
                         int       *num_NUMAs, /* OUT: */
                         int      **numa_ids)  /* OUT: [nprocs] */
{
    char *err_msg="No error";
    int i, err, rank, nprocs, numa_id, *ids;
    MPI_Comm hwcomm;

    MPI_Comm_size(comm, &nprocs);
    MPI_Comm_rank(comm, &rank);

    *num_NUMAs = 0;
    *numa_ids = NULL;

#if 1
    /* split comm based on NUMA nodes (processes sharing memory) */
    err = MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL,
                              &hwcomm);
#else
    /* Below code fragment is from MPI standard 4.0's example 7.3:
     * Splitting MPI_COMM_WORLD into NUMANode subcommunicators.
     */
    MPI_Info_set(info, "mpi_hw_resource_type" , "NUMANode");
    err = MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_HW_GUIDED,
                              rank, info, &hwcomm);

    /* Below code fragment is from MPI standard 5.0's example 7.3:
     * Splitting MPI_COMM_WORLD into NUMANode subcommunicators.
     */
    MPI_Info_set(info, "mpi_hw_resource_type" , "hwloc://NUMANode");
    err = MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_HW_GUIDED,
                              rank, info, &hwcomm);

    /* Below code fragment is from MPI standard 5.0's example 7.4:
     * Splitting MPI_COMM_WORLD into NUMANode subcommunicators.
     */
    MPI_Info_set(info, "mpi_hw_resource_type", "hwloc://NUMANode");
    err = MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_RESOURCE_GUIDED,
                              rank, info, &hwcomm);
#endif

    if (err != MPI_SUCCESS) {
        err_msg = "MPI_Comm_split_type()";
        goto err_out;
    }

    if (hwcomm == MPI_COMM_NULL) {
        err_msg = "MPI_Comm_split_type() hwcomm NULL";
        goto err_out;
    }

    /* Use hwcomm's root's rank as this process's NUMA node ID */
    numa_id = rank;
    MPI_Bcast(&numa_id, 1, MPI_INT, 0, hwcomm);

    /* Gather all NUMA node IDs */
    *numa_ids = (int*) GIOI_Malloc(sizeof(int) * nprocs);
    MPI_Allgather(&numa_id, 1, MPI_INT, *numa_ids, 1, MPI_INT, comm);

    /* Count number of unique IDs and reassign NUMA ID */
    ids = (int*) GIOI_Calloc(nprocs, sizeof(int));
    *num_NUMAs = 0;
    for (i=0; i<nprocs; i++) {
        if (ids[(*numa_ids)[i]] == 0) {
            (*num_NUMAs)++; /* unique count */
            ids[(*numa_ids)[i]] = (*num_NUMAs); /* New ID, starting from 0 */
        }
        (*numa_ids)[i] = ids[(*numa_ids)[i]] - 1;
    }
    GIOI_Free(ids);

    if (hwcomm != MPI_COMM_NULL) MPI_Comm_free(&hwcomm);

err_out:
    if (err != MPI_SUCCESS) {
        if (*numa_ids != NULL)
            GIOI_Free(*numa_ids);
        *num_NUMAs = 0;
        *numa_ids = NULL;
        return GIOI_error_mpi(err, err_msg);
    }

    return GIO_NOERR;
}

/*----< GIO_open() >---------------------------------------------------------*/
/* This is a collective call. */
int
GIO_open(MPI_Comm    comm,
         const char *filename,
         int         amode, /* O_CREAT|O_RDWR, O_RDWR, or O_RDONLY */
         MPI_Info    info,
         GIO_File   *handle)
{
    int err, min_err, status=GIO_NOERR, nprocs;

    GIOI_File *fh = GIOI_Malloc(sizeof(GIOI_File));

    MPI_Comm_size(comm, &nprocs);

    MPI_Comm_dup(comm, &fh->comm);

    fh->fd_sys    = -1;       /* file has not yet been opened */
    fh->atomicity = 0;
    fh->is_open   = 0;    /* this rank has opened the file */
    fh->is_agg    = 0;    /* whether this rank is an I/O aggregator */
    fh->amode     = amode;
    fh->io_buf    = NULL; /* collective buffer used by aggregators only */
    fh->NUMA_IDs  = NULL;

    /* allocate and initialize info object */
    fh->hints = (GIO_Hints*) GIOI_Calloc(1, sizeof(GIO_Hints));
    fh->hints->NUMA_ID = -1; /* marked as not set */

    status = GIO_set_info(fh, info);
    if (status != GIO_NOERR && status != GIO_EMULTIDEFINE_HINTS) {
        /* Inconsistent I/O hints is not a fatal error.
         * In GIO_set_info(), root's hints overwrite local's.
         */
        goto err_out;
    }

    /* Find the file system type of the file to be opened. */
    fh->fstype = GIO_FileSysType(filename);

    /* Remove the file system type prefix name if there is any. For example,
     * when path = "lustre:/home/foo/testfile.nc", remove "lustre:" to make
     * filename pointing to "/home/foo/testfile.nc", so it can be used in POSIX
     * access() below.
     */
    fh->filename = GIOI_remove_file_system_type_prefix(filename);

    /* construct fh->NUMA_IDs[nprocs] and fh->num_NUMAs */
    if (nprocs == 1) {
        fh->NUMA_IDs = (int*) GIOI_Malloc(sizeof(int));
        fh->NUMA_IDs[0] = 0;
        fh->num_NUMAs = 1;
    }
    else if (fh->hints->NUMA_ID >= 0) {
        int j, num_NUMAs, *ids;

        /* use fh->hints->NUMA_ID to construct fh->NUMA_IDs[] */
        fh->NUMA_IDs = (int*) GIOI_Malloc(sizeof(int) * nprocs);
        MPI_Allgather(&fh->hints->NUMA_ID, 1, MPI_INT, fh->NUMA_IDs, 1,
                      MPI_INT, fh->comm);

        /* Count number of unique IDs and reassign NUMA ID */
        ids = (int*) GIOI_Calloc(nprocs, sizeof(int));
        num_NUMAs = 0;
        for (j=0; j<nprocs; j++) {
            if (ids[fh->NUMA_IDs[j]] == 0) {
                num_NUMAs++; /* unique count */
                ids[fh->NUMA_IDs[j]] = num_NUMAs; /* New ID, starting from 0 */
            }
            fh->NUMA_IDs[j] = ids[fh->NUMA_IDs[j]] - 1;
        }
        GIOI_Free(ids);
        fh->num_NUMAs = num_NUMAs;
    }
    else { /* hint NUMA_ID is not set in info by user */
        err = construct_NUMA_node_list(fh->comm, &fh->num_NUMAs, &fh->NUMA_IDs);
        if (err != GIO_NOERR) { /* Failed to open the file is a fatal error */
            fh->NUMA_IDs = NULL;
            status = err;
            goto err_out;
        }
    }

    /* Now, create/open the file. */
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

    if (err != GIO_NOERR) { /* Failed to open the file is a fatal error */
        status = err;
        goto err_out;
    }

    /* Note fh->is_agg, indicating whether or not this rank is an I/O
     * aggregator, will be set at the end of create/open calls.
     */
#if GIO_DEBUG_MODE == 1
    if (nprocs == 1) assert(fh->is_agg == 1);
#endif

    /* collective buffer is used only by I/O aggregators only */
    if (fh->is_agg) {
        fh->io_buf = GIOI_Calloc(1, fh->hints->cb_buffer_size);
        if (fh->io_buf == NULL) /* fatal error */
            status = GIO_ENOMEM;
    }

err_out:
    MPI_Allreduce(&status, &min_err, 1, MPI_INT, MPI_MIN, fh->comm);
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
        fh = NULL;
    }

    if (fh->NUMA_IDs != NULL)
        GIOI_Free(fh->NUMA_IDs);

    *handle = fh;

#if GIO_PROFILING_MODE == 1
    {
        int i;
        for (i=0; i<NTIMERS; i++) {
            gio_wr_time[i] = gio_rd_time[i] = 0;
            gio_wr_count[i]= gio_rd_count[i] = 0;
        }
    }
#endif

    return status;
}

