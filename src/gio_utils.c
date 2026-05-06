/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h> /* strchr() */

#include "gioi.h"

/* For systems that do not have pread() */
#if HAVE_DECL_PREAD == 0

#include <sys/types.h>
#include <unistd.h>

/*----< GIOI_pread() >-------------------------------------------------------*/
ssize_t GIOI_pread(int fd, void *buf, size_t count, off_t offset)
{
    off_t lseek_ret;
    off_t old_offset;
    ssize_t read_ret;

    old_offset = lseek(fd, 0, SEEK_CUR);
    lseek_ret = lseek(fd, offset, SEEK_SET);
    if (lseek_ret == -1)
        return lseek_ret;
    read_ret = read(fd, buf, count);
    if (read_ret < 0)
        return read_ret;
    /* man page says "file offset is not changed" */
    if ((lseek_ret = lseek(fd, old_offset, SEEK_SET)) < 0)
        return lseek_ret;

    return read_ret;
}
#endif

/* For systems that do not have pwrite() */
#if HAVE_DECL_PWRITE == 0

#include <sys/types.h>
#include <unistd.h>

/*----< GIOI_pwrite() >------------------------------------------------------*/
ssize_t GIOI_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    off_t lseek_ret;
    off_t old_offset;
    ssize_t write_ret;

    old_offset = lseek(fd, 0, SEEK_CUR);
    lseek_ret = lseek(fd, offset, SEEK_SET);
    if (lseek_ret == -1)
        return lseek_ret;
    write_ret = write(fd, buf, count);
    if (write_ret < 0)
        return write_ret;
    /* man page says "file offset is not changed" */
    if ((lseek_ret = lseek(fd, old_offset, SEEK_SET)) < 0)
        return lseek_ret;

    return write_ret;
}
#endif

/*----< GIOI_Heap_merge() >--------------------------------------------------*/
void GIOI_Heap_merge(GIOI_Access      *others_req,
                     const MPI_Offset *count,
                     MPI_Offset       *srt_off,
                     MPI_Offset       *srt_len,
                     const MPI_Offset *start_pos,
                     int               nprocs,
                     int               nprocs_recv,
                     MPI_Offset        total_elements)
{
    typedef struct {
        MPI_Offset *off_list;
        MPI_Offset *len_list;
        MPI_Offset  nelem;
    } heap_struct;

    heap_struct *a, tmp;
    int i, j, heapsize, l, r, k, smallest;

    a = (heap_struct*) GIOI_Malloc(sizeof(heap_struct) * (nprocs_recv + 1));

    j = 0;
    for (i = 0; i < nprocs; i++)
        if (count[i]) {
            a[j].off_list = &(others_req[i].offsets[start_pos[i]]);
            a[j].len_list = &(others_req[i].lens[start_pos[i]]);
            a[j].nelem = count[i];
            j++;
        }

    /* build a heap out of the first element from each list, with
     * the smallest element of the heap at the root */

    heapsize = nprocs_recv;
    for (i = heapsize / 2 - 1; i >= 0; i--) {
        /* Heapify(a, i, heapsize); Algorithm from Cormen et al. pg. 143
         * modified for a heap with smallest element at root. I have
         * removed the recursion so that there are no function calls.
         * Function calls are too expensive. */
        k = i;
        for (;;) {
            l = 2 * (k + 1) - 1;
            r = 2 * (k + 1);

            if ((l < heapsize) && (*(a[l].off_list) < *(a[k].off_list)))
                smallest = l;
            else
                smallest = k;

            if ((r < heapsize) && (*(a[r].off_list) < *(a[smallest].off_list)))
                smallest = r;

            if (smallest != k) {
                tmp.off_list = a[k].off_list;
                tmp.len_list = a[k].len_list;
                tmp.nelem = a[k].nelem;

                a[k].off_list = a[smallest].off_list;
                a[k].len_list = a[smallest].len_list;
                a[k].nelem = a[smallest].nelem;

                a[smallest].off_list = tmp.off_list;
                a[smallest].len_list = tmp.len_list;
                a[smallest].nelem = tmp.nelem;

                k = smallest;
            } else
                break;
        }
    }

    for (i = 0; i < total_elements; i++) {
        /* extract smallest element from heap, i.e. the root */
        srt_off[i] = *(a[0].off_list);
        srt_len[i] = *(a[0].len_list);
        (a[0].nelem)--;

        if (!a[0].nelem) {
            a[0].off_list = a[heapsize - 1].off_list;
            a[0].len_list = a[heapsize - 1].len_list;
            a[0].nelem = a[heapsize - 1].nelem;
            heapsize--;
        } else {
            (a[0].off_list)++;
            (a[0].len_list)++;
        }

        /* Heapify(a, 0, heapsize); */
        k = 0;
        for (;;) {
            l = 2 * (k + 1) - 1;
            r = 2 * (k + 1);

            if ((l < heapsize) && (*(a[l].off_list) < *(a[k].off_list)))
                smallest = l;
            else
                smallest = k;

            if ((r < heapsize) && (*(a[r].off_list) < *(a[smallest].off_list)))
                smallest = r;

            if (smallest != k) {
                tmp.off_list = a[k].off_list;
                tmp.len_list = a[k].len_list;
                tmp.nelem = a[k].nelem;

                a[k].off_list = a[smallest].off_list;
                a[k].len_list = a[smallest].len_list;
                a[k].nelem = a[smallest].nelem;

                a[smallest].off_list = tmp.off_list;
                a[smallest].len_list = tmp.len_list;
                a[smallest].nelem = tmp.nelem;

                k = smallest;
            } else
                break;
        }
    }
    GIOI_Free(a);
}

/*----< GIOI_remove_file_system_type_prefix() >------------------------------*/
/* File system types recognized by ROMIO in MPICH 4.0.0, and by GIO */
static const char* fstypes[] = {"ufs", "nfs", "xfs", "pvfs2", "gpfs", "panfs", "lustre", "daos", "testfs", "ime", "quobyte", NULL};

/* Return a pointer to filename by removing the file system type prefix name if
 * there is any.  For example, when filename = "lustre:/home/foo/testfile.nc",
 * remove "lustre:" to return a pointer to "/home/foo/testfile.nc", so the name
 * can be used in POSIX open() calls.
 */
char* GIOI_remove_file_system_type_prefix(const char *filename)
{
    char *ret_filename = (char*)filename;

    if (filename == NULL) return NULL;

    if (strchr(filename, ':') != NULL) { /* there is a prefix end with ':' */
        /* check if prefix is one of recognized file system types */
        int i=0;
        while (fstypes[i] != NULL) {
            size_t prefix_len = strlen(fstypes[i]);
            if (!strncmp(filename, fstypes[i], prefix_len)) { /* found */
                ret_filename += prefix_len + 1;
                break;
            }
            i++;
        }
    }

    return ret_filename;
}

/*----< GIOI_sanity_check() >------------------------------------------------*/
int GIOI_sanity_check(const char       *func_name,
                      int               lineno,
                      GIO_File          fh,
                      MPI_Offset        file_npairs,
                      const MPI_Offset *file_offs,
                      const MPI_Offset *file_lens,
                      MPI_Offset        buf_npairs,
                      const MPI_Offset *buf_offs,
                      const MPI_Offset *buf_lens)
{
    MPI_Offset j;

    if (fh == NULL) {
        fprintf(stderr, "Error in %s at %d: NULL file handle\n",
                __func__, __LINE__);
        return GIO_EINVAL;
    }

    if (file_npairs > 0) {
        if (file_offs == NULL) {
            fprintf(stderr, "Error in %s at %d: NULL file offsets\n",
                    __func__, __LINE__);
            return GIO_EINVAL;
        }
        if (file_lens == NULL) {
            fprintf(stderr, "Error in %s at %d: NULL file lengths\n",
                    __func__, __LINE__);
            return GIO_EINVAL;
        }
    }
    if (buf_npairs > 0) {
        if (buf_offs == NULL) {
            fprintf(stderr, "Error in %s at %d: NULL buffer offsets\n",
                    __func__, __LINE__);
            return GIO_EINVAL;
        }
        if (buf_lens == NULL) {
            fprintf(stderr, "Error in %s at %d: NULL buff lengths\n",
                    __func__, __LINE__);
            return GIO_EINVAL;
        }
    }

    fh->fview.npairs = file_npairs;
    fh->fview.off    = file_offs;
    fh->fview.len    = file_lens;
    fh->fview.idx    = 0;

    fh->bview.npairs = buf_npairs;
    fh->bview.off    = buf_offs;
    fh->bview.len    = buf_lens;
    fh->bview.idx    = 0;

    /* calculate total request amount */
    fh->fview.size = (file_npairs > 0) ? file_lens[0] : 0;
    for (j=1; j<fh->fview.npairs; j++) {
        fh->fview.size += fh->fview.len[j];

        /* Check if file offsets are in a monotonically non-decreasing order. */
        if (fh->fview.off[j-1] > fh->fview.off[j])
            return GIO_EFILEVIEW;

        /* No overlap is allowed. */
        if (fh->fview.off[j-1] + fh->fview.len[j-1] >
            fh->fview.off[j])
            return GIO_EFILEVIEW;
    }

    fh->bview.size = 0;
    for (j=0; j<fh->bview.npairs; j++)
        fh->bview.size += fh->bview.len[j];

    fh->fview.rem = (file_npairs > 1) ? file_lens[0] : fh->fview.size;
    fh->bview.rem = (buf_npairs > 1) ? buf_lens[0] : fh->bview.size;

    /* Negative request amounts are not allowed */
    if (fh->fview.size < 0 || fh->bview.size < 0)
        return GIO_ENEGATIVECNT;

    /* Access amount must be the same between fview and bview */
    if (fh->fview.size != fh->bview.size) {
        fprintf(stderr,
                "Error in %s at %d: file and buffer view amounts mismatched\n",
                __func__, __LINE__);
        return GIO_EINVAL;
    }

    return GIO_NOERR;
}

