/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>     /* strerror() */
#include <fcntl.h>      /* open() */
#include <sys/types.h>  /* open(), umask() */
#include <sys/stat.h>   /* umask() */

#include <assert.h>
#include <sys/errno.h>
#include <unistd.h>     /* pread() */


#include "gioi.h"

/*----< GIO_UFS_read_contig() >--------------------------------------------*/
GIO_Count
GIO_UFS_read_contig(GIO_File    fh,
                      void         *buf,
                      GIO_Count   r_size,
                      GIO_Count  offset)
{
    char *p;
    ssize_t err = 0;
    size_t r_count;
    GIO_Count bytes_xfered = 0;

    if (r_size == 0) return GIO_NOERR;

#ifdef GIO_DEBUG
    assert(fh->is_open == 1);
    assert(fh->fd_sys >= 0);
#endif

#if defined(GIO_PROFILING) && (GIO_PROFILING == 1)
    double timing = MPI_Wtime();
#endif
    p = (char *) buf;
    while (bytes_xfered < r_size) {
        r_count = r_size - bytes_xfered;
        err = pread(fh->fd_sys, p, r_count, offset + bytes_xfered);
        if (err == -1)
            goto err_out;
        if (err == 0)
            break;
        bytes_xfered += err;
        p += err;
    }
#if defined(GIO_PROFILING) && (GIO_PROFILING == 1)
    gio_rd_time[2] += MPI_Wtime() - timing;
#endif

err_out:
    if (err == -1)
        bytes_xfered = GIOI_error_posix("pread");

    return bytes_xfered;
}

/*----< GIO_UFS_read_indep() >---------------------------------------------*/
/* This subroutine implements independent read. It consists of two major code
 * segments. The first one is for when data sieving is disabled and the second
 * one enabled.
 *
 * Note in GIO, the file view and buffer view are never used for more than
 * one round, which greatly simplifies the implementation of this subroutine.
 */
GIO_Count
GIO_UFS_read_indep(GIO_File  fh,
                     void       *buf)
{
    char *ptr, *cpy_ptr, *tmp_buf=NULL;
    GIO_Count i, j, k, ntimes;
    GIO_Count lock_off, file_off;
    GIO_Count lock_len, len, total_len=0, tmp_buf_size, file_rem, buf_rem;

#ifdef GIO_DEBUG
    /* I/O hints should have been set already, even if this rank is not an INA
     * aggregator.
     */
    assert(fh->hints->ind_rd_buffer_size > 0);

    /* zero-sized requests should have returned in GIO_read_indep() */
    assert(fh->bview.npairs > 0);
    assert(fh->bview.size > 0);
#endif

    if (!fh->is_open) {
        /* Open the file to obtain fh->fd_sys if this process has not opened
         * the file yet. This happens to the processes that are neither I/O
         * aggregators nor INA aggregators.
         */
        int perm, old_mask = umask(022);
        umask(old_mask);
        perm = old_mask ^ GIO_PERM;

        fh->fd_sys = open(fh->filename, fh->amode, perm);
        if (fh->fd_sys == -1) {
            int rank;
            MPI_Comm_rank(fh->comm, &rank);
            fprintf(stderr, "%s line %d: rank %d failed to open file %s (%s)\n",
                    __func__,__LINE__, rank, fh->filename, strerror(errno));
            return GIOI_error_posix("open");
        }
        fh->is_open = 1;
    }

    if (fh->fview.npairs <= 1 && fh->bview.npairs <= 1) {
        /* Both buffer and fileview are contiguous. */
        return GIO_UFS_read_contig(fh, buf, fh->bview.size,
                                     fh->fview.off[0]);
    }

    lock_off = fh->fview.off[0];
    if (fh->fview.npairs > 1)
        lock_len = fh->fview.off[fh->fview.npairs-1]
                 + fh->fview.len[fh->fview.npairs-1]
                 - lock_off;
    else
        lock_len = fh->fview.size;

    /* if atomicity is true, lock (exclusive) the whole region */
    if (fh->atomicity && fh->amode != O_RDONLY)
        GIO_WRITE_LOCK(fh, lock_off, SEEK_SET, lock_len);

    /* When data sieving read is disabled or fview is contiguous, read is
     * carried out in multiple rounds. In each round, file data is first read
     * into a contiguous buffer, tmp_buf, and then copied it to the user buffer
     * at the end of each round. This can improve performance when the read
     * buffer is non-contiguous and fview is contiguous, i.e. by reducing
     * the number of file reads.
     */
    if (fh->hints->ds_read == GIO_HINT_DISABLE ||
        fh->fview.npairs <= 1) {

        if (fh->bview.npairs <= 1) { /* directly read to buf */
            tmp_buf = (char*)buf;
            buf_rem = fh->bview.size;
            ntimes = 1;
            tmp_buf_size = fh->bview.size;
        }
        else { /* buf is noncontiguous */
            tmp_buf_size = MIN(fh->bview.size, fh->hints->ind_rd_buffer_size);
            tmp_buf = (char*) GIOI_Malloc(tmp_buf_size);
            buf_rem = fh->bview.len[0];
            ntimes = fh->bview.size / tmp_buf_size;
            if (fh->bview.size % tmp_buf_size)
                ntimes++;
        }

        file_off = fh->fview.off[0];
        file_rem = fh->fview.len[0];

        /* pointer to buf, starting location to copy from tmp_buf */
        cpy_ptr = (char*)buf;

        k = (fh->bview.npairs <= 1) ? 1 : 0; /* whether to skip while loop k */
        j = 0;
        for (i=0; i<ntimes; i++) { /* perform read in ntimes rounds */
            GIO_Count req_len, tmp_buf_rem;

            /* using tmp_buf to read from the file */
            tmp_buf_rem = tmp_buf_size;
            ptr = tmp_buf;
            while (j < fh->fview.npairs) {
                req_len = MIN(tmp_buf_rem, file_rem);
                /* read from offset file_off of length req_len */
                len = GIO_UFS_read_contig(fh, ptr, req_len, file_off);
                if (len < 0) return len;
                total_len += len;

                ptr += req_len;
                tmp_buf_rem -= req_len;
                if (tmp_buf_rem == 0) break;

                if (file_rem == req_len) { /* done with pair j */
                    j++;
                    file_off = fh->fview.off[j];
                    file_rem = fh->fview.len[j];
                }
                else { /* there is still data remained in pair j */
                    file_off += req_len;
                    file_rem -= req_len;
                }
            }

            /* copy data from tmp_buf to buf */
            tmp_buf_rem = tmp_buf_size;
            ptr = tmp_buf;
            while (k < fh->bview.npairs) {
                req_len = MIN(tmp_buf_rem, buf_rem);
                memcpy(cpy_ptr, ptr, req_len);

                ptr += req_len;
                tmp_buf_rem -= req_len;
                if (tmp_buf_rem == 0) break;

                if (buf_rem == req_len) { /* done with pair k */
                    k++;
                    cpy_ptr = (char*)buf + fh->bview.off[k];
                    buf_rem = fh->bview.len[k];
                }
                else { /* there is still data remained in pair k */
                    cpy_ptr += req_len;
                    buf_rem -= req_len;
                }
            }
        }

        /* free tmp_buf if allocated */
        if (tmp_buf != buf) GIOI_Free(tmp_buf);
    }
    else {
        /* fview is noncontiguous and data sieving is not disabled */
        GIO_Count disp, first_stripe, last_stripe;
        GIO_Count lock_rem, cpy_len;

        /* allocate read-copy buffer */
        tmp_buf_size = MIN(lock_len, fh->hints->striping_unit);
        tmp_buf = (char*) GIOI_Malloc(tmp_buf_size);

        /* lock_rem is the amount remained to be locked for the entire
         * read-copy region.
         */
        lock_rem = lock_len;

        /* perform ntimes rounds of read-copy */
        first_stripe = lock_off / fh->hints->striping_unit;
        last_stripe = (lock_off + lock_len - 1) / fh->hints->striping_unit;
        ntimes = (last_stripe - first_stripe) + 1;

#ifdef GIO_DEBUG
        /* fview's offsets should have already sorted into a monotonically
         * non-decreasing order without overlaps. In addition, earlier checks
         * have ensured all fh->fview.len[] > 0.
         */
        assert(fh->fview.len[0] > 0);
        for (i=1; i<fh->fview.npairs; i++) {
            assert(fh->fview.off[i-1] < fh->fview.off[i]);
            assert(fh->fview.off[i-1] + fh->fview.len[i-1] <
                   fh->fview.off[i]);
            assert(fh->fview.len[i] > 0);
        }
#endif

        /* initialize loop local variables with the 1st pair of fview and
         * bview
         */
        file_off = fh->fview.off[0];
        file_rem = fh->fview.len[0];
        buf_rem  = fh->bview.len[0];

        /* pointer to buf, starting location to copy from tmp_buf */
        cpy_ptr = (char*)buf;

        disp = 0;
        k = 0; /* index pointed to bview's  offset-length pairs */
        j = 0; /* index pointed to fview's offset-length pairs */
        for (i=0; i<ntimes; i++) { /* perform read in ntimes rounds */
            GIO_Count req_len, tmp_buf_rem, gap;

            /* adjust tmp_buf_size to achieve striping_unit aligned file
             * access
             */
            tmp_buf_size = fh->hints->striping_unit
                         - (file_off % fh->hints->striping_unit);

            if (disp >= tmp_buf_size) {
                /* This displacement at the beginning of read-copy region of
                 * this round i is too large, containing no data needed to
                 * be copied to the user read buffer. This allows to skip a few
                 * rounds.
                 */
                GIO_Count skip = disp / tmp_buf_size;
                i        += skip - 1;
                disp     -= skip * tmp_buf_size;
                lock_rem -= skip * tmp_buf_size;
                file_off += skip * tmp_buf_size;
                continue;
            }

            /* read a chunk from the file into tmp_buf */
            req_len = MIN(tmp_buf_size, lock_rem);

            if (!fh->atomicity && fh->amode != O_RDONLY) /* lock the read-copy region */
                GIO_WRITE_LOCK(fh, file_off, SEEK_SET, req_len);

            len = GIO_UFS_read_contig(fh, tmp_buf, req_len, file_off);
            if (len < 0) return len;

            /* Copy data from tmp_buf to buf. Skip 'disp' bytes at the front
             * for both buffers.
             */
            tmp_buf_rem = MIN(tmp_buf_size, lock_rem) - disp;
            ptr = tmp_buf + disp;

            /* disp will be reset at the end of each round */
            disp = 0;

            while (tmp_buf_rem > 0) {
                /* cpy_len is the amount to be copied over. It should be no
                 * more than remaining of temporary buffer size of this round
                 * i, remaining of the fview offset-length pair j, and
                 * remaining of bview's offset-length pair k.
                 */
                cpy_len = MIN(file_rem, buf_rem);
                cpy_len = MIN(cpy_len, tmp_buf_rem);
                memcpy(cpy_ptr, ptr, cpy_len);
                total_len += cpy_len;

                /* Deduct remaining of temp buffer. Note even if tmp_buf_rem ==
                 * 0, we still need continue to calculate disp for the next
                 * round. */
                tmp_buf_rem -= cpy_len;

                /* advance buffer pointer */
                ptr += cpy_len;

                if (buf_rem == cpy_len) { /* done with pair k */
                    k++;
                    if (k == fh->bview.npairs) /* all data have been copied */
                        break;
                    cpy_ptr = (char*)buf + fh->bview.off[k];
                    buf_rem = fh->bview.len[k];
                }
                else { /* there is still data remained in pair k */
                    cpy_ptr += cpy_len;
                    buf_rem -= cpy_len;
                }

                if (file_rem == cpy_len) { /* done with pair j */
                    /* When j is the last pair of this round i, the end offset
                     * of tmp_buf may fall between the end of pair j and the
                     * beginning of pair j+1. In this case, the beginning of
                     * file offset region for the next round should advance
                     * disp bytes.
                     */

                    j++;
                    assert(j < fh->fview.npairs);
                    /* Note j should never become fview.npairs, as the
                     * above check of if (k == bview.npairs) should
                     * short-circuit the while loop. This is because GIO
                     * requires fview.size == bview.size.
                     */

                    file_rem = fh->fview.len[j];

                    /* calculate the empty size between pairs j-1 and j */
                    gap = fh->fview.off[j]
                        - (fh->fview.off[j-1] + fh->fview.len[j-1]);

                    if (tmp_buf_rem <= gap) {
                        /* j-1 is last pair of this round */
                        disp = gap - tmp_buf_rem;
                        break;
                    }
                    tmp_buf_rem -= gap;
                    ptr += gap;
                }
                else { /* there is still data remained in pair j */
                    file_rem -= cpy_len;
                }
            }

            if (!fh->atomicity && fh->amode != O_RDONLY) /* unlock the read-copy region */
                GIO_UNLOCK(fh, file_off, SEEK_SET, req_len);

            /* reduce remaining size to be locked */
            lock_rem -= req_len;

            /* update file offset for the next round of read-copy */
            file_off += req_len;
        }

        /* free tmp_buf if allocated */
        if (tmp_buf != NULL) GIOI_Free(tmp_buf);
    }

    /* if atomicity is true, unlock (exclusive) the whole region */
    if (fh->atomicity && fh->amode != O_RDONLY)
        GIO_UNLOCK(fh, lock_off, SEEK_SET, lock_len);

#ifdef GIO_DEBUG
    assert(total_len >= fh->bview.size);
#endif

    return total_len;
}

