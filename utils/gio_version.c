/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> /* getopt */

typedef enum {
    Version_number = 0,
    Date           = 1,
    Patches        = 2,
    Configure_args = 3,
    Compilers      = 4,
    LastField
} fields;

/*
 *   gio_version - Report on the GIO version
 */

/*---< usage() >-------------------------------------------------------------*/
static void usage(char *argv0) {
    char *help =
        "Usage: %s [switches]\n"
        "       -v  : version number\n"
        "       -d  : release date\n"
        "       -c  : configure arguments used to build GIO\n"
        "       -b  : MPI compilers used\n"
        "       -h  : print this help (available command-line options)\n";
    fprintf(stderr, help, argv0);
    exit(-1);
}

/*----< main() >-------------------------------------------------------------*/
int main( int argc, char *argv[] )
{
           int     opt;

    int i, flags[10];

    if (argc <= 1) {
        /* Show all values */
        for (i=0; i<LastField; i++) flags[i] = 1;
    }
    else {
        /* Show only requested values */
        for (i=0; i<LastField; i++) flags[i] = 0;
    }

    while ( (opt=getopt(argc,argv,"vdcbh"))!= EOF) {
        switch (opt) {
            case 'v': flags[Version_number] = 1;
                      break;
            case 'd': flags[Date] = 1;
                      break;
            case 'c': flags[Configure_args] = 1;
                      break;
            case 'b': flags[Compilers] = 1;
                      break;
            case 'h':
            default: usage(argv[0]);
                      break;
        }
    }

    /* Print out the information, one item per line */
    if (flags[Version_number]) {
        printf( "GIO Version:    \t%s\n", GIO_VERSION);
    }
    if (flags[Date]) {
        printf( "GIO Release date:\tGIO_RELEASE_DATE\n");
    }
    if (flags[Configure_args]) {
        printf( "GIO configure: \t%s\n", CONFIGURE_ARGS_CLEAN);
    }
    if (flags[Compilers]) {
#ifdef CFLAGS
        if (strcmp(CFLAGS, ""))
            printf( "MPICC:  %s %s\n", MPICC, CFLAGS);
        else
#endif
            printf( "MPICC:  %s\n", MPICC);
    }

    return 0;
}

