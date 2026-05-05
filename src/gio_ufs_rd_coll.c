/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>  /* memcpy() */
#include <stdbool.h> /* type bool */
#include <limits.h>  /* LLONG_MAX, INT_MAX */

#include "gioi.h"

#define BUF_INCR {                                                  \
    while (buf_incr) {                                              \
        size_in_buf = MIN(buf_incr, buf_rem);                       \
        user_buf_idx += size_in_buf;                                \
        buf_rem -= size_in_buf;                                     \
        buf_incr -= size_in_buf;                                    \
        if (buf_incr > 0 && buf_rem == 0) {                         \
            buf_indx++;                                             \
            user_buf_idx = fh->bview.off[buf_indx];                 \
            buf_rem = fh->bview.len[buf_indx];                      \
        }                                                           \
    }                                                               \
}

#define BUF_COPY {                                                  \
    while (size) {                                                  \
        size_in_buf = MIN(size, buf_rem);                           \
        memcpy((char*)buf + user_buf_idx,                           \
               &recv_buf[aggr][recv_buf_idx[aggr]], size_in_buf);   \
        recv_buf_idx[aggr] += size_in_buf;                          \
        user_buf_idx += size_in_buf;                                \
        buf_rem -= size_in_buf;                                     \
        size -= size_in_buf;                                        \
        buf_incr -= size_in_buf;                                    \
        if (size > 0 && buf_rem == 0) {                             \
            buf_indx++;                                             \
            user_buf_idx = fh->bview.off[buf_indx];                 \
            buf_rem = fh->bview.len[buf_indx];                      \
        }                                                           \
    }                                                               \
    BUF_INCR                                                        \
}

/*----< fill_user_buffer() >-------------------------------------------------*/
/* This subroutine is only called when buffer view is not contiguous. */
static void
fill_user_buffer(GIO_File         fh,
                 void            *buf,
                 MPI_Offset        min_st_off,
                 MPI_Offset        fd_size,
                 const MPI_Offset *fd_end,         /* IN: [cb_nodes] */
                 const MPI_Offset *recv_size,      /* IN: [nprocs] */
                 MPI_Offset       *recd_from_proc, /* IN/OUT: [nprocs] */
                 char *const     *recv_buf)       /* IN: [nprocs] */
{
    int i, nprocs, aggr, buf_indx;
    MPI_Offset buf_rem, size_in_buf, buf_incr, size;
    MPI_Offset len, rem_len, user_buf_idx;
    MPI_Offset j, *curr_from, *done_from, *recv_buf_idx;
    MPI_Offset off;

    MPI_Comm_size(fh->comm, &nprocs);

    /* curr_from[nprocs] - amount of data received from each rank that has
     *      already been accounted for so far.
     * done_from[nprocs] - amount of data already received from each rank and
     *      filled into user buffer in previous round.
     * user_buf_idx - current location in user buffer
     * recv_buf_idx[nprocs] = current location in recv_buf of each rank
     */
    curr_from = GIOI_Malloc(sizeof(MPI_Offset) * nprocs * 3);
    done_from = curr_from + nprocs;
    recv_buf_idx = done_from + nprocs;

    for (i=0; i<nprocs; i++) {
        recv_buf_idx[i] = curr_from[i] = 0;
        done_from[i] = recd_from_proc[i];
    }

    /* buf_indx - index bview's offset-length pairs being processed
     * buf_rem - remaining length of the current offset-length pair
     */
    user_buf_idx = fh->bview.off[0];
    buf_indx = 0;
    buf_rem = fh->bview.len[0];

    for (j=0; j<fh->fview.npairs; j++) {
        off = fh->fview.off[j];
        rem_len = fh->fview.len[j];

        /* this request may span file domains of more than one aggregator */
        while (rem_len != 0) {
            len = rem_len;

            /* NOTE: len value will be modified by GIO_Calc_aggregator() to
             * be no more than the single file domain that aggregator 'aggr'
             * is responsible for.
             */
            aggr = GIO_Calc_aggregator(fh->hints->striping_unit,
                                         fh->hints->cb_nodes,
                                         fh->hints->aggr_ranks, min_st_off,
                                         fd_size, fd_end, off, &len);

            if (recv_buf_idx[aggr] < recv_size[aggr]) {
                if (curr_from[aggr] + len > done_from[aggr]) {
                    if (done_from[aggr] > curr_from[aggr]) {
                        size = MIN(curr_from[aggr] + len - done_from[aggr],
                                   recv_size[aggr] - recv_buf_idx[aggr]);
                        buf_incr = done_from[aggr] - curr_from[aggr];
                        BUF_INCR
                        buf_incr = curr_from[aggr] + len - done_from[aggr];
                        curr_from[aggr] = done_from[aggr] + size;
                        BUF_COPY
                    } else {
                        size = MIN(len, recv_size[aggr] - recv_buf_idx[aggr]);
                        buf_incr = len;
                        curr_from[aggr] += size;
                        BUF_COPY
                    }
                } else {
                    curr_from[aggr] += len;
                    buf_incr = len;
                    BUF_INCR
                }
            } else {
                buf_incr = len;
                BUF_INCR
            }
            off += len;
            rem_len -= len;
        }
    }
    for (i=0; i<nprocs; i++)
        if (recv_size[i])
            recd_from_proc[i] = curr_from[i];

    GIOI_Free(curr_from);
}

/*----< R_Exchange_data() >--------------------------------------------------*/
static MPI_Offset
R_Exchange_data(GIO_File          fh,
                void             *buf,
                const MPI_Offset  *send_size,      /* IN: [nprocs] */
                const MPI_Offset  *count,          /* IN: [nprocs] */
                const MPI_Offset  *start_pos,      /* IN: [nprocs] */
                const MPI_Offset  *partial_send,   /* IN: [nprocs] */
                MPI_Offset        *recd_from_proc, /* IN/OUT: [nprocs] */
                MPI_Offset         min_st_off,
                MPI_Offset         fd_size,
                const MPI_Offset  *fd_end,         /* IN: [cb_nodes] */
                const GIO_Access *others_req,     /* IN: [nprocs] */
                MPI_Offset        *buf_idx)        /* IN/OUT: [nprocs] */
{
    char **recv_buf = NULL;
    int i, nprocs, myrank, nrecvs, nsends;
    MPI_Offset recved_bytes;
    MPI_Offset *recv_size;
    MPI_Request *reqs;
    MPI_Datatype send_type;
    MPI_Status *sts;

#if GIO_PROFILING_MODE == 1
    double endT, startT = MPI_Wtime();
#endif

    MPI_Comm_size(fh->comm, &nprocs);
    MPI_Comm_rank(fh->comm, &myrank);

    recved_bytes = 0;

    /* Exchange send_size info so that each aggregator knows how much to
     * receive from whom and how much memory to allocate.
     *
     * recv_size[] is the total size of data to be received from each process
     * in a 2-phase round.
     */
    recv_size = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * nprocs);

    MPI_Alltoall(send_size, 1, MPI_OFFSET, recv_size, 1, MPI_OFFSET, fh->comm);

    reqs = (MPI_Request*) GIOI_Malloc(sizeof(MPI_Request) * 2 * nprocs);

    /* Post nonblocking receive calls. If buffer view is contiguous, data can
     * be directly received into user buf at location pointed by buf_idx.
     * Otherwise, allocate recv_buf and use it to receive.
     */
    nrecvs = 0;
    if (fh->bview.npairs <= 1) {
        for (i=0; i<nprocs; i++) {
            if (recv_size[i]) {
#ifdef HAVE_MPI_LARGE_COUNT
                MPI_Irecv_c((char*)buf + buf_idx[i], recv_size[i], MPI_BYTE, i,
                            0, fh->comm, &reqs[nrecvs++]);
#else
                MPI_Irecv((char*)buf + buf_idx[i], recv_size[i], MPI_BYTE, i,
                           0, fh->comm, &reqs[nrecvs++]);
#endif
                buf_idx[i] += recv_size[i];
            }
        }
    } else {
        size_t memLen = 0;
        for (i=0; i<nprocs; i++)
            memLen += recv_size[i];

        /* allocate memory for recv_buf */
        recv_buf = (char **) GIOI_Malloc(sizeof(char*) * nprocs);
        recv_buf[0] = (char *) GIOI_Malloc(memLen);
        for (i=1; i<nprocs; i++)
            recv_buf[i] = recv_buf[i - 1] + recv_size[i - 1];

        /* post receives */
        for (i=0; i<nprocs; i++) {
            if (recv_size[i]) {
#ifdef HAVE_MPI_LARGE_COUNT
                MPI_Irecv_c(recv_buf[i], recv_size[i], MPI_BYTE, i,
                            0, fh->comm, &reqs[nrecvs++]);
#else
                MPI_Irecv(recv_buf[i], recv_size[i], MPI_BYTE, i,
                            0, fh->comm, &reqs[nrecvs++]);
#endif
            }
        }
    }

    /* Construct derived datatypes and use them to send data */
    nsends = 0;
    for (i=0; i<nprocs; i++) {
        if (send_size[i]) {
            /* take care the last offset-length pair if is a partial send */
            MPI_Offset tmp = 0;
            MPI_Offset k = 0;
            if (partial_send[i]) {
                k = start_pos[i] + count[i] - 1;
                tmp = others_req[i].lens[k];
                others_req[i].lens[k] = partial_send[i];
            }
#ifdef HAVE_MPI_LARGE_COUNT
            MPI_Type_create_hindexed_c(count[i],
                                       &others_req[i].lens[start_pos[i]],
                                       &others_req[i].mem_ptrs[start_pos[i]],
                                       MPI_BYTE, &send_type);
#else
            MPI_Type_create_hindexed(count[i],
                                     &others_req[i].lens[start_pos[i]],
                                     &others_req[i].mem_ptrs[start_pos[i]],
                                     MPI_BYTE, &send_type);
#endif
            /* absolute displacement; use MPI_BOTTOM in send */
            MPI_Type_commit(&send_type);
            MPI_Isend(MPI_BOTTOM, 1, send_type, i, 0, fh->comm,
                      reqs + nrecvs + nsends);
            MPI_Type_free(&send_type);
            if (partial_send[i])
                others_req[i].lens[k] = tmp;
            nsends++;
        }
    }
#if GIO_PROFILING_MODE == 1
    endT = MPI_Wtime();
    if (fh->is_agg) gio_rd_time[3] += endT - startT;
    startT = endT;
#endif

    sts = (MPI_Status*) GIOI_Malloc(sizeof(MPI_Status) * (nsends + nrecvs));

    /* wait on the receives */
    if (nrecvs) {
        MPI_Waitall(nrecvs, reqs, sts);

        for (i=0; i<nrecvs; i++) {
#ifdef HAVE_MPI_LARGE_COUNT
            MPI_Offset count_recved;
            MPI_Get_count_c(&sts[i], MPI_BYTE, &count_recved);
#else
            int count_recved;
            MPI_Get_count(&sts[i], MPI_BYTE, &count_recved);
#endif
            recved_bytes += count_recved;
        }

        /* When buf is noncontiguous, copy data from recv_buf to buf */
        if (fh->bview.npairs > 1)
            fill_user_buffer(fh, buf, min_st_off, fd_size, fd_end,
                             recv_size, recd_from_proc, recv_buf);
    }

    /* wait on the sends */
#ifdef HAVE_MPI_STATUSES_IGNORE
    MPI_Waitall(nsends, reqs + nrecvs, MPI_STATUSES_IGNORE);
#else
    MPI_Waitall(nsends, reqs + nrecvs, sts + nrecvs);
#endif

#if GIO_PROFILING_MODE == 1
    endT = MPI_Wtime();
    if (fh->is_agg) gio_rd_time[4] += endT - startT;
#endif

    GIOI_Free(sts);
    GIOI_Free(reqs);
    GIOI_Free(recv_size);

    if (fh->bview.npairs > 1) { /* buffer view is noncontiguous */
        GIOI_Free(recv_buf[0]);
        GIOI_Free(recv_buf);
    }

    return recved_bytes;
}

/*----< Read_and_exch() >----------------------------------------------------*/
/* Each aggregator reads in sizes of no more than cb_buffer_size, an I/O info,
 * sends read data to the requesting processes. All processes place nonblocking
 * receive calls to receive read data into user buffer. The idea is to reduce
 * the amount of extra memory required for collective I/O. If all data were
 * read all at once, which is much easier, it would require a lot of temp space
 * allocated at the I/O aggregators, which is often unacceptable. For example,
 * to collectively read a distributed array from a file, where each local array
 * is 8 MiB and there are 8 processes per I/O aggregator, requiring at least
 * another 8 MiB of temp space may be unacceptable.
 */
static MPI_Offset
Read_and_exch(GIO_File          fh,
              void             *buf,
              const GIO_Access *others_req, /* IN: [nprocs] */
              MPI_Offset         min_st_off,
              MPI_Offset         fd_size,
              const MPI_Offset  *fd_end,     /* IN: [cb_nodes] */
              MPI_Offset        *buf_idx)    /* IN/OUT: [nprocs] */
{
    char *read_buf = NULL;
    int i, m, ntimes, max_ntimes, nprocs, myrank, cb_buffer_size;
    MPI_Offset st_loc, end_loc, round_end, rem_off, real_off;
    MPI_Offset rem_size, done;
    MPI_Offset real_size, for_curr_round, for_next_round, r_len, total_r_len=0;
    MPI_Offset j, *curr_offlen_ptr, *count, *send_size;
    MPI_Offset *partial_send, *recd_from_proc, *start_pos;

    MPI_Comm_size(fh->comm, &nprocs);
    MPI_Comm_rank(fh->comm, &myrank);

    /* Calculate the first and last file offsets (st_loc and end_loc,
     * respectively) this aggregator will read from the file.
     */
    st_loc = end_loc = -1;
    for (i=0; i<nprocs; i++) {
        /* Some processes may not have data for this aggregator */
        if (others_req[i].count) {
            st_loc = others_req[i].offsets[0];
            end_loc = others_req[i].offsets[0];
            break;
        }
    }
    for (; i<nprocs; i++) {
        for (j=0; j<others_req[i].count; j++) {
            st_loc = MIN(st_loc, others_req[i].offsets[j]);
            end_loc = MAX(end_loc, (others_req[i].offsets[j]
                                  + others_req[i].lens[j] - 1));
        }
    }

    /* Calculate the number of rounds of two-phase read, ntimes, each round an
     * aggregator reads an amount of no more than cb_buffer_size. Then, a call
     * to MPI_Allreduce() to obtain the max number of rounds, max_ntimes, among
     * all processes.
     */
    cb_buffer_size = fh->hints->cb_buffer_size;
    if ((st_loc == -1) && (end_loc == -1))
        /* this process does no I/O. */
        ntimes = 0;
    else
        /* ntimes is a ceiling */
        ntimes = (int) ((end_loc - st_loc + cb_buffer_size) / cb_buffer_size);

    MPI_Allreduce(&ntimes, &max_ntimes, 1, MPI_INT, MPI_MAX, fh->comm);

#if GIO_PROFILING_MODE == 1
    gio_rd_count[0] = MAX(gio_rd_count[0], max_ntimes);
#endif

    /* curr_offlen_ptr[] is the current offset-length pair in others_req[]
     * being processed for each process. It must be initialized to 0s.
     */
    curr_offlen_ptr = (MPI_Offset*) GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    /* start_pos[] stores the starting value of curr_offlen_ptr[] in a round.
     * It must be initialized to 0s.
     */
    start_pos = (MPI_Offset*) GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    /* If only a portion of the last offset-length pair is sent to an
     * aggregator in a round, then partial_send[] stores that send amount. It
     * must be initialized to 0s.
     */
    partial_send = (MPI_Offset*) GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    /* recd_from_proc[] stores the amount of data so far this aggregator has
     * received from each process. It will only be used and updated in
     * fill_user_buffer(). It must be initialized to 0s.
     */
    recd_from_proc = (MPI_Offset*) GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    /* count[] is the number of offset-length pairs of each process that will
     * be processes during a round. It must be initialized to 0s.
     */
    count = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * nprocs);

    /* Total size of data this rank will send to each aggregator in a round */
    send_size = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * nprocs);

    done = 0;
    rem_off = st_loc;
    for_curr_round = for_next_round = 0;

    for (m=0; m<ntimes; m++) {
        /* Each of ntimes rounds, an aggregator reads into buf of amount no
         * more than cb_buffer_size bytes. Each aggregator goes through all
         * others_req[] and check if any are satisfied by the current round of
         * read.
         */

        /* Since MPI standard requires that displacements in filetypes are
         * sorted in a monotonically non-decreasing order, I can maintain a
         * pointer, curr_offlen_ptr, to the current offset-length pair being
         * processed for each process in others_req[] and scan further only
         * from there.
         *
         * However, MPI standard allows overlaps in the filetypes, as described
         * as an example below: (1, 2, 3 are not process ranks. They are just
         * three offset-length pairs in a filetype.)
         *
         * pair 1  -------!--
         * pair 2    -----!----
         * pair 3       --!-----
         *
         * where '!' indicates where the current read_size limitation cuts
         * through the filetype. I resolve this by reading up to '!', but
         * filling the communication buffer only for pair 1. Move the portion
         * left over for 2 to the front of read_buf for use in the next round.
         * i.e., pair 2 and pair 3 will be satisfied in the next round. This
         * simplifies filling in the user's buf at the other end, as only one
         * offset-length pair with incomplete data will be sent. I also don't
         * need to send the individual offsets and lengths along with the data,
         * as the data is being sent in a particular order.
         *
         * rem_off   = start file offset for data actually read in this round
         * rem_size  = size of data to be read, corresponding to rem_off
         * real_off  = rem_off minus whatever data was retained in read_buf
         *             from the previous round for cases like pair 2, or pair
         *             3 illustrated above
         * real_size = size plus the extra corresponding to real_off
         */

        /* fh->io_buf has already been allocated at file open time and may be
         * re-allocated at the end of each round.
         */
        read_buf = fh->io_buf;

        rem_size = MIN(end_loc - st_loc + 1 - done, cb_buffer_size);

        round_end = rem_off + rem_size;

        for (i=0; i<nprocs; i++) {
            if (others_req[i].count == 0)
                continue;

            /* This should be only reachable by I/O aggregators only */
            for (j=curr_offlen_ptr[i]; j<others_req[i].count; j++) {
                if (others_req[i].offsets[j] + partial_send[i] < round_end) {
#if GIO_DEBUG_MODE == 1
                    assert(for_curr_round + rem_size <= cb_buffer_size);
#endif
                    r_len = GIO_UFS_read_contig(fh, read_buf + for_curr_round,
                                                rem_size, rem_off);
                    if (r_len < 0) {
                        total_r_len = r_len;
                        goto err_out;
                    }
                    rem_size = r_len;
                    goto done_read;
                }
            }
        }

done_read:
        real_off = rem_off - for_curr_round;
        real_size = rem_size + for_curr_round;
        for_next_round = 0;

        for (i=0; i<nprocs; i++) {
            count[i] = send_size[i] = 0;
            if (others_req[i].count == 0)
                continue;

            start_pos[i] = curr_offlen_ptr[i];
            for (j=curr_offlen_ptr[i]; j<others_req[i].count; j++) {
                MPI_Offset addr;
                MPI_Offset req_off;
                MPI_Offset req_len, rem_len;

                /* req_off is the file offset for offset-length pair j minus
                 * what has been satisfied in previous round
                 */
                if (partial_send[i]) {
                    /* This request may have been partially satisfied in the
                     * previous round.
                     */
                    req_off = others_req[i].offsets[j] + partial_send[i];
                    req_len = others_req[i].lens[j]    - partial_send[i];
                    partial_send[i] = 0;
                    /* modify the offset-length pair to reflect this change */
                    others_req[i].offsets[j] = req_off;
                    others_req[i].lens[j]    = req_len;
                } else {
                    req_off = others_req[i].offsets[j];
                    req_len = others_req[i].lens[j];
                }

                rem_len = real_off + real_size - req_off;
                if (rem_len <= 0)
                    break;

                /* now req_off < real_off + real_size */
                count[i]++;

#if GIO_DEBUG_MODE == 1
                assert(req_off - real_off <= cb_buffer_size);
#endif
                addr = (char*)read_buf + req_off - (char*)real_off;
                others_req[i].mem_ptrs[j] = addr;
                send_size[i] += MIN(rem_len, req_len);

                if (rem_len < req_len) {
#if GIO_DEBUG_MODE == 1
                    /* Overlapped in two consecutive offset-length pairs in
                     * fview should have already been removed in ina_get().
                     */
                    if (j + 1 < others_req[i].count &&
                        others_req[i].offsets[j + 1] < real_off + real_size) {
                        /* An overlap is found between pairs j and j+1. This is
                         * the case illustrated in the figure above.
                         */
                        for_next_round = MAX(for_next_round,
                                             real_off + real_size -
                                             others_req[i].offsets[j + 1]);
                        /* max because it must cover requests from different
                         * processes
                         */
                        assert(0);
                    }
#endif
                    partial_send[i] = rem_len;
                    break;
                }
            }
            curr_offlen_ptr[i] = j;
        }

        for_curr_round = for_next_round;

        /* carry out the communication phase */
        r_len = R_Exchange_data(fh, buf, send_size, count, start_pos,
                                partial_send, recd_from_proc, min_st_off,
                                fd_size, fd_end, others_req, buf_idx);
        total_r_len += r_len;

        if (for_next_round) {
            /* move remaining data to the front of fh->io_buf for next round */
#if GIO_DEBUG_MODE == 1
            assert(real_size - for_next_round <= cb_buffer_size);
#endif
            fh->io_buf = (char*) GIOI_Malloc(cb_buffer_size);
            memcpy(fh->io_buf, read_buf + real_size - for_next_round,
                   for_next_round);
            GIOI_Free(read_buf);
        }

        rem_off += rem_size;
        done += rem_size;
    }

    /* This process is done with its I/O, and must run the remaining rounds to
     * participate the collective communication in R_Exchange_data().
     */
    for (i=0; i<nprocs; i++)
        count[i] = send_size[i] = 0;

    for (m=ntimes; m<max_ntimes; m++) {
        /* nothing to send, but check for recv. */
        r_len = R_Exchange_data(fh, buf, send_size, count, start_pos,
                                partial_send, recd_from_proc, min_st_off,
                                fd_size, fd_end, others_req, buf_idx);
        total_r_len += r_len;
    }

err_out:
    GIOI_Free(send_size);
    GIOI_Free(count);
    GIOI_Free(recd_from_proc);
    GIOI_Free(partial_send);
    GIOI_Free(start_pos);
    GIOI_Free(curr_offlen_ptr);

    /* If successful, total_r_len is the amount received from I/O aggregators
     * plus the one read by self, if self is an aggregator, representing this
     * rank's read amount. Otherwise, it is a NetCDF error code (negative
     * value).
     */
    return total_r_len;
}

/*----< offset_compare() >---------------------------------------------------*/
/* This subroutine is used to sort st_end_all[nprocs] */
static int
offset_compare(const void *a, const void *b)
{
    if (*(MPI_Offset*)a > *(MPI_Offset*)b) return (1);
    if (*(MPI_Offset*)a < *(MPI_Offset*)b) return (-1);
    return (0);
}

/*----< GIO_UFS_read_coll() >----------------------------------------------*/
MPI_Offset
GIO_UFS_read_coll(GIO_File  fh,
                  void     *buf)
{
    /* Uses a generalized version of the extended two-phase method described in
     * "An Extended Two-Phase Method for Accessing Sections of Out-of-Core
     * Arrays", Rajeev Thakur and Alok Choudhary, Scientific Programming,
     * (5)4:301--317, Winter 1996.
     * http://www.mcs.anl.gov/home/thakur/ext2ph.ps
     */

    /* my_req contains access structures of this rank, describing the request
     * offset-length pairs that fall into each aggregator's file domain.
     */
    GIO_Access *my_req;

    /* others_req contains access structures of all processes whose requests
     * fall into this aggregator's file domain. It is only relevant of this
     * rank is an I/O aggregator.
     */
    GIO_Access *others_req;

    int i, nprocs, rank, interleave_count = 0;
    MPI_Offset *buf_idx = NULL;
    MPI_Offset *count_per_aggr, my_req_naggr;
    MPI_Offset min_st_off=0, max_end_off=LLONG_MAX, *fd_end=NULL;
    MPI_Offset fd_size, r_len, total_r_len=0;

#if GIO_PROFILING_MODE == 1
double curT = MPI_Wtime();
#endif

    MPI_Comm_size(fh->comm, &nprocs);
    MPI_Comm_rank(fh->comm, &rank);

    /* Unlike MPI-IO, GIO never reuses a file view across two or more GIO
     * calls. As fh->fview will be reset right after this subroutine returns,
     * it can be modify within this subroutine.
     */

#if GIO_DEBUG_MODE == 1
    if (fh->fview.size > 0) {
        assert(fh->fview.npairs > 0);
        assert(fh->fview.off != NULL);
        assert(fh->fview.len != NULL);
    }
    assert(fh->fview.size == fh->bview.size);
#endif

    /* only check for interleaving if cb_read isn't disabled */
    if (fh->hints->cb_read != GIO_HINT_DISABLE) {
        MPI_Offset *st_end_all;

        /* Calculate the aggregate access region of this rank's request, which
         * represents a file range from the very first byte offset accessed by
         * this rank till the end offset (exclusively).
         *
         * Note fview.off[] is always relative to the beginning of file.
         *
         * All processes gather the aggregate access regions of all other
         * processes in order to tell whether there is an interleaving access
         * among all.
         */
        st_end_all = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * 2 * nprocs);
        if (fh->fview.size == 0)
            /* set to -1 to indicate zero-sized request */
            st_end_all[2*rank] = st_end_all[2*rank+1] = -1;
        else {
            st_end_all[2*rank]   = fh->fview.off[0];
            st_end_all[2*rank+1] = fh->fview.off[fh->fview.npairs-1]
                                 + fh->fview.len[fh->fview.npairs-1];
        }

        MPI_Allgather(MPI_IN_PLACE, 0, MPI_DATATYPE_NULL, st_end_all, 2,
                      MPI_OFFSET, fh->comm);

        /* Check whether access ranges of all processes are interleaved. In
         * the meantime, find global minimum starting offset and maximum end
         * offset:
         *   min_st_off  - starting file offset of the aggregate access region
         *   max_end_off - end file offset of the aggregate access region
         */
        qsort(st_end_all, nprocs, sizeof(MPI_Offset)*2, offset_compare);

        for (i=0; i<2*nprocs; i+=2) { /* find the 1st non-zero sized */
            if (st_end_all[i] >= 0) {
                min_st_off  = st_end_all[i];
                max_end_off = st_end_all[i+1];
                break;
            }
        }
        for (i+=2; i<2*nprocs; i+=2) {
            if (st_end_all[i] == -1) /* skip zero-sized request */
                continue;
            if (st_end_all[i] <  st_end_all[i-1] &&
                st_end_all[i] <= st_end_all[i+1])
                interleave_count++;
            min_st_off  = MIN(min_st_off,  st_end_all[i]);
            max_end_off = MAX(max_end_off, st_end_all[i+1]);
        }
        GIOI_Free(st_end_all);

        /* Check if this collective read is entirely zero-sized. */
        if (min_st_off == -1 && max_end_off == -1) {
#if GIO_DEBUG_MODE == 1
            /* Warn a zero-sized collective request */
            if (rank == 0)
                printf("%s at %d: zero--sized collective read!\n",
                       __func__,__LINE__);
#endif
            return 0;
        }
    }

    if (fh->hints->cb_read == GIO_HINT_DISABLE ||
        (!interleave_count && fh->hints->cb_read == GIO_HINT_AUTO)) {
        /* switch to perform independent read */

        if (fh->fview.npairs == 0) /* zero-sized request */
            return 0;

        if (!fh->is_open) {
            /* If file has not been opened (only happen to non-I/O
             * aggregators), open it now and obtain hint striping_unit.
             */
            int err;
            if (fh->fstype == GIO_FS_LUSTRE)
                /* This subroutine is also used by Lustre's collective read. */
                err = GIOI_Lustre_open_on_demand(fh);
            else if (fh->fstype == GIO_FS_UFS)
                err = GIOI_UFS_open_on_demand(fh);
            else
                err = GIO_EFSTYPE;

            if (err != GIO_NOERR)
                return err;
        }

// if (rank == 0) printf("%s %d: SWITCH to GIO_UFS_read_indep !!!\n",__func__,__LINE__);
        return GIO_UFS_read_indep(fh, buf);
    }
    /* We now proceed to perform two-phase I/O.
     *
     * At first, a call to GIO_Calc_file_domains() to calculate the file
     * domains assigned to each I/O aggregator. fh->hints->cb_nodes is the
     * number of aggregators. The aggregate access region of this collective
     * read call is divided among all aggregators into a set of disjoined file
     * domains. A file domain (denoted as 'fd') is the set of file regions an
     * aggregator is responsible for their file access. Thus, a file domain is
     * only relevant to I/O aggregators. All processes must send their requests
     * to an aggregator for the portions that fall into the aggregator's file
     * domain. Non-aggregators are not assigned a file domain. fh->is_agg
     * tells whether this rank is an aggregator.
     *
     * GIO_Calc_file_domains() set the following 2 variables:
     *   fd_end[cb_nodes] - end location of file domains, inclusive offsets.
     *      The values are indexed by an aggregator number; they needs to be
     *      mapped to actual rank IDs in the communicator later.
     *   fd_size - average size (ceiling) of file domain among cb_nodes.
     */
    GIO_Calc_file_domains(fh->hints->cb_nodes, fh->hints->striping_unit,
                          min_st_off, max_end_off, &fd_end, &fd_size);

    /* GIO_Calc_my_req() calculates what portions of this rank's requests
     * fall into every aggregator's file domains. It sets the following
     * variables:
     *   my_req_naggr - number of aggregators for which this rank has a portion
     *      of its request falling into their file domains
     *   count_per_aggr[nprocs] - number of contiguous offset-length pairs of
     *      this rank's request that fall into the aggregators' file domains
     *   my_req[nprocs] - metadata describing this rank's requests to be carried
     *      out by each I/O aggregator.
     *   buf_idx[nprocs] - indices into the user buffer that can be directly
     *      used to perform file read/write. Note this is only relevant when
     *      the user buffer is contiguous.
     */
    count_per_aggr = GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    GIO_Calc_my_req(fh, min_st_off, fd_end, fd_size, &my_req_naggr,
                    count_per_aggr, &my_req, &buf_idx);

    /* GIO_Calc_others_req() produces results that are only relevant to the I/O
     * aggregators. Based on every rank's my_req, it calculates what portions
     * of requests from all processes that fall into this aggregator's file
     * domain. Note MPI communication is performed inside this subroutine. It
     * sets the following variable:
     *   others_req[nprocs] - metadata describing all processes' requests to be
     *      carried out by this aggregator.
     */
    GIO_Calc_others_req(fh, my_req_naggr, count_per_aggr, my_req, &others_req);

    GIOI_Free(count_per_aggr);

#if GIO_PROFILING_MODE == 1
    if (fh->is_agg) gio_rd_time[1] += MPI_Wtime() - curT;
#endif

    /* read data in sizes of no more than collective buffer size, communicate
     * to exchange read data, and fill user buf.
     */
    r_len = Read_and_exch(fh, buf, others_req, min_st_off, fd_size, fd_end,
                          buf_idx);
    if (r_len > 0) total_r_len += r_len;

    /* free all memory allocated for collective I/O */
    if (fd_end != NULL) GIOI_Free(fd_end);
    GIOI_Free(my_req[0].offsets);
    GIOI_Free(my_req);
    GIOI_Free(buf_idx);
    GIOI_Free(others_req[0].offsets);
    GIOI_Free(others_req);

#if GIO_PROFILING_MODE == 1
    if (fh->is_agg) gio_rd_time[0] += MPI_Wtime() - curT;
#endif

    /* All PnetCDF error codes are negative. */
    return (r_len < 0) ? r_len : total_r_len;
}

