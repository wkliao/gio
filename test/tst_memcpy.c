/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *
 * Copyright (C) 2026, Northwestern University
 *
 * This program tests collective write and read calls using a noncontiguous
 * user buffer data layout, which consists of 2 contiguous blocks separated by
 * a gap. The block size and gap size can be adjusted through command-line
 * options.
 *
 * This program is designed to check the performance of collective write by
 * reporting the number of calls to memcpy() made internally by the GIO
 * subroutine LUSTRE_Fill_send_buffer(). A high number of memcpy() calls can
 * happen when the user buffer layout is not contiguous.
 *
 * The performance issue is discovered when running a PIO test program using
 * Lustre. When read/write requests are large and the Lustre striping size is
 * small, then the number of calls to memcpy() can become large, hurting the
 * performance.
 *
 * The original settings of PIO test program using the followings:
 *   The number of MPI process clients = 2048
 *   The number of I/O tasks (aggregators) = 16
 *   The number of variables = 64
 *   One extra small variable is written before 64 variables.
 *   Each variables is a 2D array of size 58 x 10485762
 *   Data partitioning is done along the 2nd dimension
 *   Writes to all 64 subarrays are aggregated into one MPI_File_write call
 *   User buffer consists of two separately allocated memory spaces.
 *
 * To compile:
 *   % mpicc -O2 tst_memcpy.c -o tst_memcpy -lgio
 *
 * Example output of running 16 processes on a local Linux machine using UFS:
 * Note the 2 runs below differ only on whether option "-g 0" is used. Option
 * "-g 0" does not add a gap into the user buffer data layout, which makes the
 * buffer contiguous.
 *
 *   % mpiexec -n 16 tst_memcpy -k 256 -c 32768 -w
 *     Number of global variables = 64
 *     Each global variable is of size 256 x 32768 bytes
 *     Each  local variable is of size 256 x 16 bytes
 *     Gap between the first 2 variables is of size 16 bytes
 *     Number of subarray types concatenated is 8192
 *     Each process makes a request of amount 33554688 bytes
 *     ---------------------------------------------------------
 *     Time of collective write = 33.07 sec
 *     ---------------------------------------------------------
 *
 *   % mpiexec -n 16 tst_memcpy -k 256 -c 32768 -w -g 0
 *     Number of global variables = 64
 *     Each global variable is of size 256 x 32768 bytes
 *     Each  local variable is of size 256 x 16 bytes
 *     Gap between the first 2 variables is of size 0 bytes
 *     Number of subarray types concatenated is 8192
 *     Each process makes a request of amount 33554688 bytes
 *     ---------------------------------------------------------
 *     Time of collective write = 8.27 sec
 *     ---------------------------------------------------------
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* strcpy() */
#include <unistd.h> /* getopt() */
#include <fcntl.h>  /* O_CREAT, O_RDWR */
#include <limits.h> /* INT_MAX */

#include <mpi.h>
#include <gio.h>
#include <tst_utils.h>

#define NVARS 64         /* Number of variables */
#define NROWS 58         /* Number of rows in each variable */
#define NCOLS 1048576    /* Number of rows in each variable */
#define NAGGR 16         /* Number of I/O aggregators */
#define NCLIENTS 2048    /* Number of MPI process clients */
#define GAP 16           /* gap size in the user buffer, mimic 2 malloc() */

typedef struct {
    MPI_Offset  num;
    MPI_Offset  size;
    MPI_Offset *off;
    MPI_Offset *len;
} off_len;

static int verbose;

/*----< tst_memcpy() >------------------------------------------------------*/
static
int tst_memcpy(const char *out_path,
               MPI_Info    info,
               int         nvars,
               int         nrows,
               int         ncols_g,
               int         gap,
               int         do_read)
{
    char *buf;
    int i, j, k, err, nerrs=0, max_nerrs, rank, nprocs, mode;
    int nreqs, ncols;
    double timing[2], max_timing[2];
    GIO_File fh;
    MPI_Offset wlen, rlen;
    off_len bview, fview;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    /* NCLIENTS is the number of MPI processes running the original application
     *      exhibiting the I/O performance issue.
     * NAGGR is the number of I/O aggregators in the original application.
     * nprocs is the number of MPI processes allocated to this run. This test
     *      program is designed to run on nprocs to recreate the memory
     *      operations occurring on the I/O aggregators. Thus, nprocs here
     *      represents the number of I/O aggregators. The original case ran
     *      2048 MPI client processes and 16 of them are I/O aggregators.
     *      processes.
     * nreqs is the number of subarray requests each aggregator writes or
     *      reads. Each original MPI process client forwards all its requests
     *      to one of the I/O aggregators.
     */
    nreqs = nvars * NCLIENTS / nprocs;
    nreqs++; /* one small variable at the beginning */

    /* Data partitioning is done along 2nd dimension */
    ncols = ncols_g / NCLIENTS;

    wlen = (MPI_Offset)nrows * ncols * (nreqs - 1) + nrows;
    if (verbose && rank == 0) {
        printf("Number of global variables = %d\n", nvars);
        printf("Each global variable is of size %d x %d bytes\n",nrows,ncols_g);
        printf("Each  local variable is of size %d x %d bytes\n",nrows,ncols);
        printf("Number of fileview offset-length pairs is %d\n", nreqs);
        printf("Buffer gap between the first 2 pairs is %d bytes\n", gap);
        printf("Each aggregator makes a request of amount %lld bytes\n", wlen);
    }
    /* check 4-byte integer overflow */
    if (wlen > INT_MAX) {
        if (rank == 0) {
            printf("Error: local write size %lld > INT_MAX.\n", wlen);
            printf("       Try increasing number of processes\n");
            printf("       or reduce the block size.\n");
            printf("       nrows=%d ncols=%d\n", nrows,ncols);
        }
        MPI_Abort(MPI_COMM_WORLD, 1);
        exit(1);
    }

    /* User buffer consists of two noncontiguous spaces. */
    bview.num = 2;
    bview.off = (MPI_Offset*) malloc(sizeof(MPI_Offset) * bview.num);
    bview.len = (MPI_Offset*) malloc(sizeof(MPI_Offset) * bview.num);
    bview.off[0] = 0;
    bview.off[1] = nrows + gap;
    bview.len[0] = nrows; /* a small request of size nrows */
    bview.len[1] = nrows * ncols * (nreqs - 1);;
    bview.size = bview.len[0] + bview.len[1];

    buf = (char*) malloc(bview.size + gap);
    for (j=0; j<bview.size+gap; j++) buf[j] = -1;
    for (j=0; j<bview.len[0]; j++)
        buf[j] = (j + rank) % 127;
    j += gap;
    for (; j<bview.len[0]+bview.len[1]; j++)
        buf[j] = (j + rank) % 127;

    /* Set up the file layout. There are nreqs arrays stored in the file and
     * each process writes to a subarray of each varaible.
     */
    fview.num = 1 + (nreqs - 1) * nrows;
    fview.off = (MPI_Offset*) malloc(sizeof(MPI_Offset) * fview.num);
    fview.len = (MPI_Offset*) malloc(sizeof(MPI_Offset) * fview.num);

    /* first is a small variable of size nrows at the beginning of file */
    fview.off[0] = nrows * rank;
    fview.len[0] = nrows;

    MPI_Offset disp = nrows * nprocs;
    k = 1;
    for (i=1; i<nreqs; i++) {
        fview.off[k] = disp + ncols * rank;
        fview.len[k] = ncols;
        k++;
        for (j=1; j<nrows; j++) {
            fview.off[k] = fview.off[k-1] + ncols * nprocs;
            fview.len[k] = ncols;
            k++;
        }
        disp += nrows * ncols * nprocs;
    }

    fview.size = 0;
    for (i=0; i<fview.num; i++)
        fview.size += fview.len[i];

    if (verbose)
        printf("%2d: bview.size=%lld fview.size=%lld\n", rank,
               bview.size,fview.size);
    assert(bview.size == fview.size);

    mode = O_CREAT | O_RDWR;
    err = GIO_open(MPI_COMM_WORLD, out_path, mode, MPI_INFO_NULL, &fh);
    CHECK_ERR(err)

    /* write to the file */
    MPI_Barrier(MPI_COMM_WORLD);
    timing[0] = MPI_Wtime();
    wlen = GIO_write_all(fh, buf, fview.num, fview.off, fview.len,
                                  bview.num, bview.off, bview.len);
    if (wlen < 0) CHECK_ERR((int)wlen)

    timing[0] = MPI_Wtime() - timing[0];

    /* read from the file */
    if (do_read) {
        /* reset contents of buffer */
        for (j=0; j<bview.size; j++) buf[j] = -1;

        MPI_Barrier(MPI_COMM_WORLD);
        timing[1] = MPI_Wtime();
        rlen = GIO_read_all(fh, buf, fview.num, fview.off, fview.len,
                                     bview.num, bview.off, bview.len);
        if (rlen < 0) CHECK_ERR((int)rlen)
        timing[1] = MPI_Wtime() - timing[1];

        /* check contents of read buffer */
        for (j=0; j<nrows; j++) {
            char exp = (j + rank) % 127;
            if (buf[j] != exp) {
                printf("Error: buf[%d] expect %d but got %d\n",j,exp,buf[j]);
                nerrs++;
                break;
            }
        }
        j += gap;
        for (; j<nrows * ncols * (nreqs-1); j++) {
            char exp = (j + rank) % 127;
            if (buf[j] != exp) {
                printf("Error: buf[%d] expect %d but got %d\n",j,exp,buf[j]);
                nerrs++;
                break;
            }
        }
    }

    err = GIO_close(&fh);
    CHECK_ERR(err)

    free(buf);

    MPI_Allreduce(&nerrs, &max_nerrs, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
    MPI_Reduce(timing, max_timing, 2, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    if (verbose && max_nerrs == 0 && rank == 0) {
        printf("---------------------------------------------------------\n");
        printf("Time of collective write = %.2f sec\n", max_timing[0]);
        if (do_read)
            printf("Time of collective read  = %.2f sec\n", max_timing[1]);
        printf("---------------------------------------------------------\n");
    }

    return nerrs;
}

#ifdef STAND_ALONE
static void
usage(char *argv0)
{
    char *help =
    "Usage: %s [-hqr | -n num | -k num | -c num | -g num ] -f file_name\n"
    "       [-h] Print this help\n"
    "       [-q] quiet mode\n"
    "       [-r] performs read  only (default: both write and read)\n"
    "       [-n num] number of global variables (default: %d)\n"
    "       [-k num] number of rows    in each global variable (default: %d)\n"
    "       [-c num] number of columns in each global variable (default: %d)\n"
    "       [-g num] gap in bytes between first 2 blocks (default: %d)\n"
    "        -f file_name: output file name\n";
    fprintf(stderr, help, argv0, NVARS, NROWS, NCOLS, GAP);
}

int main(int argc, char **argv)
{
    extern int optind;
    extern char *optarg;
    char filename[256];
    int i, nerrs=0, rank, nvars, nrows, ncols_g, gap, do_read;

    MPI_Init(&argc,&argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    verbose = 1;
    nvars   = NVARS;
    nrows   = NROWS;
    ncols_g = NCOLS;
    gap     = GAP;
    do_read  = 0;
    filename[0] = '\0';

    /* get command-line arguments */
    while ((i = getopt(argc, argv, "hqrn:k:c:g:f:")) != EOF)
        switch(i) {
            case 'q': verbose = 0;
                      break;
            case 'n': nvars = atoi(optarg);
                      break;
            case 'k': nrows = atoi(optarg);
                      if (nrows < 0) {
                          if (rank == 0)
                              printf("Error: number of rows must >= 0\n");
                          MPI_Finalize();
                          return 1;
                      }
                      break;
            case 'c': ncols_g = atoi(optarg);
                      if (ncols_g < 2048) {
                          if (rank == 0)
                              printf("Error: number of columns must >= %d\n",
                                     NCLIENTS);
                          MPI_Finalize();
                          return 1;
                      }
                      break;
            case 'g': gap = atoi(optarg);
                      break;
            case 'r': do_read = 1;
                      break;
            case 'f': strcpy(filename, optarg);
                      break;

            case 'h':
            default:  if (rank==0) usage(argv[0]);
                      MPI_Finalize();
                      return 1;
        }

    if (filename[0] == '\0') {
        if (rank==0) usage(argv[0]);
        MPI_Finalize();
        return 1;
    }

    nerrs = tst_memcpy(filename, MPI_INFO_NULL, nvars, nrows, ncols_g, gap,
                       do_read);

    MPI_Finalize();

    return (nerrs > 0);
}

#else
static
int test_io(const char *out_path,
            const char *in_path,  /* ignored */
            int         coll_io,  /* ignored */
            MPI_Info    info)
{
    int nerrs, nvars, nrows, ncols_g, gap, do_read;

    verbose = 0;

    nvars   = 2;
    nrows   = 2;
    ncols_g = NCOLS;
    gap     = 8;
    do_read = 1;

    nerrs = tst_memcpy(out_path, info, nvars, nrows, ncols_g, gap, do_read);

    return nerrs;
}

int main(int argc, char **argv)
{
    int err;
    loop_opts opt;

    MPI_Init(&argc, &argv);

    opt.ds   = 2;     /* auto and disable data sieving modes */
    opt.mod  = 0;     /* collective APIs only */
    opt.diff = true;  /* run diff on output files */

    err = tst_main(argc, argv, "check number of memcpy() calls", opt, test_io);

    MPI_Finalize();

    return err;
}
#endif
