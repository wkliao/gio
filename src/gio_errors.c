/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRERROR
#include <string.h> /* contains prototype for ansi libc function strerror() */
#else
/* provide a strerror function for older unix systems */
static char *
strerror(int errnum)
{
    extern int sys_nerr;
    extern char *sys_errlist[];

    if(errnum < 0 || errnum >= sys_nerr) return NULL;
    /* else */
    return sys_errlist[errnum];
}
#endif /* HAVE_STRERROR */

#include <errno.h>

#include "gioi.h"

/*----< GIOI_error_posix() ------------------------------------------------*/
/* Translate posix io error codes to GIO error codes */
int GIOI_error_posix(char *err_msg)       /* extra error message */
{
#if defined(HAVE_STRERROR) && (HAVE_STRERROR == 1)
    char *errorString= strerror(errno);
#else
    char *errorString="Other I/O error";
#endif

    /* check for specific error codes understood by GIO */
    switch (errno){
        case ENOSPC :
            return GIO_ENO_SPACE;
        case ENAMETOOLONG:
        case ENOTDIR :
        case EISDIR:
            return GIO_EBAD_FILE;
        case EDQUOT:
            return GIO_EQUOTA;
        case ENOENT:
            return GIO_ENOENT;
        case EEXIST:
            return GIO_EEXIST;
    }

    /* other errors that currently have no corresponding GIO error codes */
    if (err_msg == NULL) err_msg = "";

    printf("IO error (%s) : %s\n", err_msg, errorString);

    return GIO_EFILE; /* other unknown file I/O error */
}

/*----< GIOI_error_mpi() --------------------------------------------------*/
/* translate MPI error codes to GIO error codes */
int GIOI_error_mpi(int         mpi_errorcode,
                   const char *err_msg) /* extra error message */
{
    int errorclass, errorStringLen;
    char errorString[MPI_MAX_ERROR_STRING];
    const char *dump_str = (err_msg == NULL) ? "" : err_msg;

    /* check for specific error codes understood by GIO */

    /* When GIO_NOCLOBBER is used in ioflags(cmode) for open to create,
     * netCDF requires GIO_EEXIST returned if the file already exists.
     * In MPI standard 2.1, if MPI_File_open uses MPI_MODE_EXCL and the file has
     * already existed, the error class MPI_ERR_FILE_EXISTS should be returned.
     * For opening an existing file but the file does not exist, MPI 2.1
     * will return MPI_ERR_NO_SUCH_FILE
     * Note for MPI 2.1 and prior, we return MPI_ERR_IO, as these error classes
     * have not been defined.
     */
    MPI_Error_class(mpi_errorcode, &errorclass);

    if (errorclass == MPI_ERR_ARG)  return GIO_EINVAL;

    if (errorclass == MPI_ERR_FILE_EXISTS)  return GIO_EEXIST;
    if (errorclass == MPI_ERR_NO_SUCH_FILE) return GIO_ENOENT;
    /* MPI-IO should return MPI_ERR_NOT_SAME when one or more arguments of a
     * collective MPI call are different. However, MPI-IO may not report this
     * error code correctly. For instance, some MPI-IO returns MPI_ERR_AMODE
     * instead when amode is found inconsistent. MPI_ERR_NOT_SAME can also
     * report inconsistent file name. */
    if (errorclass == MPI_ERR_NOT_SAME) return GIO_EMULTIDEFINE_FNC_ARGS;
    /* MPI-IO may or may not report MPI_ERR_AMODE if inconsistent amode is
     * detected. MPI_ERR_AMODE can also indicate other conflict amode used
     * on each process. But in GIO, MPI_ERR_AMODE can only be caused by
     * inconsistent file open/create mode. So, if MPI-IO returns this error
     * we are sure it is because of the inconsistent mode */
    if (errorclass == MPI_ERR_AMODE)     return GIO_EMULTIDEFINE_OMODE;
    if (errorclass == MPI_ERR_READ_ONLY) return GIO_EPERM;
    if (errorclass == MPI_ERR_ACCESS)    return GIO_EACCESS;
    if (errorclass == MPI_ERR_BAD_FILE)  return GIO_EBAD_FILE;
    if (errorclass == MPI_ERR_NO_SPACE)  return GIO_ENO_SPACE;
    if (errorclass == MPI_ERR_QUOTA)     return GIO_EQUOTA;

    /* other errors that currently have no corresponding GIO error codes,
     * or the error class is MPI_ERR_IO (Other I/O error). For example,
     * MPI_ERR_INFO_VALUE (MPI info Value longer than MPI_MAX_INFO_VAL).
     */

    MPI_Error_string(mpi_errorcode, errorString, &errorStringLen);
#ifdef GIO_DEBUG
    /* report the world rank */
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    printf("rank %d: MPI error (%s) : %s\n", rank, dump_str, errorString);
#else
    printf("MPI error (%s) : %s\n", dump_str, errorString);
#endif

    return GIO_EFILE; /* other unknown file I/O error */
}

#define ISSYSERR(err) ((err) > 0)

static char GIO_unknown_err_msg[128];

const char*
GIO_strerror(int err)
{
    sprintf(GIO_unknown_err_msg,"Unknown Error: Unrecognized error code %5d\n",err);

    if (ISSYSERR(err)) {
        const char *cp = (const char *) strerror(err);
        if (cp == NULL) return GIO_unknown_err_msg;
        return cp;
    }

    switch (err) {
        case GIO_NOERR:
            return "No error.";
        case GIO_EINVAL:
	        return "Invalid argument";
        case GIO_EPERM:
	        return "Write to read only";
        case GIO_ENOMEM:
	        return "Memory allocation (malloc) failure";
        case GIO_EACCESS:
	        return "Access failure";

        case GIO_EEXIST:
            return strerror(EEXIST);
        case GIO_ENEGATIVECNT:
            return "Negative count is prohibited";
            /* Number of offset-length pairs for file view or buffer view is
             * negative.
             */
        case GIO_EFILE:
            return "Unknown error in file operation";
            /* This error is caused by an unsuccessful file I/O call. */
        case GIO_ENOENT:
            return strerror(ENOENT);
        case GIO_ENOTENABLED:
            return "Feature is not enabled at configure time.";
        case GIO_EBAD_FILE:
            return "Invalid file name (e.g., path name too long)";
        case GIO_ENO_SPACE:
            return strerror(ENOSPC);
        case GIO_EQUOTA:
            return strerror(EDQUOT);
        case GIO_EFSTYPE:
            return "Invalid file system type.";
        case GIO_EMULTIDEFINE_OMODE:
            return "File open mode is inconsistent among processes.";
        case GIO_EMULTIDEFINE_FNC_ARGS:
            return "Arguments in collective API are inconsistent among processes.";
        case GIO_EMULTIDEFINE_HINTS:
            return "I/O hints are not consistent among processes.";
        case GIO_EFILEVIEW:
            return "File view offsets are not in a monotonically non-decreasing order.";

        default:
            return GIO_unknown_err_msg;
    }
}

/*----< GIO_strerrno() >-----------------------------------------------------*/
/* return the GIO error code name */
const char *
GIO_strerrno(int err)
{
    static char unknown_str[64];

    if (err > 0) { /* system error */
        const char *cp = (const char *) strerror(err);
        if (cp == NULL)
            sprintf(unknown_str,"Unknown error code %d",err);
        else
            sprintf(unknown_str,"System error code %d (%s)",err,cp);
        return unknown_str;
    }

#define RETURN_ERRORNO(x) case x : return #x ;

    switch (err) {
        RETURN_ERRORNO(GIO_NOERR)
        RETURN_ERRORNO(GIO_EINVAL)
        RETURN_ERRORNO(GIO_EPERM)
        RETURN_ERRORNO(GIO_ENOMEM)
        RETURN_ERRORNO(GIO_EACCESS)
        RETURN_ERRORNO(GIO_EEXIST)
        RETURN_ERRORNO(GIO_ENEGATIVECNT)
        RETURN_ERRORNO(GIO_EFILE)
        RETURN_ERRORNO(GIO_ENOENT)
        RETURN_ERRORNO(GIO_ENOTENABLED)
        RETURN_ERRORNO(GIO_EBAD_FILE)
        RETURN_ERRORNO(GIO_ENO_SPACE)
        RETURN_ERRORNO(GIO_EQUOTA)
        RETURN_ERRORNO(GIO_EFSTYPE)
        RETURN_ERRORNO(GIO_EMULTIDEFINE_OMODE)
        RETURN_ERRORNO(GIO_EMULTIDEFINE_FNC_ARGS)
        RETURN_ERRORNO(GIO_EMULTIDEFINE_HINTS)
        RETURN_ERRORNO(GIO_EFILEVIEW)
        default:
            sprintf(unknown_str,"Unknown code %d",err);
    }
    return unknown_str;
}

