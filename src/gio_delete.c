/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h> /* unlink() */
#endif

#include "gioi.h"

/*----< GIO_delete() >-------------------------------------------------------*/
int GIO_delete(const char *filename)
{
    int err = GIO_NOERR;
    char *path = GIOI_remove_file_system_type_prefix(filename);

    err = unlink(path);
    if (err != 0)
        err = GIOI_error_posix("unlink");

    return err;
}

