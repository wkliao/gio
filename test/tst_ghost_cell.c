/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Copyright (C) 2026, Northwestern University
 * See COPYRIGHT notice in top-level directory.
 *
 * This program uses a 2D checkerboad partitioning pattern on file and a buffer
 * view with ghost cells on both ends of each dimension, to test collective and
 * independent write and read APIs.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h> /* O_CREAT, O_RDWR */
#include <sys/types.h>
#include <errno.h>

#include <sys/types.h> /* open() */
#include <sys/stat.h>  /* open() */
#include <fcntl.h>     /* open() */

#include <mpi.h>
#include <gio.h>
#include <tst_utils.h>

#define LEN 100
#define NGHOSTS 2

/*----< tst_ghost_cell() >--------------------------------------------------*/
int tst_ghost_cell(const char *out_path,
                   int         coll_io,
                   MPI_Info    info)
{
    int i, j, k, rank, nprocs, err, nerrs=0, verbose, omode, esize;
    int fd, dump_rank, len, *buf, *file_buf, nghosts, psizes[2];
    int f_sizes[2], f_subsizes[2], f_starts[2];
    int b_sizes[2], b_subsizes[2], b_starts[2];
    MPI_Offset wlen, file_npairs, *file_offs, *file_lens;
    MPI_Offset rlen, buf_npairs, *buf_offs, *buf_lens;
    GIO_File fh;

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* element data type size (in this example the data type is int) */
    esize = sizeof(int);

    verbose     = 0;
    dump_rank   = 0;
    len         = LEN;     /* default dimension size */
    nghosts     = NGHOSTS; /* number of ghost cells on both sizes of each dim */

    if (verbose && rank == dump_rank) {
        printf("Number of MPI processes:  %d\n",nprocs);
        printf("Each subarray is of size: %d x %d (int) = %d\n",
               len, len, esize*len*len);
    }

    /* calculate number of processes along each dimension */
    psizes[0] = psizes[1] = 0;
    err = MPI_Dims_create(nprocs, 2, psizes); CHECK_ERR(err)

    if (verbose && rank == dump_rank)
        printf("%d: 2D rank IDs: %d, %d\n",rank,rank/psizes[1], rank%psizes[1]);

    /* set the global array sizes */
    f_sizes[0]    = len * psizes[0];
    f_sizes[1]    = len * psizes[1];
    f_subsizes[0] = len;
    f_subsizes[1] = len;
    f_starts[0]   = len * (rank / psizes[1]);
    f_starts[1]   = len * (rank % psizes[1]);

    if (verbose && rank == dump_rank) {
        printf("%d: f_sizes=%d %d f_subsizes=%d %d f_starts=%d %d\n",rank,
               f_sizes[0],f_sizes[1], f_subsizes[0],f_subsizes[1],
               f_starts[0],f_starts[1]);
    }

    /* construct a checkerboard partitioning file's layout and flatten it into
     * a list of offset-length pairs.
     */
    err = GIO_flatten_subarray(2, esize, 0, f_sizes, f_subsizes, f_starts,
                               &file_npairs, &file_offs, &file_lens);
    CHECK_ERR(err)

    if (verbose && rank == dump_rank) {
        for (i=0; i<file_npairs; i++)
            printf("%d: file[%d]  off %3lld len %3lld\n",
                   rank,i,file_offs[i],file_lens[i]);
    }

    /* set the local array sizes */
    b_subsizes[0] = len;
    b_subsizes[1] = len;
    b_sizes[0]    = b_subsizes[0] * nghosts * 2;
    b_sizes[1]    = b_subsizes[1] * nghosts * 2;
    b_starts[0]   = nghosts;
    b_starts[1]   = nghosts;

    /* construct a local buffer memory layout with ghost cells at the both ends
     * of each dimenion, and flatten it into a list of offset-length pairs.
     */
    err = GIO_flatten_subarray(2, esize, 0, b_sizes, b_subsizes, b_starts,
                               &buf_npairs, &buf_offs, &buf_lens);
    CHECK_ERR(err)

    if (verbose && rank == dump_rank) {
        for (i=0; i<buf_npairs; i++)
            printf("%d:  buf[%d]  off %3lld len %3lld\n",
                   rank,i,buf_offs[i],buf_lens[i]);
    }

    /* allocate and initialize I/O buffer */
    k = 0;
    buf = (int*) malloc(esize * b_sizes[0] * b_sizes[1]);
    for (i=0; i<b_sizes[0]; i++) {
        for (j=0; j<b_sizes[1]; j++) {
            int ij = i*b_sizes[1] + j;
            if (nghosts <= i && i < b_subsizes[0]+nghosts &&
                nghosts <= j && j < b_subsizes[1]+nghosts)
                buf[ij] = rank*100 + (k++);
            else
                /* set all ghost cells value to -1 */
                buf[ij] = -1;
        }
    }

    if (verbose && rank == dump_rank) {
        printf("\nDump the contents of write buffer:\n");
        for (i=0; i<b_sizes[0]; i++) {
            printf("buf[%2d][] = ",i);
            for (j=0; j<b_sizes[1]; j++) {
                int ij = i*b_sizes[1] + j;
                printf(" %3d", buf[ij]);
            }
            printf("\n");
        }
        printf("\n\n");
    }

    /* open file and truncate it to zero sized */
    omode = O_CREAT | O_RDWR;
    err = GIO_open(MPI_COMM_WORLD, out_path, omode, info, &fh); CHECK_ERR(err)
    err = GIO_set_size(fh, 0); CHECK_ERR(err)

    /* write to the file */
    wlen = GIO_write_all(fh, buf, file_npairs, file_offs, file_lens,
                                  buf_npairs, buf_offs, buf_lens);
    if (wlen < 0) CHECK_ERR((int)wlen)

    err = GIO_close(&fh); CHECK_ERR(err)

    if (verbose && rank == dump_rank) {
        char *filename;
        filename = remove_file_system_type_prefix(out_path);

        printf("\n\nAfter GIO_write_all, read the whole file back:\n");
        fd = open(filename, O_RDONLY, 0400);
        if (fd < 0) {
            fprintf(stderr,"Error at file open %s (%s)", filename, strerror(errno));
            goto err_out;
        }
        file_buf = (int*) malloc(esize * f_sizes[0] * f_sizes[1]);
        off_t rlen = pread(fd, file_buf, f_sizes[0] * f_sizes[1] * esize, 0);
        assert(rlen >= 0);
        close(fd);
        for (i=0; i<f_sizes[0]; i++) {
            printf("file_buf[%2d][] = ",i);
            for (j=0; j<f_sizes[1]; j++) {
                int ij = i*f_sizes[1] + j;
                printf(" %3d", file_buf[ij]);
            }
            printf("\n");
        }
        printf("\n\n");
        free(file_buf);
    }

    /* check file size */
    if (rank == 0) {
        char *filename;
        filename = remove_file_system_type_prefix(out_path);

        fd = open(filename, O_RDONLY, 0400);
        if (fd < 0) {
            fprintf(stderr,"Error at file open %s (%s)", filename, strerror(errno));
            goto err_out;
        }
        off_t fsize = lseek(fd, 0, SEEK_END);
        off_t expected = esize * f_sizes[0] * f_sizes[1];
        if (fsize != expected) {
            fprintf(stderr,"Error: expecting file size %lld, but got %lld\n",
                    (long long)expected, (long long)fsize);
            err = 1;
        }
        close(fd);
    }
    MPI_Bcast(&err, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (err > 0) nerrs++;

    err = GIO_open(MPI_COMM_WORLD, out_path, O_RDONLY, info, &fh);
    CHECK_ERR(err)

    /* set contents of read buffer to all -1 */
    for (i=0; i<b_sizes[0] * b_sizes[1]; i++) buf[i] = -1;

    /* read from the file */
    rlen = GIO_read_all(fh, buf, file_npairs, file_offs, file_lens,
                                 buf_npairs, buf_offs, buf_lens);
    if (rlen < 0) CHECK_ERR((int)rlen)

    err = GIO_close(&fh); CHECK_ERR(err)

    if (verbose && rank == dump_rank) {
        printf("\n\nAfter GIO_read_all, dump the whole buf:\n");
        for (i=0; i<b_sizes[0]; i++) {
            printf("buf[%2d][] = ",i);
            for (j=0; j<b_sizes[1]; j++) {
                int ij = i*b_sizes[1] + j;
                printf(" %3d", buf[ij]);
            }
            printf("\n");
        }
        printf("\n\n");
    }

    /* check read data */
    k = 0;
    for (i=0; i<b_sizes[0]; i++) {
        for (j=0; j<b_sizes[1]; j++) {
            int ij = i*b_sizes[1] + j;
            if (nghosts <= i && i < b_subsizes[0]+nghosts &&
                nghosts <= j && j < b_subsizes[1]+nghosts) {
                int exp = rank*100 + (k++);
                if (buf[ij] != exp) {
                    fprintf(stderr,"Error: expect buf[%d][%d] = %d but got %d\n",
                            i, j, exp, buf[ij]);
                    nerrs++;
                    goto loop_out;
                }
            }
            else if (buf[ij] != -1) {
                fprintf(stderr,"Error: expect ghost cell buf[%d][%d] = -1 but got %d\n",
                       i, j, buf[ij]);
                nerrs++;
                goto loop_out;
            }
        }
    }

    if (verbose && rank == dump_rank) {
        char *filename;
        filename = remove_file_system_type_prefix(out_path);

        printf("\n\nAfter GIO_read_all, dump the whole file:\n");
        fd = open(filename, O_RDONLY, 0400);
        if (fd < 0) {
            fprintf(stderr,"Error at file open %s (%s)", filename, strerror(errno));
            goto err_out;
        }
        file_buf = (int*) malloc(esize * f_sizes[0] * f_sizes[1]);
        off_t rlen = pread(fd, file_buf, f_sizes[0] * f_sizes[1] * esize, 0);
        assert(rlen >= 0);
        close(fd);
        for (i=0; i<f_sizes[0]; i++) {
            printf("file_buf[%2d][] = ",i);
            for (j=0; j<f_sizes[1]; j++) {
                int ij = i*f_sizes[1] + j;
                printf(" %3d", file_buf[ij]);
            }
            printf("\n");
        }
        printf("\n\n");
        free(file_buf);
    }

loop_out:
    free(file_offs);
    free(file_lens);
    free(buf_offs);
    free(buf_lens);
    free(buf);

err_out:
    return nerrs;
}

static
int test_io(const char *out_path,
            const char *in_path, /* ignored */
            int         coll_io,
            MPI_Info    info)
{
    int nerrs;

    nerrs = tst_ghost_cell(out_path, coll_io, info);

    return nerrs;
}

int main(int argc, char **argv)
{
    int err;
    loop_opts opt;

    MPI_Init(&argc, &argv);

    opt.ds   = 2;     /* auto and disable data sieving modes */
    opt.mod  = 2;     /* collective and independent APIs */
    opt.diff = true;  /* run diff on output files */

    err = tst_main(argc, argv, "ghost cells in user buffer", opt, test_io);

    MPI_Finalize();

    return err;
}

