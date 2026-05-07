/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h> /* memcpy() */
#include <limits.h> /* INT_MAX */

#include <gioi.h>


/*----< GIOI_Calc_aggregator() >---------------------------------------------*/
/* This subroutine returns the rank ID of aggregator who is responsible for the
 * request represented by (off, *len). The "len" parameter may be modified to
 * indicate the amount of data actually available in this file domain.
 */
int GIOI_Calc_aggregator(int               striping_unit,
                         int               cb_nodes,
                         const int        *cb_node_list, /* IN: [cb_nodes] */
                         MPI_Offset        min_st_off,
                         MPI_Offset        fd_size,
                         const MPI_Offset *fd_end,       /* IN: [cb_nodes] */
                         MPI_Offset        off,
                         MPI_Offset       *len)          /* IN/OUT: */
{
    int rank_index, rank;
    MPI_Offset avail_bytes;

    /* get an index into array of aggregators */
    rank_index = (int) ((off - min_st_off + fd_size) / fd_size - 1);

    if (striping_unit > 0) {
        /* Implementation for file domain alignment. Note fd_end[] have been
         * aligned with file system lock boundaries when it was produced by
         * GIOI_Calc_file_domains().
         */
        rank_index = 0;
        while (off > fd_end[rank_index])
            rank_index++;
    }

#if GIO_DEBUG_MODE == 1
    /* We index into fd_end with rank_index, and fd_end was allocated to be no
     * bigger than cb_nodes. If we ever violate that, we're overrunning arrays.
     * Obviously, we should never ever hit this abort.
     */
    if (rank_index >= cb_nodes || rank_index < 0) {
        fprintf(stderr, "Error %s(): rank_index(%d) >= cb_nodes(%d) fd_size=%lld off=%lld\n",
                __func__,rank_index, cb_nodes, fd_size, off);
        MPI_Abort(MPI_COMM_WORLD, 1);
    }
#endif

    /* fd_end[] is to make sure that we know how much data this aggregator is
     * working with. The +1 is to take into account the end vs. length issue.
     */
    avail_bytes = fd_end[rank_index] + 1 - off;
    if (avail_bytes < *len) {
        /* this file domain only has part of the requested contiguous region */
        *len = avail_bytes;
    }

    /* map our index to a rank */
    rank = cb_node_list[rank_index];

    return rank;
}

/*----<GIOI_Calc_file_domains() >--------------------------------------------*/
/* Divide the aggregate access region of a collective read/write call into a
 * set of contiguous but disjoined file regions, called file domain (denoted as
 * 'fd'). Each of them is assigned to an I/O aggregator. An aggregator is
 * responsible for carrying out the file I/O for other processes whose requests
 * fall into its file domain.
 *
 * fd_end[cb_nodes] - end location of file domains, inclusive offsets.
 *      The values are indexed by an aggregator number; they needs to be
 *      mapped to actual rank IDs in the communicator later.
 * fd_size - average size (ceiling) of file domain among cb_nodes.
 */
void GIOI_Calc_file_domains(int          cb_nodes,
                            int          striping_unit,
                            MPI_Offset   min_st_off,
                            MPI_Offset   max_end_off,
                            MPI_Offset **fd_end,    /* OUT: [cb_nodes] */
                            MPI_Offset  *fd_size)   /* OUT: */
{
    int i, rem_front, rem_back;
    MPI_Offset end_off;

    *fd_end = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * cb_nodes);

    /* partition the aggregate access region equally among I/O aggregators */
    *fd_size  = (max_end_off - min_st_off + 1 + cb_nodes - 1) / cb_nodes;

    /* Align file domain to the nearest file lock boundary (as specified by
     * striping_unit hint).
     */
#if GIO_DEBUG_MODE == 1
    assert(striping_unit > 0);
#endif

    /* align fd_end[0] to the nearest file lock boundary */
    end_off = min_st_off + *fd_size;
    rem_front = end_off % striping_unit;
    rem_back = striping_unit - rem_front;
    if (rem_front < rem_back)
        end_off -= rem_front;
    else
        end_off += rem_back;
    (*fd_end)[0] = end_off - 1;

    /* align (*fd_end)[i] to the nearest file lock boundary */
    for (i=1; i<cb_nodes; i++) {
        end_off = min_st_off + *fd_size * (i + 1);
        rem_front = end_off % striping_unit;
        rem_back = striping_unit - rem_front;
        if (rem_front < rem_back)
            end_off -= rem_front;
        else
            end_off += rem_back;
        (*fd_end)[i] = end_off - 1;
    }
    (*fd_end)[cb_nodes - 1] = max_end_off;

    /* Take care of cases in which the aggregate access region is not divisible
     * by the number of aggregators. In such cases, the last process, or the
     * last few processes, may have less load (even 0). For example, a region
     * of 97 divided among 16 processes.  Note that the division is ceiling
     * division.
     */
    for (i=0; i<cb_nodes; i++) {
        if ((*fd_end)[i] > max_end_off)
            (*fd_end)[i] = max_end_off;
    }
}

/*----< GIOI_Calc_my_req() >-------------------------------------------------*/
/* This subroutine calculates every portions of this rank's requests that fall
 * into each aggregator's file domain. When returned, it set the following
 * variables:
 * my_req_naggr - number of aggregators for which this rank has a portion of
 *      its request falling into their file domains
 * count_per_aggr[nprocs] - number of contiguous offset-length pairs of this
 *      rank's request that fall into the aggregators' file domains
 * my_req[nprocs] - metadata describing this rank's requests to be carried out
 *      by each I/O aggregator.
 * buf_idx[nprocs] - indices into the user buffer that can be directly used to
 *      perform file read/write. Note this is only relevant when the user
 *      buffer is contiguous.
 */
void
GIOI_Calc_my_req(GIO_File           fh,
                 MPI_Offset         min_st_off,
                 const MPI_Offset  *fd_end,
                 MPI_Offset         fd_size,
                 MPI_Offset        *my_req_naggr,   /* OUT: */
                 MPI_Offset        *count_per_aggr, /* OUT: [nprocs] */
                 GIOI_Access      **my_req,         /* OUT: [nprocs] */
                 MPI_Offset       **buf_idx)        /* OUT: [nprocs] */
{
    size_t memLen, alloc_sz;
    int i, nprocs, aggr;
    MPI_Offset j, l;
    MPI_Offset fd_len, rem_len, curr_idx, off, *off_ptr;
    MPI_Offset *len_ptr;

    MPI_Comm_size(fh->comm, &nprocs);

    *my_req_naggr = 0;

    /* Contents of count_per_aggr[] should be initialized to all 0s */

    *my_req = (GIOI_Access*) GIOI_Calloc(nprocs, sizeof(GIOI_Access));

    /* buf_idx is relevant only if the user buffer is contiguous. buf_idx[i]
     * stores the index into user_buf where data received from rank i should be
     * placed. This allows receives to be done without extra buffer. This can't
     * be done if buftype is not contiguous.
     */
    *buf_idx = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * nprocs);
    for (i=0; i<nprocs; i++) /* initialize buf_idx to -1 */
        (*buf_idx)[i] = -1;

    if (fh->fview.size == 0) /* zero-sized request */
        return;

#if GIO_DEBUG_MODE == 1
    /* For non-zero sized requests, fh->fview.npairs has been checked and
     * adjusted to a positive number at the beginning of GIOI_UFS_read_coll()
     * and GIOI_UFS_write_coll().
     */
    assert(fh->fview.npairs > 0);

    /* fh->fview's offset-length pairs should have been coalesced */
    for (j=0; j<fh->fview.npairs; j++)
        assert(fh->fview.len[j] > 0);
#endif

    /* one pass just to calculate how much space to allocate for my_req */
    memLen = 0;
    for (i=0; i<fh->fview.npairs; i++) {
        off = fh->fview.off[i];
        fd_len = fh->fview.len[i];
        /* Note: we set fd_len to be the total size of the access, then
         * GIOI_Calc_aggregator() will modify the value to return the
         * amount that was available from the file domain that holds the
         * first part of the access.
         */
        aggr = GIOI_Calc_aggregator(fh->hints->striping_unit,
                                    fh->hints->cb_nodes, fh->hints->aggr_ranks,
                                    min_st_off, fd_size, fd_end, off, &fd_len);
        count_per_aggr[aggr]++;
        memLen++;

        /* figure out how much data is remaining in the access (i.e. wasn't
         * part of the file domain that had the starting byte); we'll take
         * care of this data (if there is any) in the while loop below.
         */
        rem_len = fh->fview.len[i] - fd_len;

        while (rem_len != 0) {
            off += fd_len;      /* point to first remaining byte */
            fd_len = rem_len;   /* save remaining size, pass to calc */
            aggr = GIOI_Calc_aggregator(fh->hints->striping_unit,
                                        fh->hints->cb_nodes,
                                        fh->hints->aggr_ranks, min_st_off,
                                        fd_size, fd_end, off, &fd_len);

            count_per_aggr[aggr]++;
            memLen++;
            rem_len -= fd_len;  /* reduce remaining length by amount from fd */
        }
    }

    alloc_sz = sizeof(MPI_Offset) * 2;
    (*my_req)[0].off = (MPI_Offset *) GIOI_Malloc(alloc_sz * memLen);
    (*my_req)[0].len = (*my_req)[0].off + memLen;

    off_ptr = (*my_req)[0].off;
    len_ptr = (*my_req)[0].len;
    for (i=0; i<nprocs; i++) {
        if (count_per_aggr[i]) {
            (*my_req)[i].off = off_ptr;
            off_ptr += count_per_aggr[i];
            (*my_req)[i].len = len_ptr;
            len_ptr += count_per_aggr[i];
            (*my_req_naggr)++;
        }
        (*my_req)[i].num = 0; /* will be incremented in loop j below */
    }

    /* now fill in my_req */
    curr_idx = 0;
    for (j=0; j<fh->fview.npairs; j++) {
        off = fh->fview.off[j];
        fd_len = fh->fview.len[j];

        aggr = GIOI_Calc_aggregator(fh->hints->striping_unit,
                                    fh->hints->cb_nodes, fh->hints->aggr_ranks,
                                    min_st_off, fd_size, fd_end, off, &fd_len);

        /* for each separate contiguous access from this process */
        if ((*buf_idx)[aggr] == -1)
            (*buf_idx)[aggr] = (MPI_Offset) curr_idx;

        l = (*my_req)[aggr].num;
        curr_idx += fd_len;

        rem_len = fh->fview.len[j] - fd_len;

        /* store aggr, offset, and len in an array of structures, my_req. Each
         * structure contains the offsets and lengths located in that process's
         * FD, and the associated count.
         */
        (*my_req)[aggr].off[l] = off;
        (*my_req)[aggr].len[l] = fd_len;
        (*my_req)[aggr].num++;

        while (rem_len != 0) {
            off += fd_len;
            fd_len = rem_len;
            aggr = GIOI_Calc_aggregator(fh->hints->striping_unit,
                                        fh->hints->cb_nodes,
                                        fh->hints->aggr_ranks, min_st_off,
                                        fd_size, fd_end, off, &fd_len);

            if ((*buf_idx)[aggr] == -1)
                (*buf_idx)[aggr] = (MPI_Offset) curr_idx;

            l = (*my_req)[aggr].num;
            curr_idx += fd_len;
            rem_len -= fd_len;

            (*my_req)[aggr].off[l] = off;
            (*my_req)[aggr].len[l] = fd_len;
            (*my_req)[aggr].num++;
        }
    }
}

/*----< GIOI_Calc_others_req() >---------------------------------------------*/
/* This subroutine produces results that are only relevant to the I/O
 * aggregators. Based on every rank's my_req, it calculates through MPI
 * communication for what portions of requests from all processes that fall
 * into this aggregator's file domain. It sets the following variable:
 * others_req[nprocs] - metadata describing all processes' requests to be
 *      carried out by this aggregator.
 */
int
GIOI_Calc_others_req(GIO_File            fh,
                     MPI_Offset          my_req_naggr,
                     const MPI_Offset   *count_per_aggr,/* IN: [nprocs] */
                     const GIOI_Access  *my_req,        /* IN: [nprocs] */
                     GIOI_Access       **others_req)    /* OUT: [nprocs] */
{
    size_t alloc_sz, memLen;
    int i, j, nprocs, myrank, err;
    MPI_Request *reqs;
    MPI_Offset *off_ptr;
    MPI_Offset others_nprocs, *others_npairs;
    MPI_Offset *len_ptr;
    MPI_Offset *mem_ptr;

    MPI_Comm_size(fh->comm, &nprocs);
    MPI_Comm_rank(fh->comm, &myrank);

    /* First find out how much to send/recv and from/to which aggregators.
     * others_npairs[nprocs] is the number of contiguous offset-length pairs of
     * each process that fall into this aggregator's file domain.
     */
    others_npairs = GIOI_Malloc(sizeof(MPI_Offset) * nprocs);

    MPI_Alltoall(count_per_aggr, 1, MPI_OFFSET, others_npairs,
                                 1, MPI_OFFSET, fh->comm);

    *others_req = (GIOI_Access*) GIOI_Malloc(sizeof(GIOI_Access) * nprocs);

    memLen = 0;
    for (i=0; i<nprocs; i++)
        memLen += others_npairs[i];

    alloc_sz = sizeof(MPI_Offset) * 2 + sizeof(MPI_Offset);
    (*others_req)[0].off = (MPI_Offset *) GIOI_Malloc(alloc_sz * memLen);
    (*others_req)[0].len = (*others_req)[0].off + memLen;
    (*others_req)[0].ptr = (MPI_Offset*) ((*others_req)[0].len + memLen);

    off_ptr = (*others_req)[0].off;
    len_ptr = (*others_req)[0].len;
    mem_ptr = (*others_req)[0].ptr;

    /* others_nprocs is number of processes whose portions of requests fall
     * into this aggregator's file domain (including self rank)
     */
    others_nprocs = 0;
    for (i=0; i<nprocs; i++) {
        if (others_npairs[i]) {
            (*others_req)[i].num = others_npairs[i];
            (*others_req)[i].off = off_ptr;
            off_ptr += others_npairs[i];
            (*others_req)[i].len = len_ptr;
            len_ptr += others_npairs[i];
            (*others_req)[i].ptr = mem_ptr;
            mem_ptr += others_npairs[i];
            others_nprocs++;
        } else
            (*others_req)[i].num = 0;
    }

    /* now send the calculated offsets and lengths to respective processes */
    reqs = (MPI_Request*) GIOI_Malloc(sizeof(MPI_Request) *
                                      (my_req_naggr + others_nprocs) * 2);

    j = 0;
    for (i=0; i<nprocs; i++) {
        if ((*others_req)[i].num == 0) continue;

        if (i == myrank) {
            /* send to self by using memcpy().
             * Note (*others_req)[i].num == my_req[i].num
             */
            memcpy((*others_req)[i].off, my_req[i].off,
                   my_req[i].num * sizeof(MPI_Offset));
            memcpy((*others_req)[i].len, my_req[i].len,
                   my_req[i].num * sizeof(MPI_Offset));
            continue;
        }

        /* i != myrank */
        if ((*others_req)[i].num > INT_MAX) {
#ifdef HAVE_MPI_LARGE_COUNT
            err = MPI_Irecv_c((*others_req)[i].off, (*others_req)[i].num,
                        MPI_OFFSET, i, i + myrank, fh->comm, &reqs[j++]);
            err = GIOI_error_mpi(err, "MPI_Irecv_c");

            err = MPI_Irecv_c((*others_req)[i].len, (*others_req)[i].num,
                        MPI_OFFSET, i, i + myrank, fh->comm, &reqs[j++]);
            err = GIOI_error_mpi(err, "MPI_Irecv_c");
#else
            err = GIO_EINTOVERFLOW;
#endif
        }
        else {
            int nelems = (int)(*others_req)[i].num;
            err = MPI_Irecv((*others_req)[i].off, nelems, MPI_OFFSET,
                            i, i + myrank, fh->comm, &reqs[j++]);
            err = GIOI_error_mpi(err, "MPI_Irecv");

            err = MPI_Irecv((*others_req)[i].len, nelems, MPI_OFFSET,
                            i, i + myrank, fh->comm, &reqs[j++]);
            err = GIOI_error_mpi(err, "MPI_Irecv");
        }
        if (err != GIO_NOERR) return err;
    }

    for (i=0; i<nprocs; i++) {
        if (my_req[i].num == 0 || i == myrank) continue;

        if (my_req[i].num > INT_MAX) {
#ifdef HAVE_MPI_LARGE_COUNT
            err = MPI_Isend_c(my_req[i].off, my_req[i].num, MPI_OFFSET, i,
                              i + myrank, fh->comm, &reqs[j++]);
            err = GIOI_error_mpi(err, "MPI_Isend_c");

            err = MPI_Isend_c(my_req[i].len, my_req[i].num, MPI_OFFSET, i,
                              i + myrank, fh->comm, &reqs[j++]);
            err = GIOI_error_mpi(err, "MPI_Isend_c");
#else
            err = GIO_EINTOVERFLOW;
#endif
        }
        else {
            int nelems = (int)my_req[i].num;
            err = MPI_Isend(my_req[i].off, nelems, MPI_OFFSET, i, i + myrank,
                            fh->comm, &reqs[j++]);
            err = GIOI_error_mpi(err, "MPI_Isend");

            err = MPI_Isend(my_req[i].len, nelems, MPI_OFFSET, i, i + myrank,
                            fh->comm, &reqs[j++]);
            err = GIOI_error_mpi(err, "MPI_Isend");
        }
        if (err != GIO_NOERR) return err;
    }

    if (j) {
#ifdef HAVE_MPI_STATUSES_IGNORE
        MPI_Waitall(j, reqs, MPI_STATUSES_IGNORE);
#else
        MPI_Status *sts = (MPI_Status*) GIOI_Malloc(sizeof(MPI_Status) * j);
        MPI_Waitall(j, reqs, sts);
        GIOI_Free(sts);
#endif
    }

    GIOI_Free(others_npairs);
    GIOI_Free(reqs);

    return err;
}

