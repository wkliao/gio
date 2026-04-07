/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

/*
gio_aggregate.c:         MPI_Irecv_c(
gio_aggregate.c:         MPI_Isend_c(
gio_lustre_wr_coll.c:    MPI_Alltoallv_c(
gio_lustre_wr_coll.c:    MPI_Issend_c(
gio_lustre_wr_coll.c:    MPI_Type_create_hindexed_c(
gio_lustre_wr_coll.c:    MPI_Alltoallw_c(
gio_ufs_rd_coll.c:       MPI_Get_count_c(
gio_ufs_wr_coll.c:       MPI_Unpack_c(
*/

#ifndef H_GIOI
#define H_GIOI

#include <stdlib.h>

#include "gio.h"

#if GIO_DEBUG_MODE == 1
#include <assert.h>

#define DEBUG_RETURN_ERROR(err) {                                     \
    char *_env_str = getenv("GIO_VERBOSE_DEBUG_MODE");                \
    if (_env_str != NULL && *_env_str != '0') {                       \
        int _rank;                                                    \
        MPI_Comm_rank(MPI_COMM_WORLD, &_rank);                        \
        fprintf(stderr, "Rank %d: %s error at line %d of %s in %s\n", \
        _rank,gio_strerrno(err),__LINE__,__func__,__FILE__);          \
    }                                                                 \
    return err;                                                       \
}
#else
#define DEBUG_RETURN_ERROR(err) return err;
#endif

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#if defined(F_SETLKW64)
#define GIO_UNLOCK(fh, offset, whence, len) \
        GIO_GEN_SetLock64(fh, F_SETLK, F_UNLCK, offset, whence, len)
#define GIO_WRITE_LOCK(fh, offset, whence, len) \
        GIO_GEN_SetLock64(fh, F_SETLKW, F_WRLCK, offset, whence, len)
#else
#define GIO_UNLOCK(fh, offset, whence, len) \
        GIO_GEN_SetLock(fh, F_SETLK, F_UNLCK, offset, whence, len)
#define GIO_WRITE_LOCK(fh, offset, whence, len) \
        GIO_GEN_SetLock(fh, F_SETLKW, F_WRLCK, offset, whence, len)
#endif

#define GIO_PERM          0666   /* file creation permission mask */

#define GIO_FS_UFS        152    /* Unix file system */
#define GIO_FS_LUSTRE     163    /* Lustre file system */

#define GIO_LUSTRE_MAX_OSTS 256  /* Maximum number of Lustre OSTs if hint
                                  * striping_factor is not set by user.
                                  */

#define GIO_CB_BUFFER_SIZE_DFLT     "16777216"
#define GIO_IND_RD_BUFFER_SIZE_DFLT "4194304"
#define GIO_IND_WR_BUFFER_SIZE_DFLT "4194304"
#define GIO_CB_CONFIG_LIST_DFLT     "*:1"

/* GIO_DS_WR_NPAIRS_LB is the lower bound of the total number of
 *     offset-length pairs over the non-aggregator senders to be received by an
 *     I/O aggregator to skip the potentially expensive heap-merge sort that
 *     determines whether or not data sieving write is necessary.
 * GIO_DS_WR_NAGGRS_LB is the lower bound of the number of non-aggregators
 *     sending their offset-length pairs to an I/O aggregator.
 * Both conditions must be met to skip the heap-merge sort.
 *
 * When data sieving is enabled, read-modify-write will perform at each round
 * of two-phase I/O at each aggregator. The following describes whether
 * detecting "holes" in a write region is necessary, depending on the data
 * sieving hint, romio_ds_write, is set to enable/disable/automatic.
 *   + automatic - We need to check whether holes exist. If holes exist, the
 *       "read-modify" part must run. If not, "read-modify" can be skipped.
 *   + enable - "read-modify" part must perform, skip hole checking, and thus
 *       skip the heap-merge sort.
 *   + disable - "read-modify" part must skip, need not check holes, but must
 *       construct srt_off_len to merge all others_req[] into a single sorted
 *       list, which requires to call a heap-merge sort. This step is necessary
 *       because write data from all non-aggregators are received into the same
 *       write_buf, with a possibility of overlaps, and srt_off_len stores the
 *       coalesced offset-length pairs of individual non-contiguous write
 *       request and will be used to write them to the file.
 *
 * Heap-merge sort merges offset-length pairs received from all non-aggregators
 * into a single list, which can be expensive. Its cost can be even larger than
 * the cost of "read" in "read-modify-write". Below two constants are the lower
 * bounds used to determine whether or not to perform such sorting, when data
 * sieving is set to the automatic mode.
 */
#define GIO_DS_WR_NPAIRS_LB 8192
#define GIO_DS_WR_NAGGRS_LB 256
#define DO_HEAP_MERGE(nrecv, npairs) ((nrecv) > GIO_DS_WR_NAGGRS_LB || (npairs) > GIO_DS_WR_NPAIRS_LB)

#define GIO_HINT_AUTO -1
#define GIO_HINT_DISABLE 0
#define GIO_HINT_ENABLE 1

#define GIO_STRIPING_AUTO -1
#define GIO_STRIPING_INHERIT 0

typedef struct {
    int nc_striping;
    int striping_factor;
    int striping_unit;
    int start_iodevice;
    int cb_nodes;
    int cb_buffer_size;
    int ind_rd_buffer_size;
    int ind_wr_buffer_size;

    int cb_read;
    int cb_write;
    int ds_read;
    int ds_write;

    /* Hints for Lustre file system */
    int lustre_overstriping_ratio;

    /* Hints set by GIO internally */
    int lustre_num_osts;
    int *aggr_ranks; /* [cb_nodes] rank IDs of I/O aggregators */

} GIO_Hints;

typedef struct {
    GIO_Count size;       /* total size in bytes, i.e. sum of len[*], 0 means
                           * zero-sized request. -1 means view has been reset
                           * (in this case count should be 0).
                           */
    GIO_Count npairs;     /* number of off-len pairs. 0 means the entire file
                           * is visible. 0 or 1 means buf_view/file_view is
                           * contiguous. Only when noncontiguous, off and len
                           * are malloc-ed. Note 0 does not necessarily means
                           * zero-sized request.
                           */
    const GIO_Count *off; /* [count] byte offsets */
    const GIO_Count *len; /* [count] block lengths in bytes */
    GIO_Count        idx; /* index of off-len pairs consumed so far */
    GIO_Count        rem; /* remaining amount in the pair to be consumed */
} GIO_View;

typedef struct GIOI_File {
    MPI_Comm comm;        /* communicator indicating who called open */
    int  num_nodes;       /* number of unique compute nodes */
    int *ids;             /* [nprocs] node ID of each MPI process */

    const char *filename;

    int fstype;           /* type of file system: GIO_FS_LUSTRE, GIO_FS_UFS */

    int fd_sys;           /* system file descriptor */
    int amode;            /* O_CREAT|O_RDWR, O_RDWR, or O_RDONLY */

    int is_open;          /* no_indep_rw, 0: not open yet 1: is open */

    int skip_read;        /* whether to skip reads in read-modify-write */

    GIO_View fview;       /* file view's flattened offset-length pairs */
    GIO_View bview;       /* buffer view's flattened offset-length pairs */

    int atomicity;         /* true or false */
    char *io_buf;          /* internal buffer allocated for two-phase I/O */
    int is_agg;            /* whether this process is an aggregator */
    int my_cb_nodes_index; /* my index into fh->hints->aggr_ranks[].
                            * -1 if N/A */

    GIO_Hints *hints;      /* hints used by GIO */
    MPI_Info info;         /* contains all I/O hints */
} GIOI_File;

typedef struct {
    GIO_Count *offsets;   /* array of offsets */
#ifdef HAVE_MPI_LARGE_COUNT
    GIO_Count *lens;      /* array of lengths */
    GIO_Count  *mem_ptrs; /* array of pointers. used in the read/write phase to
                           * indicate where the data is stored in memory
                           * promoted to GIO_Count so we can construct types
                           * with _c versions
                           */
    GIO_Count   count;    /* size of above arrays */
#else
    int        *lens;
    MPI_Aint   *mem_ptrs;
    size_t      count;
#endif
    size_t curr; /* index of offsets/lens that is currently being processed */
} GIO_Access;

#if defined(GIO_PROFILING) && (GIO_PROFILING == 1)
#define NTIMERS 10
extern double    gio_wr_time[NTIMERS];
extern double    gio_rd_time[NTIMERS];
extern MPI_Count gio_wr_count[NTIMERS];
extern MPI_Count gio_rd_count[NTIMERS];
#endif

/*---- APIs -----------------------------------------------------------------*/

#define SANITY_CHECK(fh, fn, fo, fl, bn, bo, bl) \
    GIO_sanity_check(__func__, __LINE__, fh, fn, fo, fl, bn, bo, bl)

extern int
GIO_sanity_check(const char *func_name, int lineno, GIO_File fh,
              GIO_Count file_npairs, const GIO_Count *file_offs,
              const GIO_Count *file_lens, GIO_Count buf_npairs,
              const GIO_Count *buf_offs, const GIO_Count *buf_lens);

/* utility APIs */
extern char*
GIOI_remove_file_system_type_prefix(const char *filename);

extern void*
GIOI_Malloc_fn(size_t size, const int lineno, const char *func,
              const char *filename);

extern void*
GIOI_Strdup_fn(const char *src, const int lineno, const char *func,
              const char *filename);

extern void*
GIOI_Calloc_fn(size_t nelem, size_t elsize, const int lineno, const char *func,
              const char *filename);

extern void*
GIOI_Realloc_fn(void *ptr, size_t size, const int lineno, const char *func,
               const char *filename);

extern void
GIOI_Free_fn(void *ptr, const int lineno, const char *func,
            const char *filename);

#define GIOI_Malloc(a)    GIOI_Malloc_fn(a,__LINE__,__func__,__FILE__)
#define GIOI_Strdup(a)    GIOI_Strdup_fn(a,__LINE__,__func__,__FILE__)
#define GIOI_Calloc(a,b)  GIOI_Calloc_fn(a,b,__LINE__,__func__,__FILE__)
#define GIOI_Realloc(a,b) GIOI_Realloc_fn(a,b,__LINE__,__func__,__FILE__)
#define GIOI_Free(a)      GIOI_Free_fn(a,__LINE__,__func__,__FILE__)

extern int
GIOI_inq_malloc_size(size_t *size);

extern int
GIOI_inq_malloc_max_size(size_t *size);

extern int
GIOI_inq_malloc_list(void);


extern int
GIO_FileSysType(const char *filename);

extern void
GIO_Calc_file_domains(int cb_nodes, int striping_unit,
                GIO_Count min_st_off, GIO_Count max_end_off,
                GIO_Count **fd_end, GIO_Count *fd_size);

extern void
GIO_Calc_my_req(GIO_File fh, GIO_Count min_st_off,
                const GIO_Count *fd_end, GIO_Count fd_size,
                GIO_Count *my_req_naggr, GIO_Count *count_per_aggr,
                GIO_Access **my_req, GIO_Count **buf_idx);

extern void
GIO_Calc_others_req(GIO_File fh, GIO_Count my_req_naggr,
                const GIO_Count *count_per_aggr, const GIO_Access *my_req,
                GIO_Access **others_req);

extern int
GIO_Calc_aggregator(int striping_unit, int cb_nodes, const int *cb_node_list,
                GIO_Count min_st_off, GIO_Count fd_size,
                const GIO_Count *fd_end, GIO_Count off, GIO_Count *len);

extern void
GIO_Heap_merge(GIO_Access *others_req, const GIO_Count *count,
                GIO_Count *srt_off, GIO_Count *srt_len,
                const GIO_Count *start_pos, int nprocs, int nprocs_recv,
                GIO_Count total_elements);

/* File lock APIs */
extern int
GIO_GEN_SetLock(GIO_File fh, int cmd, int type, GIO_Count offset,
                int whence, GIO_Count len);

extern int
GIO_GEN_SetLock64(GIO_File fh, int cmd, int type, GIO_Count offset,
                int whence, GIO_Count len);

/* UFS driver APIs */
extern int
GIO_UFS_open(GIO_File fh);

extern GIO_Count
GIO_UFS_write_indep(GIO_File fh, const void *buf);

extern GIO_Count
GIO_UFS_write_coll(GIO_File fh, const void *buf);

extern GIO_Count
GIO_UFS_read_indep(GIO_File fh, void *buf);

extern GIO_Count
GIO_UFS_read_coll(GIO_File fh, void *buf);

extern GIO_Count
GIO_UFS_write_contig(GIO_File fh, const void *buf, GIO_Count w_size,
                GIO_Count offset, int is_coll);

extern GIO_Count
GIO_UFS_read_contig(GIO_File fh, void *buf, GIO_Count r_size,
                GIO_Count offset);

/* Lustre driver APIs */
extern int
GIO_Lustre_create(GIO_File fh);

extern int
GIO_Lustre_open(GIO_File fh);

extern GIO_Count
GIO_Lustre_write_coll(GIO_File fh, const void *buf);

/* Error handle subroutines */
extern int
GIOI_error_posix(char *err_msg);

extern int
GIOI_error_mpi(int mpi_errorcode, const char *err_msg);

#endif
