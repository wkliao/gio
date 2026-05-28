/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */


#include <stdio.h>
#include <libgen.h>    /* basename() */
#include <limits.h>
#include <string.h>    /* strchr(), strerror(), strdup(), strcpy(), strlen() */
#include <unistd.h>    /* getopt(), stat(), lseek(), pread(), close() */
#include <sys/types.h> /* stat(), pread() */
#include <sys/stat.h>  /* stat() */
#include <fcntl.h>     /* open() */
#include <errno.h>     /* errno */

#include <mpi.h>

#include "tst_utils.h"

/* File system types recognized by ROMIO in MPICH 4.0.0 */
static const char* fstypes[] = {"ufs", "nfs", "xfs", "pvfs2", "gpfs", "panfs", "lustre", "daos", "testfs", "ime", "quobyte", NULL};

/* Return a pointer to filename by removing the file system type prefix name if
 * there is any.  For example, when filename = "lustre:/home/foo/testfile.nc",
 * remove "lustre:" to return a pointer to "/home/foo/testfile.nc", so the name
 * can be used in POSIX open() calls.
 */
char* remove_file_system_type_prefix(const char *filename)
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

static
int file_diff(const char *path1, const char *path2)
{
    char *fname1, *fname2, *buf1=NULL, *buf2=NULL;
    int i, err, fd1, fd2, numDiff;
    size_t ntimes, rem, amnt;
    off_t fsize1, fsize2, off;
    ssize_t rlen;

    fname1 = remove_file_system_type_prefix(path1);
    fname2 = remove_file_system_type_prefix(path2);

    fd1 = open(fname1, O_RDONLY);
    if (fd1 < 0) {
        fprintf(stderr,"Error at file open %s (%s)", fname1, strerror(errno));
        return -1;
    }
    fd2 = open(fname2, O_RDONLY);
    if (fd2 < 0) {
        fprintf(stderr,"Error at file open %s (%s)", fname2, strerror(errno));
        return -1;
    }

    fsize1 = lseek(fd1, 0, SEEK_END);
    fsize2 = lseek(fd1, 0, SEEK_END);
    if (fsize1 != fsize2) {
        fprintf(stderr,"File sizes are different, %lld != %lld\n",
                (long long)fsize1, (long long)fsize2);
        return -1;
    }

    buf1 = (char*) malloc(1048576);
    buf2 = (char*) malloc(1048576);

    ntimes = fsize1 / 1048576;
    if (fsize1 % 1048576) ntimes++;

    rem = fsize1;
    off = 0;
    for (i=0; i<ntimes; i++) {
        amnt = MIN(rem, 1048576);
        rlen = pread(fd1, buf1, amnt, off);
        if (rlen < 0 || rlen != amnt) {
            fprintf(stderr,"Error %s at %d: read (%s)", __func__,__LINE__,
                    strerror(errno));
            numDiff = -1;
            goto err_out;
        }
        if (rlen != amnt) {
            fprintf(stderr,"Error %s at %d: read expect read amont %zd, but got %zd)",
                    __func__,__LINE__, amnt, rlen);
            numDiff = -1;
            goto err_out;
        }

        rlen = pread(fd2, buf2, amnt, off);
        if (rlen < 0 || rlen != amnt) {
            fprintf(stderr,"Error %s at %d: read (%s)", __func__,__LINE__,
                    strerror(errno));
            numDiff = -1;
            goto err_out;
        }
        if (rlen != amnt) {
            fprintf(stderr,"Error %s at %d: read expect read amont %zds, but got %zd)",
                    __func__,__LINE__, amnt, rlen);
            numDiff = -1;
            goto err_out;
        }

        numDiff = memcmp(buf1, buf2, amnt);
        if (numDiff != 0)
            goto err_out;

        off += amnt;
        rem -= amnt;
    }

err_out:
    err = close(fd1);
    if (err < 0)
        fprintf(stderr,"Error at file close (%s)", strerror(errno));
    err = close(fd2);
    if (err < 0)
        fprintf(stderr,"Error at file close (%s)", strerror(errno));

    if (buf1 != NULL) free(buf1);
    if (buf2 != NULL) free(buf2);

    return numDiff;
}

void
static tst_main_usage(char *argv0)
{
    char *base_name = basename(argv0);
    char *help =
    "Usage: %s [OPTIONS]...[filename]\n"
    "       [-h] Print help\n"
    "       [-q] quiet mode\n"
    "       [-k] Keep output files (default: no)\n"
    "       [-i  in_path]: input file path (default: NULL)\n"
    "       [-o out_path]: output file name (default: %s.dat)\n";
    fprintf(stderr, help, base_name, base_name);
}

int tst_main(int        argc,
             char      **argv,
             char       *msg,  /* short description about the test */
             loop_opts   opt,  /* test options */
             int       (*tst_body)(const char*,const char*,int,MPI_Info))
{
    extern int optind;
    extern char *optarg;
    char *in_path=NULL, *out_path=NULL;
    int i, nprocs, rank, err, nerrs=0, keep_files, quiet;
    int s, m, s_mod, e_mod, s_ds, e_ds;

    MPI_Info info=MPI_INFO_NULL;

    double timing = MPI_Wtime();
#if GIO_PROFILING == 1
    double itiming[256]; int k=0;
#endif

    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    keep_files = 0;
    quiet = 0;

    while ((i = getopt(argc, argv, "hqki:o:")) != EOF)
        switch(i) {
            case 'q':
                quiet = 1;
                break;
            case 'k':
                keep_files = 1;
                break;
            case 'i':
                in_path = strdup(optarg);
                break;
            case 'o':
                out_path = strdup(optarg);
                break;
            case 'h':
            default:  if (rank==0) tst_main_usage(argv[0]);
                      MPI_Finalize();
                      exit(1);
        }

    if (out_path == NULL)
        out_path = strdup("testfile.dat");

    if (rank == 0) {
        char *cmd_str = (char *)malloc(strlen(argv[0]) + 256);
        sprintf(cmd_str, "*** TESTING C   %s - %s", basename(argv[0]), msg);
        printf("%-63s -- ", cmd_str);
        free(cmd_str);
    }

    char cmd_opts[64];
    sprintf(cmd_opts, "Rank %d: diff", rank);

    char *ptr = strrchr(out_path, '.');
    if (ptr != NULL) *ptr = '\0';

    MPI_Info_create(&info);

    /* Set common I/O hints. Using smaller values for hints below can make
     * the tests more rigorous.
     */
    MPI_Info_set(info, "ind_wr_buffer_size", "60");
    MPI_Info_set(info, "ind_rd_buffer_size", "70");
    MPI_Info_set(info, "cb_buffer_size", "500");

#define SET_OPT(key) {               \
    if (opt.key == 2) {              \
        s_ ## key = 1; e_##key = 0;  \
    }                                \
    else if (opt.key == 1) {         \
        s_##key = 1; e_##key = 1;    \
    }                                \
    else { /* #key == 0 */           \
        s_##key = 0; e_##key = 0;    \
    }                                \
}

    SET_OPT(mod)    /* test collective/independent data mode */
    SET_OPT(ds)     /* test date sieving mode */

    char out_filename[512], ext[16], *base_file;

    base_file = NULL;
    strcpy(ext, "dat");

    /* For indice s,m, 0 is default setting */
    for (s=s_ds;   s>=e_ds;   s--) {
    for (m=s_mod;  m>=e_mod;  m--) {

#if GIO_PROFILING == 1
        MPI_Barrier(MPI_COMM_WORLD);
        itiming[k] = MPI_Wtime();
#endif

        sprintf(out_filename, "%s.%s", out_path, ext);

        /* Whether or not to enable data sieving */
        if (s == 0) { /* default */
            MPI_Info_set(info, "ds_read",  "automatic");
            MPI_Info_set(info, "ds_write", "automatic");
            strcat(out_filename, ".ds");
        }
        else {
            MPI_Info_set(info, "ds_read",  "disable");
            MPI_Info_set(info, "ds_write", "disable");
            strcat(out_filename, ".nods");
        }

        /* Test collective or independent APIs */
        if (m == 0) /* collective data mode */
            strcat(out_filename, ".coll_mod");
        else /* independent data mode */
            strcat(out_filename, ".indep_mod");

#define RUN_ERR(msg, fname) {\
printf("\n%s %-44s (DS=%s mode=%s)\n", \
       msg, fname, (s)?"no":"auto", (m)?"indp":"coll"); \
}

#define DIFF_ERR(msg, f1, f2) {\
printf("\n%s %-44s %s (DS=%s mode=%s)\n", \
       msg, f1, f2, (s)?"no":"auto", (m)?"indp":"coll"); \
}

        double time_body = MPI_Wtime();
        if (!quiet && rank == 0)
            RUN_ERR("Testing", out_filename)

        /* tst_body() is the core of test program */
        int coll_io = (m == 0);
        nerrs = tst_body(out_filename, in_path, coll_io, info);
        MPI_Allreduce(MPI_IN_PLACE, &nerrs, 1, MPI_INT, MPI_MAX,
                      MPI_COMM_WORLD);
        if (nerrs > 0) {
            fflush(stdout);
            if (rank == 0)
                RUN_ERR("\nFAILED", out_filename)
            goto err_out;
        }

        if (!quiet) {
            time_body = MPI_Wtime() - time_body;
            MPI_Allreduce(MPI_IN_PLACE, &time_body, 1, MPI_DOUBLE, MPI_MAX,
                          MPI_COMM_WORLD);
            if (rank == 0)
                printf(" (%.2fs)\n", time_body);
        }

#if GIO_PROFILING == 1
        itiming[k] = MPI_Wtime() - itiming[k]; k++;
#endif
        /* wait for all processes to complete */
        MPI_Barrier(MPI_COMM_WORLD);

        /* run diff to compare output files */
        if (base_file == NULL) { /* skip first file */
            base_file = strdup(out_filename);
            goto skip_diff;
        }

        if (!opt.diff) goto skip_diff;

        if (rank > 0) goto skip_diff;

        /* only root calls file_diff() to compare two files */
        nerrs = 0;
        if (strcmp(base_file, out_filename)) {
            if (!quiet)
                DIFF_ERR("diff", out_filename, base_file)

            if (0 != file_diff(base_file, out_filename))
                nerrs = 1;
        }

skip_diff:
        MPI_Bcast(&nerrs, 1, MPI_INT, 0, MPI_COMM_WORLD);
        if (nerrs != 0) {
            if (rank == 0)
                DIFF_ERR("FAILED: diff", out_filename, base_file)
            goto err_out;
        }

        /* wait for all ranks to complete diff before file delete */
        MPI_Barrier(MPI_COMM_WORLD);

        if (!keep_files && base_file != NULL &&
            strcmp(base_file, out_filename)) {

            if (rank == 0) unlink(out_filename);
            /* wait for deletion to complete before next iteration */
            MPI_Barrier(MPI_COMM_WORLD);
        }
    } /* loop m */
    } /* loop s */

    if (base_file != NULL) {
        if (!keep_files) {
            if (rank == 0) unlink(base_file);

            /* all ranks wait for root to complete file deletion */
            MPI_Barrier(MPI_COMM_WORLD);
        }
        free(base_file);
    }

    MPI_Info_free(&info);

    /* check if there is any malloc residue */
    MPI_Offset malloc_size, sum_size;
    err = GIOI_inq_malloc_size(&malloc_size);
    if (err == GIO_NOERR) {
        MPI_Reduce(&malloc_size, &sum_size, 1, MPI_OFFSET, MPI_SUM, 0, MPI_COMM_WORLD);
        if (rank == 0 && sum_size > 0)
            printf("heap memory allocated by GIO internally has %lld bytes yet to be freed\n",
                   sum_size);
        if (malloc_size > 0) GIOI_inq_malloc_list();
    }

err_out:
    if (in_path  != NULL) free(in_path);
    if (out_path != NULL) free(out_path);

    timing = MPI_Wtime() - timing;
    MPI_Allreduce(MPI_IN_PLACE, &timing, 1, MPI_DOUBLE, MPI_MAX,MPI_COMM_WORLD);
    MPI_Allreduce(MPI_IN_PLACE, &nerrs, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

#if GIO_PROFILING == 1
    MPI_Allreduce(MPI_IN_PLACE, itiming, 256, MPI_DOUBLE, MPI_MAX, MPI_COMM_WORLD);
    if (rank == 0 && !quiet) {
        for (i=0; i<k; i++) printf("k=%d timing[%3d]=%.4f\n",k,i,itiming[i]);
        printf("\n");
    }
#endif

    if (rank == 0) {
        if (nerrs)
            printf(FAIL_STR, nerrs);
        else
            printf(PASS_STR, timing);
    }

    return (nerrs > 0);
}

