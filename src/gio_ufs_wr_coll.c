/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>  /* memcpy() */
#include <limits.h>  /* LLONG_MAX, INT_MAX */
#include <assert.h>

#include "gioi.h"

#define BUF_INCR {                                      \
    while (buf_incr) {                                  \
        size_in_buf = MIN(buf_incr, buf_rem);           \
        user_buf_idx += size_in_buf;                    \
        buf_rem -= size_in_buf;                         \
        buf_incr -= size_in_buf;                        \
        if (buf_incr > 0 && buf_rem == 0) {             \
            buf_indx++;                                 \
            user_buf_idx = fh->bview.off[buf_indx];     \
            buf_rem = fh->bview.len[buf_indx];          \
        }                                               \
    }                                                   \
}

#define BUF_COPY {                                      \
    while (size) {                                      \
        size_in_buf = MIN(size, buf_rem);               \
        memcpy(&send_buf[aggr][send_buf_idx[aggr]],     \
               (char*)buf + user_buf_idx, size_in_buf); \
        send_buf_idx[aggr] += size_in_buf;              \
        user_buf_idx += size_in_buf;                    \
        buf_rem -= size_in_buf;                         \
        size -= size_in_buf;                            \
        buf_incr -= size_in_buf;                        \
        if (size > 0 && buf_rem == 0) {                 \
            buf_indx++;                                 \
            user_buf_idx = fh->bview.off[buf_indx];     \
            buf_rem = fh->bview.len[buf_indx];          \
        }                                               \
    }                                                   \
    BUF_INCR                                            \
}

/*----< fill_send_buffer() >-------------------------------------------------*/
/* This subroutine is only called when buffer view is not contiguous. */
static int
fill_send_buffer(GIO_File          fh,
                 const void       *buf,
                 MPI_Offset        min_st_off,
                 MPI_Offset        fd_size,
                 const MPI_Offset *fd_end,       /* IN: [cb_nodes] */
                 const MPI_Offset *send_size,    /* IN: [nprocs] */
                 MPI_Offset       *sent_to_proc, /* IN/OUT: [nprocs] */
                 char *const      *send_buf,     /* OUT: [nprocs] */
                 MPI_Request      *reqs)         /* OUT: [nprocs] */
{
    int i, k, nprocs, myrank, err, aggr;
    MPI_Offset j, off, len, buf_indx, buf_rem, size_in_buf, buf_incr, size;
    MPI_Offset rem_len, user_buf_idx, *curr_to, *done_to, *send_buf_idx;

    MPI_Comm_size(fh->comm, &nprocs);
    MPI_Comm_rank(fh->comm, &myrank);

    /* curr_to[nprocs] - amount of data sent to each rank that has already been
     *      accounted for so far.
     * done_to[nprocs] - amount of data already sent to each rank in previous
     *      round.
     * user_buf_idx - current location in user buffer
     * send_buf_idx[nprocs] = current location in send_buf of each rank
     */
    curr_to = GIOI_Malloc(sizeof(MPI_Offset) * nprocs * 3);
    done_to = curr_to + nprocs;
    send_buf_idx = done_to + nprocs;

    for (i=0; i<nprocs; i++) {
        send_buf_idx[i] = curr_to[i] = 0;
        done_to[i] = sent_to_proc[i];
    }

    /* buf_indx - index bview's offset-length pairs being processed
     * buf_rem - remaining length of the current offset-length pair
     */
    user_buf_idx = fh->bview.off[0];
    buf_indx = 0;
    buf_rem = fh->bview.len[0];

    k = 0;
    for (j=0; j<fh->fview.npairs; j++) {
        off = fh->fview.off[j];
        rem_len = fh->fview.len[j];

        /* this request may span file domains of more than one aggregator */
        while (rem_len != 0) {
            len = rem_len;

            /* NOTE: len value will be modified by GIOI_Calc_aggregator() to
             * be no more than the single file domain that aggregator 'aggr'
             * is responsible for.
             */
            aggr = GIOI_Calc_aggregator(fh->hints->striping_unit,
                                        fh->hints->cb_nodes,
                                        fh->hints->aggr_ranks, min_st_off,
                                        fd_size, fd_end, off, &len);

            if (send_buf_idx[aggr] < send_size[aggr]) {
                if (curr_to[aggr] + len > done_to[aggr]) {
                    if (done_to[aggr] > curr_to[aggr]) {
                        size = MIN(curr_to[aggr] + len - done_to[aggr],
                                   send_size[aggr] - send_buf_idx[aggr]);
                        buf_incr = done_to[aggr] - curr_to[aggr];
                        BUF_INCR
                        buf_incr = curr_to[aggr] + len - done_to[aggr];
                        curr_to[aggr] = done_to[aggr] + size;
                        BUF_COPY
                    } else {
                        size = MIN(len, send_size[aggr] - send_buf_idx[aggr]);
                        buf_incr = len;
                        curr_to[aggr] += size;
                        BUF_COPY
                    }
                    if (send_buf_idx[aggr] == send_size[aggr] &&
                        aggr != myrank) {
                        if (send_size[aggr] > INT_MAX) {
#ifdef HAVE_MPI_LARGE_COUNT
                            err = MPI_Isend_c(send_buf[aggr], send_size[aggr],
                                              MPI_BYTE, aggr, 0, fh->comm,
                                              &reqs[k++]);
                            err = GIOI_error_mpi(err, "MPI_Isend_c");
#else
                            err = GIO_EINTOVERFLOW;
#endif
                        }
                        else {
                            int nelems = (int)send_size[aggr];
                            err = MPI_Isend(send_buf[aggr], nelems, MPI_BYTE,
                                            aggr, 0, fh->comm, &reqs[k++]);
                            err = GIOI_error_mpi(err, "MPI_Isend");
                        }
                        if (err != GIO_NOERR) return err;
                    }
                } else {
                    curr_to[aggr] += len;
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
        if (send_size[i])
            sent_to_proc[i] = curr_to[i];

    GIOI_Free(curr_to);

    return GIO_NOERR;
}

/*----< W_Exchange_data() >--------------------------------------------------*/
static MPI_Offset
W_Exchange_data(GIO_File          fh,
                const void       *buf,          /* user buffer */
                char             *write_buf,    /* collective buffer */
                const MPI_Offset *recv_size,    /* IN: [nprocs] */
                MPI_Offset        rem_off,
                MPI_Offset        rem_size,
                const MPI_Offset *count,        /* IN: [nprocs] */
                const MPI_Offset *start_pos,    /* IN: [nprocs] */
                const MPI_Offset *partial_recv, /* IN: [nprocs] */
                MPI_Offset       *sent_to_proc, /* IN/OUT: [nprocs] */
                MPI_Offset        min_st_off,
                MPI_Offset        fd_size,
                const MPI_Offset *fd_end,       /* IN: [cb_nodes] */
                GIOI_Access      *others_req,   /* IN/OUT: [nprocs] */
                MPI_Offset       *buf_idx)      /* IN/OUT: [nprocs] */
{
    char **send_buf = NULL;
    int i, j, nprocs, myrank, err=GIO_NOERR;
    int nrecvs, nsends, num_rtypes, nreqs, hole;
    MPI_Request *reqs, *send_req;
    MPI_Datatype *recv_types, self_recv_type=MPI_DATATYPE_NULL;
    MPI_Offset *send_size, sum, *srt_len=NULL, *tmp_len, *srt_off=NULL;

#if GIO_PROFILING_MODE == 1
    double curT = MPI_Wtime();
#endif

    MPI_Comm_size(fh->comm, &nprocs);
    MPI_Comm_rank(fh->comm, &myrank);

    /* Exchange recv_size info so that each aggregator knows how much to send
     * to whom and how much memory to allocate.
     */
    send_size = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * nprocs);
    MPI_Alltoall(recv_size, 1, MPI_OFFSET, send_size, 1, MPI_OFFSET, fh->comm);

    /* construct derived datatypes for recv */
    recv_types = (MPI_Datatype*) GIOI_Malloc(sizeof(MPI_Datatype) * nprocs);

    tmp_len = GIOI_Malloc(sizeof(MPI_Offset) * nprocs);
    j = 0;
    nsends = 0;
    nrecvs = 0;
    sum = 0;
    for (i=0; i<nprocs; i++) {
        sum += count[i];
        if (send_size[i])
            nsends++;

        if (recv_size[i]) {
            MPI_Datatype *dtype;

            nrecvs++;
            dtype = (i != myrank) ? (recv_types + j) : (&self_recv_type);

            if (partial_recv[i]) {
                /* take care if the last off-len pair is a partial recv */
                MPI_Offset k = start_pos[i] + count[i] - 1;
                tmp_len[i] = others_req[i].len[k];
                others_req[i].len[k] = partial_recv[i];
            }

            /* absolute displacements; use MPI_BOTTOM in recv */
            err = GIOI_type_create_hindexed(count[i],
                       &(others_req[i].ptr[start_pos[i]]),
                       &(others_req[i].len[start_pos[i]]),
                       dtype);
            if (err != GIO_NOERR) return err;

            if (i != myrank)
                j++;
        }
    }
    num_rtypes = j;     /* number of non-self receive datatypes created */

    /* To avoid a read-modify-write, check if there are holes in the data to be
     * written. For this, merge the (sorted) offset lists others_req using a
     * heap-merge sort.
     */

    if (sum) {
#if GIO_PROFILING_MODE == 1
        double timing = MPI_Wtime();
#endif
        srt_off = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * sum);
        srt_len = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * sum);

/* TODO: GIOI_Heap_merge is expensive, borrow codes from ad_lustre_wrcoll.c to skip it when possible */

        /* Skip hole checking if there is no write data by this aggregator */
        GIOI_Heap_merge(others_req, count, srt_off, srt_len, start_pos, nprocs,
                       nrecvs, sum);
#if GIO_PROFILING_MODE == 1
        if (fh->is_agg) gio_wr_time[5] += MPI_Wtime() - timing;
#endif
    }

    /* for partial recvs, restore original lengths */
    for (i=0; i<nprocs; i++)
        if (partial_recv[i])
            others_req[i].len[start_pos[i] + count[i] - 1] = tmp_len[i];

    GIOI_Free(tmp_len);

    /* Check if there are any holes. If yes, must do read-modify-write.
     *
     * Holes can occur in three places. 'middle' is what you'd expect: the
     * processes are operating on noncontiguous data. However, holes can also
     * show up at the beginning or end of the file domain. Missing these holes
     * would result in us writing more data than received by everyone else.
     */
    hole = 0;
    if (sum) {
        if (rem_off != srt_off[0])  /* hole at the front */
            hole = 1;
        else {  /* coalesce the sorted offset-length pairs */
            MPI_Offset k, new_len;
            for (k=1; k<sum; k++) {
                if (srt_off[k] <= srt_off[0] + srt_len[0]) {
                    new_len = srt_off[k] + srt_len[k] - srt_off[0];
                    if (new_len > srt_len[0])
                        srt_len[0] = new_len;
                } else
                    break;
            }
            if (i < sum || rem_size != srt_len[0])  /* hole in middle or end */
                hole = 1;
        }

        GIOI_Free(srt_off);
        GIOI_Free(srt_len);
    }

    if (nrecvs && hole) {
        MPI_Offset r_len;
        r_len = GIOI_UFS_read_contig(fh, write_buf, rem_size, rem_off);
        if (r_len < 0) return r_len;
    }

    if (fh->atomicity) {
        /* nreqs is the number of Isend and Irecv to be posted */
        nreqs = (send_size[myrank]) ? (nsends - 1) : nsends;
        reqs = (MPI_Request*) GIOI_Malloc(sizeof(MPI_Request) * nreqs);
        send_req = reqs;
    } else {
        nreqs = nsends + nrecvs;
        if (send_size[myrank])  /* NO send to and recv from self */
            nreqs -= 2;
        reqs = (MPI_Request*) GIOI_Malloc(sizeof(MPI_Request) * nreqs);

        /* post receives */
        j = 0;
        for (i=0; i<nprocs; i++) {
            if (recv_size[i] == 0)
                continue;
            if (i != myrank) {
                MPI_Irecv(MPI_BOTTOM, 1, recv_types[j], i, 0, fh->comm,
                          &reqs[j]);
                j++;
            } else if (fh->bview.npairs <= 1) {
                /* sen/recv to/from self uses MPI_Unpack() */
                if (recv_size[i] > INT_MAX) {
#ifdef HAVE_MPI_LARGE_COUNT
                    MPI_Count pos=0;
                    err = MPI_Unpack_c((char*)buf + buf_idx[i], recv_size[i],
                                       &pos, write_buf, 1, self_recv_type,
                                       MPI_COMM_SELF);
                    err = GIOI_error_mpi(err, "MPI_Unpack_c");
#else
                    err = GIO_EINTOVERFLOW;
#endif
                }
                else {
                    int pos=0, nelems = (int)recv_size[i];
                    err = MPI_Unpack((char*)buf + buf_idx[i], nelems, &pos,
                                     write_buf, 1, self_recv_type,
                                     MPI_COMM_SELF);
                    err = GIOI_error_mpi(err, "MPI_Unpack_c");
                }
                if (err != GIO_NOERR) return err;

                buf_idx[i] += recv_size[i];
            }
        }
        send_req = reqs + j;
    }

    /* Post nonblocking send calls. If buffer view is contiguous, data can be
     * directly sent from user buf at location given by buf_idx. Otherwise,
     * allocate send_buf and use it to send.
     */
    if (fh->bview.npairs <= 1) {
        j = 0;
        for (i=0; i<nprocs; i++) {
            if (send_size[i] == 0 || i == myrank) continue;
#if GIO_DEBUG_MODE == 1
            assert(buf_idx[i] != -1);
#endif
            if (send_size[i] > INT_MAX) {
#ifdef HAVE_MPI_LARGE_COUNT
                err = MPI_Isend_c((char*)buf + buf_idx[i], send_size[i],
                                  MPI_BYTE, i, 0, fh->comm, &send_req[j++]);
                err = GIOI_error_mpi(err, "MPI_Isend_c");
#else
                err = GIO_EINTOVERFLOW;
#endif
            }
            else {
                int nelems = (int)send_size[i];
                err = MPI_Isend((char*)buf + buf_idx[i], nelems, MPI_BYTE,
                                i, 0, fh->comm, &send_req[j++]);
                err = GIOI_error_mpi(err, "MPI_Isend");
                buf_idx[i] += send_size[i];
            }
            if (err != GIO_NOERR) return err;
        }
    }
    else if (nsends) {
        /* buffer view is not contiguous */
        size_t msgLen = 0;
        for (i=0; i<nprocs; i++)
            msgLen += send_size[i];
        send_buf = (char**) GIOI_Malloc(sizeof(char*) * nprocs);
        send_buf[0] = (char*) GIOI_Malloc(msgLen);
        for (i=1; i<nprocs; i++)
            send_buf[i] = send_buf[i - 1] + send_size[i - 1];

        err = fill_send_buffer(fh, buf, min_st_off, fd_size, fd_end,
                               send_size, sent_to_proc, send_buf, send_req);
        if (err != GIO_NOERR) return err;

        /* the send is done in fill_send_buffer() */
    }

    if (fh->atomicity) {
        /* In atomic mode, we must use blocking receives to receive data in the
         * same increasing order of MPI process rank IDs.
         */
        j = 0;
        for (i=0; i<nprocs; i++) {
            if (recv_size[i] == 0)
                continue;
            if (i != myrank) {
                MPI_Status st;
                MPI_Recv(MPI_BOTTOM, 1, recv_types[j++], i, 0, fh->comm, &st);
            } else {
                /* sen/recv to/from self uses MPI_Unpack() */
                char *ptr = (fh->bview.npairs <= 1) ? (char*)buf + buf_idx[i]
                                                    : send_buf[i];
#if GIO_DEBUG_MODE == 1
                assert(self_recv_type != MPI_DATATYPE_NULL);
#endif

                if (recv_size[i] > INT_MAX) {
#ifdef HAVE_MPI_LARGE_COUNT
                    MPI_Count pos=0;
                    err = MPI_Unpack_c(ptr, recv_size[i], &pos, write_buf, 1,
                                       self_recv_type, MPI_COMM_SELF);
                    err = GIOI_error_mpi(err, "MPI_Unpack_c");
#else
                    err = GIO_EINTOVERFLOW;
#endif
                }
                else {
                    int pos=0, nelems=(int)recv_size[i];
                    err = MPI_Unpack(ptr, nelems, &pos, write_buf, 1,
                                     self_recv_type, MPI_COMM_SELF);
                    err = GIOI_error_mpi(err, "MPI_Unpack");
                }
                if (err != GIO_NOERR) return err;

                buf_idx[i] += recv_size[i];
            }
        }
    }
    else if (fh->bview.npairs > 1 && recv_size[myrank]) {
#if GIO_DEBUG_MODE == 1
        assert(self_recv_type != MPI_DATATYPE_NULL);
#endif

        if (recv_size[myrank] > INT_MAX) {
#ifdef HAVE_MPI_LARGE_COUNT
            MPI_Count pos=0;
            err = MPI_Unpack_c(send_buf[myrank], recv_size[myrank], &pos,
                               write_buf, 1, self_recv_type, MPI_COMM_SELF);
            err = GIOI_error_mpi(err, "MPI_Unpack_c");
#else
            err = GIO_EINTOVERFLOW;
#endif
        }
        else {
            int pos=0, nelems=(int)recv_size[myrank];

            err = MPI_Unpack(send_buf[myrank], nelems, &pos, write_buf,
                             1, self_recv_type, MPI_COMM_SELF);
            err = GIOI_error_mpi(err, "MPI_Unpack");
        }
        if (err != GIO_NOERR) return err;
    }

    for (i=0; i<num_rtypes; i++)
        MPI_Type_free(recv_types + i);
    GIOI_Free(recv_types);

    if (self_recv_type != MPI_DATATYPE_NULL)
        MPI_Type_free(&self_recv_type);

#if GIO_PROFILING_MODE == 1
    if (fh->is_agg) gio_wr_time[4] += MPI_Wtime() - curT;
    curT = MPI_Wtime();
#endif

#ifdef HAVE_MPI_STATUSES_IGNORE
    MPI_Waitall(nreqs, reqs, MPI_STATUSES_IGNORE);
#else
    MPI_Status *sts = (MPI_Status*) GIOI_Malloc(sizeof(MPI_Status) * nreqs);
    MPI_Waitall(nreqs, reqs, sts);
    GIOI_Free(sts);
#endif

#if GIO_PROFILING_MODE == 1
    if (fh->is_agg) gio_wr_time[3] += MPI_Wtime() - curT;
#endif

    GIOI_Free(reqs);
    if (fh->bview.npairs > 1 && nsends) {
        GIOI_Free(send_buf[0]);
        GIOI_Free(send_buf);
    }
    GIOI_Free(send_size);

    return err;
}

/*----< Exch_and_write() >---------------------------------------------------*/
/* Each process sends its portions of write requests to I/O aggregators based
 * on their file domain assignment. Note "portions" not entire means to ensure
 * that each I/O aggregator will receive from all processes a total amount of
 * write data of no more than cb_buffer_size per two-phase I/O round. At the
 * end of each round, aggregators write to the file of size no more than amount
 * of cb_buffer_size.
 * The idea of 'cb_buffer_size' hint is to reduce the amount of extra memory
 * required for collective I/O. If all data were written all at once, which is
 * much easier, it would require a lot of temp space allocated at the I/O
 * aggregators, which is often unacceptable. For example, to collectively write
 * a distributed array to a file, where each local array is 8 MiB and there are
 * 8 processes per I/O aggregator, requiring at least another 8 MiB of temp
 * space may be unacceptable.
 */
static MPI_Offset
Exch_and_write(GIO_File          fh,
               const void       *buf,
               MPI_Offset        min_st_off,
               MPI_Offset        fd_size,
               const MPI_Offset *fd_end,     /* IN: [cb_nodes] */
               GIOI_Access      *others_req, /* IN/OUT: [nprocs] */
               MPI_Offset       *buf_idx)    /* IN/OUT: [nprocs] */
{
    char *write_buf = NULL;
    int i, m, ntimes, max_ntimes, nprocs, myrank, do_write, cb_buffer_size;
    MPI_Offset st_loc, end_loc, round_end, rem_off;
    MPI_Offset rem_size, done, w_len, total_w_len=0;
    MPI_Offset j, *curr_offlen_ptr, *count, *recv_size;
    MPI_Offset *partial_recv, *sent_to_proc, *start_pos;

    MPI_Comm_size(fh->comm, &nprocs);
    MPI_Comm_rank(fh->comm, &myrank);

    /* Calculate the first and last file offsets (st_loc and end_loc,
     * respectively) this aggregator will write from the file.
     */
    st_loc = end_loc = -1;
    for (i=0; i<nprocs; i++) {
        /* Some processes may not have data for this aggregator */
        if (others_req[i].num) {
            st_loc  = others_req[i].off[0];
            end_loc = others_req[i].off[0];
            break;
        }
    }
    for (i=0; i<nprocs; i++)
        for (j=0; j<others_req[i].num; j++) {
            st_loc  = MIN(st_loc, others_req[i].off[j]);
            end_loc = MAX(end_loc, (others_req[i].off[j]
                                  + others_req[i].len[j] - 1));
        }

    /* Calculate the number of rounds of two-phase write, ntimes, each round an
     * aggregator writes an amount of no more than cb_buffer_size. Then, a call
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
    gio_wr_count[0] = MAX(gio_wr_count[0], max_ntimes);
#endif

    /* curr_offlen_ptr[] is the current off-len pair in others_req[] being
     * processed for each process. It must be initialized to 0s.
     */
    curr_offlen_ptr = (MPI_Offset*) GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    /* start_pos[] stores the starting value of curr_offlen_ptr[] in a round.
     * It must be initialized to 0s.
     */
    start_pos = (MPI_Offset*) GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    /* If only a portion of the last offset-length pair is received from an
     * aggregator in a round, then partial_recv[] stores that receive amount.
     * It must be initialized to 0s.
     */
    partial_recv = (MPI_Offset*) GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    /* sent_to_proc[] stores the amount of data so far this aggregator has sent
     * to each process. It will be only used and updated in fill_send_buffer().
     * It must be initialized to 0s.
     */
    sent_to_proc = (MPI_Offset*) GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    /* count[] is the number of offset-length pairs of each process that will
     * be processes during a round. It must be initialized to 0s.
     */
    count = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * nprocs);

    /* Total size of data this rank will receive from each aggregator in a
     * round.
     */
    recv_size = (MPI_Offset*) GIOI_Malloc(sizeof(MPI_Offset) * nprocs);

    done = 0;
    rem_off = st_loc;

    /* fh->io_buf has already been allocated at file open time. */
    write_buf = fh->io_buf;

    for (m=0; m<ntimes; m++) {
        /* Go through all others_req and check which will be satisfied by the
         * current round.
         *
         * Note that MPI standard requires that displacements in filetypes are
         * sorted in a monotonically non-decreasing order and that, for writes,
         * the filetypes cannot specify overlapping regions in the file. This
         * simplifies implementation a bit compared to reads.
         *
         * rem_off  = start file offset for data actually written in this round
         * rem_size = size of data to be written, corresponding to rem_off
         */

        /* first calculate what should be communicated */
        for (i=0; i<nprocs; i++)
            count[i] = recv_size[i] = 0;

        do_write = 0; /* will change to 1 if any of count[i] becomes > 0 */

        rem_size = MIN(end_loc - st_loc + 1 - done, cb_buffer_size);

        round_end = rem_off + rem_size;

        for (i=0; i<nprocs; i++) {
            if (others_req[i].num == 0)
                continue;

            start_pos[i] = curr_offlen_ptr[i];
            for (j=curr_offlen_ptr[i]; j<others_req[i].num; j++) {
                MPI_Offset req_off, req_len;

                /* req_off is the file offset for offset-length pair j minus
                 * what has been satisfied in previous round
                 */
                if (partial_recv[i]) {
                    /* this request may have been partially satisfied in the
                     * previous iteration.
                     */
                    req_off = others_req[i].off[j] + partial_recv[i];
                    req_len = others_req[i].len[j] - partial_recv[i];
                    partial_recv[i] = 0;
                    /* modify the off-len pair to reflect this change */
                    others_req[i].off[j] = req_off;
                    others_req[i].len[j] = req_len;
                } else {
                    req_off = others_req[i].off[j];
                    req_len = others_req[i].len[j];
                }

                if (req_off >= round_end)
                    break;

                /* Now req_off < round_end */
                count[i]++;
                do_write = 1;

                if (myrank != i)
                    others_req[i].ptr[j] = (char*)write_buf + req_off
                                         - (char*)rem_off;
                else
                    others_req[i].ptr[j] = req_off - rem_off;
                recv_size[i] += MIN(round_end - req_off, req_len);

                if (round_end - req_off < req_len) {
#if GIO_DEBUG_MODE == 1
                    /* Overlapped in two consecutive offset-length pairs in
                     * fview should have already been removed in ina_put().
                     */
                    if (j + 1 < others_req[i].num &&
                        others_req[i].off[j + 1] < round_end) {
                        /* An overlap is found between pairs j and j+1. */
                        fprintf(stderr, "Filetype contains overlapping write regions (which is illegal according to the MPI standard\n");
                        assert(0);
                    }
#endif
                    partial_recv[i] = round_end - req_off;
                    break;
                }
            }
            curr_offlen_ptr[i] = j;
        }

        w_len = W_Exchange_data(fh, buf, write_buf, recv_size, rem_off,
                                rem_size, count, start_pos, partial_recv,
                                sent_to_proc, min_st_off, fd_size, fd_end,
                                others_req, buf_idx);
        if (w_len < 0) {
            total_w_len = w_len;
            goto err_out;
        }
        else
            total_w_len += w_len;

        if (do_write) {
            w_len = GIOI_UFS_write_contig(fh, write_buf, rem_size, rem_off, 1);
            if (w_len < 0) {
                total_w_len = w_len;
                goto err_out;
            }
            else
                total_w_len += w_len;
        }

        rem_off += rem_size;
        done += rem_size;
    }

    for (i=0; i<nprocs; i++)
        count[i] = recv_size[i] = 0;
    for (m=ntimes; m<max_ntimes; m++) {
        /* nothing to recv, but check for send. */
        rem_size = 0;
        w_len = W_Exchange_data(fh, buf, write_buf, recv_size, rem_off,
                                rem_size, count, start_pos, partial_recv,
                                sent_to_proc, min_st_off, fd_size, fd_end,
                                others_req, buf_idx);
        if (w_len < 0) {
            total_w_len = w_len;
            goto err_out;
        }
        else
            total_w_len += w_len;
    }

err_out:
    GIOI_Free(recv_size);
    GIOI_Free(count);
    GIOI_Free(sent_to_proc);
    GIOI_Free(partial_recv);
    GIOI_Free(start_pos);
    GIOI_Free(curr_offlen_ptr);

    /* If successful, total_w_len is the amount written. Otherwise it is a
     * NetCDF error code (negative value).
     */
    return total_w_len;
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

/*----< GIOI_UFS_write_coll() >----------------------------------------------*/
MPI_Offset
GIOI_UFS_write_coll(GIO_File    fh,
                    const void *buf)
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
    GIOI_Access *my_req;

    /* others_req contains access structures of all processes whose requests
     * fall into this aggregator's file domain. It is only relevant of this
     * rank is an I/O aggregator.
     */
    GIOI_Access *others_req;

    int i, nprocs, rank, interleave_count=0;
    MPI_Offset *buf_idx = NULL;
    MPI_Offset *count_per_aggr, my_req_naggr;
    MPI_Offset *fd_end=NULL, min_st_off=0, max_end_off=LLONG_MAX;
    MPI_Offset fd_size, w_len=0;

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

    /* only check for interleaving if hint cb_write isn't disabled */
    if (fh->hints->cb_write != GIOI_HINT_DISABLE) {
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
        if (fh->fview.size == 0) /* -1 to indicate zero-sized request */
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
            if (st_end_all[i] == -1) /* zero-sized request */
                continue;
            if (st_end_all[i] <  st_end_all[i-1] &&
                st_end_all[i] <= st_end_all[i+1])
                interleave_count++;
            min_st_off  = MIN(min_st_off,  st_end_all[i]);
            max_end_off = MAX(max_end_off, st_end_all[i+1]);
        }
        GIOI_Free(st_end_all);

        /* Check if this collective write is entirely zero-sized. */
        if (min_st_off == -1 && max_end_off == -1) {
#if GIO_DEBUG_MODE == 1
            /* Warn a zero-sized collective write */
            if (rank == 0)
                printf("%s at %d: zero--sized collective write!\n",
                       __func__,__LINE__);
#endif
            return 0;
        }
    }

    if (fh->hints->cb_write == GIOI_HINT_DISABLE ||
        (!interleave_count && fh->hints->cb_write == GIOI_HINT_AUTO)) {
        /* switch to perform independent write */

        if (fh->fview.npairs == 0) /* zero-sized request */
            return 0;

        if (!fh->is_open) {
            /* If file has not yet been opened (this only happens to non-I/O
             * aggregators), then open it now and obtain hint striping_unit.
             */
            int err = GIOI_UFS_open_on_demand(fh);
            if (err != GIO_NOERR)
                return err;
        }

// if (rank == 0) printf("%s %d: SWITCH to GIOI_UFS_write_indep !!!\n",__func__,__LINE__);
        return GIOI_UFS_write_indep(fh, buf);
    }

    /* We now proceed to perform two-phase I/O.
     *
     * At first, a call to GIOI_Calc_file_domains() to calculate the file
     * domains assigned to each I/O aggregator. fh->hints->cb_nodes is the
     * number of aggregators. The aggregate access region of this collective
     * write call is divided among all aggregators into a set of disjoined file
     * domains. A file domain (denoted as 'fd') is the set of file regions an
     * aggregator is responsible for their file access. Thus, a file domain is
     * only relevant to I/O aggregators. All processes must send their requests
     * to an aggregator for the portions that fall into the aggregator's file
     * domain. Non-aggregators are not assigned a file domain. fh->is_agg
     * tells whether this rank is an aggregator.
     *
     * GIOI_Calc_file_domains() set the following 2 variables:
     *   fd_end[cb_nodes] - end location of file domains, inclusive offsets.
     *      The values are indexed by an aggregator number; they needs to be
     *      mapped to actual rank IDs in the communicator later.
     *   fd_size - average size (ceiling) of file domain among cb_nodes.
     */
    GIOI_Calc_file_domains(fh->hints->cb_nodes, fh->hints->striping_unit,
                           min_st_off, max_end_off, &fd_end, &fd_size);

    /* GIOI_Calc_my_req() calculates the portions of this rank's requests that
     * fall into every aggregator's file domains. It sets the following
     * variables:
     *   my_req_naggr - number of aggregators for which this rank has a portion
     *      of its request falling into their file domains.
     *   count_per_aggr[nprocs] - number of contiguous offset-length pairs in
     *      this rank's request that fall into each aggregator's file domain.
     *   my_req[nprocs] - metadata describes this rank's requests to be carried
     *      out by each I/O aggregator.
     *   buf_idx[nprocs] - indices into the user buffer that can be directly
     *      used to perform file read/write. Note this is only relevant when
     *      the user buffer is contiguous.
     */
    count_per_aggr = GIOI_Calloc(nprocs, sizeof(MPI_Offset));

    GIOI_Calc_my_req(fh, min_st_off, fd_end, fd_size, &my_req_naggr,
                     count_per_aggr, &my_req, &buf_idx);

    /* GIOI_Calc_others_req() produces results that are only relevant to the I/O
     * aggregators. Based on every rank's my_req, it calculates what portions
     * of requests from all processes that fall into this aggregator's file
     * domain. Note MPI communication is performed inside this subroutine. It
     * sets the following variable:
     *   others_req[nprocs] - metadata describing all processes' requests to be
     *      carried out by this aggregator.
     */
    GIOI_Calc_others_req(fh, my_req_naggr, count_per_aggr, my_req, &others_req);

    GIOI_Free(count_per_aggr);

#if GIO_PROFILING_MODE == 1
    if (fh->is_agg) gio_wr_time[1] += MPI_Wtime() - curT;
#endif

    /* exchange data and write in sizes of no more than cb_buffer_size. */
    w_len = Exch_and_write(fh, buf, min_st_off, fd_size, fd_end, others_req,
                           buf_idx);

    /* If this collective write is followed by an independent write, it is
     * possible to have those subsequent writes on other processes race ahead
     * and sneak in before the read-modify-write completes. Below, we make a
     * collective communication call before this subroutine returns, so no
     * process can start an independent I/O before this collective I/O
     * completes.
     */
    if (fh->hints->cb_nodes == 1)
        /* If there is only one aggregator, we can perform a less-expensive
         * Bcast(). All PnetCDF error codes are negative.
         */
        MPI_Bcast(&w_len, 1, MPI_OFFSET, fh->hints->aggr_ranks[0], fh->comm);
    else
        MPI_Allreduce(MPI_IN_PLACE, &w_len, 1, MPI_OFFSET, MPI_MIN, fh->comm);

    /* free all memory allocated for collective I/O */
    if (fd_end != NULL) GIOI_Free(fd_end);
    GIOI_Free(my_req[0].off);
    GIOI_Free(my_req);
    GIOI_Free(buf_idx);
    GIOI_Free(others_req[0].off);
    GIOI_Free(others_req);

#if GIO_PROFILING_MODE == 1
    if (fh->is_agg) gio_wr_time[0] += MPI_Wtime() - curT;
#endif

    /* w_len may not be the same as bview.size, because data sieving may
     * write more than requested.
     */
    return fh->bview.size;
}

