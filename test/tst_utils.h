/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */


#ifndef H_TST_UTILS
#define H_TST_UTILS

#ifdef HAVE_CONFIG_H
#include <config.h> /* output of 'configure' */
#endif

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>

#if defined(HAVE_STDBOOL_H) && HAVE_STDBOOL_H == 1
#include <stdbool.h> /* type false and true */
typedef bool boolean;
#else
typedef int boolean;
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
enum {false=0, true=1};
#endif
#endif

#include <gioi.h>

#ifndef MAX
#define MAX(mm,nn) (((mm) > (nn)) ? (mm) : (nn))
#endif
#ifndef MIN
#define MIN(mm,nn) (((mm) < (nn)) ? (mm) : (nn))
#endif

#define MODE_COLL  1
#define MODE_INDEP 0

#define CHECK_ERR(err) { \
    if (err != GIO_NOERR) { \
        printf("Error at line %d in %s: (%s)\n", \
        __LINE__,__FILE__,GIO_strerrno(err)); \
        assert(0); \
    } \
}

#define CHECK_ERR_ALL(err) { \
    if (err != GIO_NOERR) { \
        nerrs++; \
        printf("Error at line %d in %s: (%s)\n", \
        __LINE__,__FILE__,GIO_strerrno(err)); \
    } \
    MPI_Allreduce(MPI_IN_PLACE, &nerrs, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD); \
    if (nerrs > 0) goto err_out; \
}

#define CHECK_ERROUT(err) { \
    if (err != GIO_NOERR) { \
        nerrs++; \
        printf("Error at line %d in %s: (%s)\n", \
        __LINE__,__FILE__,GIO_strerrno(err)); \
        goto err_out; \
    } \
}

#define CHECK_FATAL_ERR(err) { \
    if (err != GIO_NOERR) { \
        nerrs++; \
        printf("Error at line %d in %s: (%s)\n", \
        __LINE__,__FILE__,GIO_strerrno(err)); \
        assert(0); \
    } \
}

#define EXP_ERR(err, exp) { \
    if (err != exp) { \
        nerrs++; \
        printf("Error at line %d in %s: expecting %s but got %s\n", \
        __LINE__,__FILE__,GIO_strerrno(exp), GIO_strerrno(err)); \
    } \
}

#define CHECK_EXP_ERR_ALL(err, exp) { \
    if (err != exp) { \
        nerrs++; \
        printf("Error at line %d in %s: expecting %s but got %s\n", \
        __LINE__,__FILE__,GIO_strerrno(exp), GIO_strerrno(err)); \
    } \
    MPI_Allreduce(MPI_IN_PLACE, &nerrs, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD); \
    if (nerrs > 0) goto err_out; \
}

#define CHECK_NERRS_ALL(nerrs) if (nerrs != 0) assert(0);
/*
#define CHECK_NERRS_ALL { \
    MPI_Allreduce(MPI_IN_PLACE, &nerrs, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD); \
    if (nerrs > 0) assert(0) err_out; \
}
*/

int inq_env_hint(char *hint_key, char **hint_value);

#define PASS_STR "pass (%4.1fs)\n"
#define SKIP_STR "skip\n"
#define FAIL_STR "fail with %d mismatches\n"

#ifndef HAVE_STRDUP
extern char *strdup(const char *s);
#endif
#ifndef HAVE_STRCASECMP
extern int strcasecmp(const char *s1, const char *s2);
#endif

extern
char* remove_file_system_type_prefix(const char *filename);

extern
int is_relax_coord_bound(void);

extern
void tst_usage(char *argv0);

typedef struct {
    char *in_path;  /* input file path for read tests */
    /* below 4 options:
     *      2 for testing both non-default and defaulte settings.
     *      1 for testing non-default setting only.
     *      0 for testing     defaulte setting only.
     */
    int  ds;  /* test of data sieving mode */
    int  mod; /* test of independent data mode */

    boolean  diff; /* run diff on files */
} loop_opts;

extern
int tst_main(int argc, char **argv, char *msg, loop_opts opt,
             int (*tst_body)(const char*, const char*, int, MPI_Info));

#endif
