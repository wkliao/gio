/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>

#include "gioi.h"

/*----< GIO_flatten_subarray() >---------------------------------------------*/
/* This subroutine flattens a subarray access layout, described by arguments
 * esize, sizes, subsizes, and starts, into a list of offset-length pairs.
 *   ndims: number of array dimensions
 *   esize: array element size in bytes
 *   disp: displacement, starting offset
 *   sizes[ndims]: global array sizes
 *   subsizes[ndims]: subarray sizes
 *   starts[ndims]: starting indices of the subarray relative to global array
 *   npairs: number of offset-length pairs
 *   offs[npairs]: array of flattened offsets
 *   lens[npairs]: array of flattened lengths
 */
int GIO_flatten_subarray(int          ndims,
                         int          esize,
                         MPI_Offset   disp,
                         const int   *sizes,
                         const int   *subsizes,
                         const int   *starts,
                         MPI_Offset  *npairs,
                         MPI_Offset **offs,
                         MPI_Offset **lens)
{
    int i;

    if (ndims == 0) return GIO_NOERR;

    if (sizes == NULL || subsizes == NULL || starts == NULL || npairs == NULL)
        return GIO_EINVAL;

    *npairs = subsizes[0];

    *offs = (MPI_Offset*) malloc(sizeof(MPI_Offset) * (*npairs));
    if (*offs == NULL) return GIO_ENOMEM;

    *lens = (MPI_Offset*) malloc(sizeof(MPI_Offset) * (*npairs));
    if (*lens == NULL) return GIO_ENOMEM;

    for (i=0; i<subsizes[0]; i++) {
        (*offs)[i] = disp + ((starts[0] + i) * sizes[1] + starts[1]) * esize;
        (*lens)[i] = subsizes[1] * esize;
    }
    return GIO_NOERR;
}

