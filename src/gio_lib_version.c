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
 * This string must be made a global variable. Otherwise, it won't work
 * when compiled with optimization options, e.g. -O2
 */
char const gio_libvers[] =
    "\044Id: \100(#) GIO library version "GIO_VERSION" of "GIO_RELEASE_DATE" $";

/* a cleaner version for running command "strings", e.g.
 * % strings libgio.a | grep "GIO library version"
 * or
 * % strings a.out | grep "GIO library version"
 */
char const gio_lib_vers[] = "GIO library version "GIO_VERSION" of "GIO_RELEASE_DATE;

/* gio_libvers is slightly different from the one returned from
 * GIO_inq_libvers(). The string gio_libvers is for command "ident" to
 * use. People can run command ident libgio.a to obtain the version of a
 * library (or an executable built from that library). In GIO case, the
 * command will print the string of gio_libvers. Command "ident' looks for
 * a specific keyword pattern and print it. See man page of ident.
 */

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

