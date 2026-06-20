/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <gio.h>

/* The const string below is for the RCS ident(1) command to find a string like
 * "\044Id: \100(#) GIO library version 1.0.0 of 13 Apr 2026 $"
 * in the library file (libgio.a).
 *
 * This string must be made a global variable. Otherwise, it won't work when
 * compiled with optimization options, e.g. -O2
 *
 * Contents of string gio_libvers is slightly different from the one to be
 * returned from ncmpi_inq_libvers(). gio_libvers is to be used for command
 * "ident" to identify the RCS keyword strings. Note command "ident' looks for
 * a specific keyword pattern and print it. See man page of ident.  One can run
 * command "ident libfoo.a" to obtain the version string of library named foo
 * (or an executable compiled from that library). In the PnetCDF case, the
 * command "ident libgio.so" will print the contents of gio_libvers.
 */
char const gio_libvers[128] =
    "\044Id: \100(#) GIO library version "GIO_VERSION" of "GIO_RELEASE_DATE" $";

/* Below gio_lib_vers is a cleaner version of gio_libvers. It is for running
 * command "strings", e.g.
 *    % strings libgio.a | grep "^GIO library version"
 * or
 *    % strings a.out | grep "^GIO library version"
 */
char const gio_lib_vers[128] =
    "GIO library version "GIO_VERSION" of "GIO_RELEASE_DATE;

/*----< GIO_inq_libvers() >--------------------------------------------------*/
const char*
GIO_inq_libvers(void) {

    /* for example, "1.0.0 of Apr 23 2026 12:11:30 $"
     * we need some silly operation so the compiler will emit the otherwise
     * unused gio_libvers
     */
    if ((void *)gio_libvers != (void *)GIO_inq_libvers) {
    ; /* do nothing */
    }
    return GIO_VERSION " of " GIO_RELEASE_DATE;
}

