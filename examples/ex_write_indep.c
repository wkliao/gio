/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Copyright (C) 2026, Northwestern University
 * See COPYRIGHT notice in top-level directory.
 *
 * This program shows an example to call API GIO_writ(). The file layout
 * is a 2D checkerboad partitioning pattern among processes. The buffer memory
 * layout is a 2D array with ghost cells on both ends of the dimensions.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>     /* O_CREAT, O_RDWR */

#include <mpi.h>
#include <gio.h>

#define LEN 100
#define NGHOSTS 2

#define CHECK_ERR(err) { \
    if (err != GIO_NOERR) { \
        printf("Error at line %d in %s: (%s)\n", \
        __LINE__,__FILE__,GIO_strerrno(err)); \
        goto err_out; \
    } \
}

/*----< main() >-------------------------------------------------------------*/
int main(int argc, char **argv)
{
    const char *filename;
    int i, rank, nprocs, err=0, omode;
    int local_len, *buf, nghosts;
    int psizes[2], sizes[2], subsizes[2], starts[2];
    MPI_Offset wlen, file_npairs, *file_offs, *file_lens;
    MPI_Offset buf_npairs, *buf_offs, *buf_lens;
    GIO_File fh;

    MPI_Init(&argc, &argv);

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (argc == 1) filename = "dummy.dat";
    else           filename = argv[1];

    local_len = LEN;     /* default dimension size */
    nghosts   = NGHOSTS; /* number of ghost cells on both sizes of each dim */

    /* calculate number of processes along each dimension */
    psizes[0] = psizes[1] = 0;
    err = MPI_Dims_create(nprocs, 2, psizes); CHECK_ERR(err)

    /* set the global array sizes */
    subsizes[0] = local_len;
    subsizes[1] = local_len;
    sizes[0]    = local_len * psizes[0];
    sizes[1]    = local_len * psizes[1];
    starts[0]   = local_len * (rank / psizes[1]);
    starts[1]   = local_len * (rank % psizes[1]);

    /* construct a checkerboard partitioning file's layout and flatten it into
     * a list of offset-length pairs.
     */
    err = GIO_flatten_subarray(2, sizeof(int), 0, sizes, subsizes, starts,
                               &file_npairs, &file_offs, &file_lens);
    CHECK_ERR(err)

    /* set the local array sizes */
    subsizes[0] = local_len;
    subsizes[1] = local_len;
    sizes[0]    = subsizes[0] * nghosts * 2;
    sizes[1]    = subsizes[1] * nghosts * 2;
    starts[0]   = nghosts;
    starts[1]   = nghosts;

    /* construct a local buffer memory layout with ghost cells at the both ends
     * of each dimenion, and flatten it into a list of offset-length pairs.
     */
    err = GIO_flatten_subarray(2, sizeof(int), 0, sizes, subsizes, starts,
                               &buf_npairs, &buf_offs, &buf_lens);
    CHECK_ERR(err)

    /* allocate and initialize I/O buffer */
    buf = (int*) malloc(sizeof(int) * sizes[0] * sizes[1]);
    for (i=0; i<sizes[0]*sizes[1]; i++)
        buf[i] = rank + i;

    /* create a new file */
    omode = O_CREAT | O_RDWR;
    err = GIO_open(MPI_COMM_WORLD, filename, omode, MPI_INFO_NULL, &fh);
    CHECK_ERR(err)

    /* write to the file independently */
    wlen = GIO_write(fh, buf, file_npairs, file_offs, file_lens,
                              buf_npairs, buf_offs, buf_lens);
    if (wlen < 0) CHECK_ERR((int)wlen)

    err = GIO_close(&fh); CHECK_ERR(err)

    free(file_offs);
    free(file_lens);
    free(buf_offs);
    free(buf_lens);
    free(buf);

err_out:
    MPI_Allreduce(MPI_IN_PLACE, &err, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);

    MPI_Finalize();

    return (err > 0);
}

