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
    Lmod           = 5,
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
        "       -l  : LMOD PrgEnv module loaded, if available\n"
        "       -h  : print this help (available command-line options)\n";
    fprintf(stderr, help, argv0);
    exit(-1);
}

/*----< main() >-------------------------------------------------------------*/
int main( int argc, char *argv[] )
{
    int i, opt, flags[10];

    if (argc <= 1) {
        /* Show all values */
        for (i=0; i<LastField; i++) flags[i] = 1;
    }
    else {
        /* Show only requested values */
        for (i=0; i<LastField; i++) flags[i] = 0;
    }

    while ( (opt=getopt(argc,argv,"vdcblh"))!= EOF) {
        switch (opt) {
            case 'v': flags[Version_number] = 1;
                      break;
            case 'd': flags[Date] = 1;
                      break;
            case 'c': flags[Configure_args] = 1;
                      break;
            case 'b': flags[Compilers] = 1;
                      break;
            case 'l': flags[Lmod] = 1;
                      break;
            case 'h':
            default: usage(argv[0]);
                      break;
        }
    }

    /* Print out the information, one item per line */
    if (flags[Version_number])
        printf("GIO Version:    \t%s\n", GIO_VERSION);

    if (flags[Date])
        printf("GIO Release date:\tGIO_RELEASE_DATE\n");

    if (flags[Configure_args])
        printf("GIO configure: \t%s\n", CONFIGURE_ARGS_CLEAN);

    printf("\n");

    if (flags[Compilers]) {
        printf("MPICC:  %s\n", MPICC);
#ifdef CFLAGS
        if (strcmp(CFLAGS, ""))
            printf("        CFLAGS: %s\n", CFLAGS);
        else
            printf("        CFLAGS:\n");
#else
        printf("        CFLAGS:\n");
#endif
        printf("        MPI standard version: %s\n", MPI_VERSION);
        printf("        MPI compiler vendor: %s %s\n", MPI_VENDOR, MPI_VENDOR_VERSION);
        printf("        Base compiler: %s\n", MPICC_BASE);
        printf("        Base compiler version: %s\n", MPICC_BASE_VERSION);
        printf("\n");
    }

    if (flags[Lmod]) {
        char *env_str = getenv("LMOD_FAMILY_PRGENV");

        if (env_str == NULL) env_str = "N/A";

        printf("LMOD PrgEnv module loaded: %s\n", env_str);
        printf("\n");
    }

    return 0;
}

