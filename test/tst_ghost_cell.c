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

/*----< main() >------------------------------------------------------------*/
int tst_ghost_cell(const char *out_path,
                   int         coll_io,
                   MPI_Info    info)
{
    extern int optind;
    extern char *optarg;
    size_t esize;
    int i, j, k, rank, nprocs, err, nerrs=0, verbose, omode;
    int fd, dump_rank, len, *buf, *file_buf, nghosts;
    int psizes[2], sizes[2], subsizes[2], starts[2], buf_len[2];
    MPI_Offset wlen, rlen, file_npairs, *file_offs, *file_lens;
    MPI_Offset buf_npairs, *buf_offs, *buf_lens;
    GIO_File fh;

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    verbose     = 0;
    dump_rank   = 0;
    len         = LEN;     /* default dimension size */
    nghosts     = NGHOSTS; /* number of ghost cells on both sizes of each dim */

    if (verbose && rank == dump_rank) {
        printf("Number of MPI processes:  %d\n",nprocs);
        printf("Each subarray is of size: %d x %d (int) = %zd\n",
               len, len, sizeof(int)*len*len);
    }

    /* calculate number of processes along each dimension */
    psizes[0] = psizes[1] = 0;
    err = MPI_Dims_create(nprocs, 2, psizes); CHECK_ERR(err)

    if (verbose && rank == dump_rank)
        printf("%d: 2D rank IDs: %d, %d\n",rank,rank/psizes[1], rank%psizes[1]);

    /* create a subarray datatype */
    sizes[0]    = len * psizes[0];
    sizes[1]    = len * psizes[1];
    subsizes[0] = len;
    subsizes[1] = len;
    starts[0]   = len * (rank / psizes[1]);
    starts[1]   = len * (rank % psizes[1]);

    if (verbose && rank == dump_rank) {
        file_buf = (int*) malloc(sizeof(int) * sizes[0] * sizes[1]);
        printf("%d: sizes=%d %d subsizes=%d %d starts=%d %d\n",rank,
               sizes[0],sizes[1], subsizes[0],subsizes[1], starts[0],starts[1]);
    }

    /* element data type size (in this example the data type is int) */
    esize = sizeof(int);

    file_npairs = subsizes[0];
    file_offs = (MPI_Offset*) malloc(sizeof(MPI_Offset) * file_npairs);
    file_lens = (MPI_Offset*) malloc(sizeof(MPI_Offset) * file_npairs);
    for (i=0; i<subsizes[0]; i++) {
        file_offs[i] = (starts[0] + i) * sizes[1] + starts[1];
        file_offs[i] *= esize;
        file_lens[i] = subsizes[1] * esize;
        if (verbose && rank == dump_rank)
            printf("%d: file[%d]  off %3lld len %3lld\n",rank,i,file_offs[i],file_lens[i]);
    }

    /* allocate and initialize I/O buffer */
    k = 0;
    buf_len[0] = subsizes[0] + nghosts * 2;
    buf_len[1] = subsizes[1] + nghosts * 2;
    buf = (int*) malloc(esize * buf_len[0] * buf_len[1]);
    for (i=0; i<buf_len[0]; i++) {
        for (j=0; j<buf_len[1]; j++) {
            int ij = i*buf_len[1] + j;
            if (nghosts <= i && i < subsizes[0]+nghosts &&
                nghosts <= j && j < subsizes[1]+nghosts)
                buf[ij] = rank*100 + (k++);
            else
                /* set all ghost cells value to -1 */
                buf[ij] = -1;
        }
    }

    if (verbose && rank == dump_rank) {
        printf("\nDump the contents of write buffer:\n");
        for (i=0; i<buf_len[0]; i++) {
            printf("buf[%2d][] = ",i);
            for (j=0; j<buf_len[1]; j++) {
                int ij = i*buf_len[1] + j;
                printf(" %3d", buf[ij]);
            }
            printf("\n");
        }
        printf("\n\n");
    }

    /* buffer has ghost cells on 4 sides of size nghosts each */
    buf_npairs = subsizes[0];
    buf_offs = (MPI_Offset*) malloc(sizeof(MPI_Offset) * buf_npairs);
    buf_lens = (MPI_Offset*) malloc(sizeof(MPI_Offset) * buf_npairs);
    for (i=0; i<subsizes[0]; i++) {
        buf_offs[i] = (nghosts + i) * buf_len[1] + nghosts;
        buf_offs[i] *= esize;
        buf_lens[i] = subsizes[1] * esize;
        if (verbose && rank == dump_rank)
            printf("%d:  buf[%d]  off %3lld len %3lld\n",rank,i,buf_offs[i],buf_lens[i]);
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
        pread(fd, file_buf, sizes[0] * sizes[1] * sizeof(int), 0);
        close(fd);
        for (i=0; i<sizes[0]; i++) {
            printf("file_buf[%2d][] = ",i);
            for (j=0; j<sizes[1]; j++) {
                int ij = i*sizes[1] + j;
                printf(" %3d", file_buf[ij]);
            }
            printf("\n");
        }
        printf("\n\n");
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
        off_t expected = esize * sizes[0] * sizes[1];
        if (fsize != expected) {
            fprintf(stderr,"Error: expecting file size %lld, but got %lld\n", expected, fsize);
            err = 1;
        }
        close(fd);
    }
    MPI_Bcast(&err, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if (err > 0) nerrs++;

    err = GIO_open(MPI_COMM_WORLD, out_path, O_RDONLY, info, &fh);
    CHECK_ERR(err)

    /* set contents of read buffer to all -1 */
    for (i=0; i<buf_len[0] * buf_len[1]; i++) buf[i] = -1;

    /* read from the file */
    rlen = GIO_read_all(fh, buf, file_npairs, file_offs, file_lens,
                                 buf_npairs, buf_offs, buf_lens);
    if (rlen < 0) CHECK_ERR((int)rlen)

    err = GIO_close(&fh); CHECK_ERR(err)

    if (verbose && rank == dump_rank) {
        printf("\n\nAfter GIO_read_all, dump the whole buf:\n");
        for (i=0; i<buf_len[0]; i++) {
            printf("buf[%2d][] = ",i);
            for (j=0; j<buf_len[1]; j++) {
                int ij = i*buf_len[1] + j;
                printf(" %3d", buf[ij]);
            }
            printf("\n");
        }
        printf("\n\n");
    }

    /* check read data */
    k = 0;
    for (i=0; i<buf_len[0]; i++) {
        for (j=0; j<buf_len[1]; j++) {
            int ij = i*buf_len[1] + j;
            if (nghosts <= i && i < subsizes[0]+nghosts &&
                nghosts <= j && j < subsizes[1]+nghosts) {
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
        pread(fd, file_buf, sizes[0] * sizes[1] * sizeof(int), 0);
        close(fd);
        for (i=0; i<sizes[0]; i++) {
            printf("file_buf[%2d][] = ",i);
            for (j=0; j<sizes[1]; j++) {
                int ij = i*sizes[1] + j;
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

