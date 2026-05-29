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
                _rank,GIO_strerrno(err),__LINE__,__func__,__FILE__);  \
    }                                                                 \
    return err;                                                       \
}
#else
#define DEBUG_RETURN_ERROR(err) return err;
#endif

#define MPI_ERR(err, api) {                                           \
    if (err != MPI_SUCCESS) {                                         \
        int world_rank, errorclass, errorStringLen;                   \
        char errorString[MPI_MAX_ERROR_STRING];                       \
        MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);                   \
        MPI_Error_class(err, &errorclass);                            \
        MPI_Error_string(err, errorString, &errorStringLen);          \
        fprintf(stderr,"Rank %d: Error in %s at %d: %s() (%s)\n",     \
                world_rank,  __func__, __LINE__, api, errorString);   \
    }                                                                 \
}

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

#if defined(F_SETLKW64)
#define GIOI_UNLOCK(fh, offset, whence, len) \
        GIOI_GEN_SetLock64(fh, F_SETLK, F_UNLCK, offset, whence, len)
#define GIOI_WRITE_LOCK(fh, offset, whence, len) \
        GIOI_GEN_SetLock64(fh, F_SETLKW, F_WRLCK, offset, whence, len)
#else
#define GIOI_UNLOCK(fh, offset, whence, len) \
        GIOI_GEN_SetLock(fh, F_SETLK, F_UNLCK, offset, whence, len)
#define GIOI_WRITE_LOCK(fh, offset, whence, len) \
        GIOI_GEN_SetLock(fh, F_SETLKW, F_WRLCK, offset, whence, len)
#endif

#define GIOI_PERM          0666   /* file creation permission mask */

#define GIOI_FS_UFS        152    /* Unix file system */
#define GIOI_FS_LUSTRE     163    /* Lustre file system */

#define GIOI_LUSTRE_MAX_OSTS 256  /* Maximum number of Lustre OSTs if hint
                                  * striping_factor is not set by user.
                                  */

#define GIOI_CB_BUFFER_SIZE_DFLT     "16777216"
#define GIOI_IND_RD_BUFFER_SIZE_DFLT "4194304"
#define GIOI_IND_WR_BUFFER_SIZE_DFLT "4194304"
#define GIOI_CB_CONFIG_LIST_DFLT     "*:1"

/* GIOI_DS_WR_NPAIRS_LB is the lower bound of the total number of
 *     offset-length pairs over the non-aggregator senders to be received by an
 *     I/O aggregator to skip the potentially expensive heap-merge sort that
 *     determines whether or not data sieving write is necessary.
 * GIOI_DS_WR_NAGGRS_LB is the lower bound of the number of non-aggregators
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
#define GIOI_DS_WR_NPAIRS_LB 8192
#define GIOI_DS_WR_NAGGRS_LB 256
#define DO_HEAP_MERGE(nrecv, npairs) ((nrecv) > GIOI_DS_WR_NAGGRS_LB || (npairs) > GIOI_DS_WR_NPAIRS_LB)

#define GIOI_HINT_AUTO -1
#define GIOI_HINT_DISABLE 0
#define GIOI_HINT_ENABLE 1

#define GIOI_STRIPING_AUTO -1
#define GIOI_STRIPING_INHERIT 0

typedef struct {
    int file_striping;
    int striping_factor;
    int striping_unit;
    int start_iodevice;
    int cb_nodes;
    int cb_buffer_size;

    /* Hints for Lustre file system */
    int overstriping_ratio;

    /* hints below are not required to be consistent among all processes */
    int ind_rd_buffer_size;
    int ind_wr_buffer_size;

    int cb_read;
    int cb_write;
    int ds_read;
    int ds_write;

    /* NUMA node ID of this rank */
    int NUMA_ID;

    /* Hints set by GIO, not changeable by users */
    int lustre_num_osts;
    int *aggr_ranks; /* [cb_nodes] rank IDs of I/O aggregators */

} GIOI_Hints;

typedef struct {
    MPI_Offset size;       /* total size in bytes, i.e. sum of len[*]. 0 means
                            * zero-sized request (in this case npairs == 0)
                            */
    MPI_Offset npairs;     /* number of off-len pairs. 0 means zero-sized
                            * request. > 1 means noncontiguous request. Only
                            * when npairs > 0, off and len are malloc-ed.
                            */
    const MPI_Offset *off; /* [npairs] byte offsets */
    const MPI_Offset *len; /* [npairs] block lengths in bytes */
    MPI_Offset        idx; /* index of off-len pairs consumed so far */
    MPI_Offset        rem; /* remaining amount in pair idx to be consumed */
} GIOI_View;

typedef struct GIOI_File {
    MPI_Comm comm;   /* communicator indicating who called open */
    int num_NUMAs;   /* number of unique NUMA compute nodes */
    int *NUMA_IDs;   /* [nprocs] node ID of each MPI process */

    char *filename;  /* duplicated internal from user's filename */

    int fstype;      /* type of file system: GIOI_FS_LUSTRE, GIOI_FS_UFS */

    int fd_sys;      /* system file descriptor */
    int amode;       /* O_CREAT|O_RDWR, O_RDWR, or O_RDONLY */

    int is_open;     /* no_indep_rw, 0: not open yet 1: is open */

    int skip_read;   /* whether to skip reads in read-modify-write */

    GIOI_View fview; /* file view's flattened offset-length pairs */
    GIOI_View bview; /* buffer view's flattened offset-length pairs */

    int atomicity;         /* true or false */
    char *io_buf;          /* internal buffer allocated for two-phase I/O */
    int is_agg;            /* whether this process is an aggregator */
    int my_cb_nodes_index; /* my index into fh->hints->aggr_ranks[].
                            * -1 if N/A */

    GIOI_Hints *hints; /* hints used by GIO */
    MPI_Info info;     /* contains all I/O hints */
} GIOI_File;

typedef struct {
    MPI_Offset  num;  /* number of elements in off[], len[], ptr[] */
    MPI_Offset  cur;  /* index to off[]/len[] currently being processed */
    MPI_Offset *off;  /* array of offsets */
    MPI_Offset *len;  /* array of lengths */
    MPI_Offset *ptr;  /* array of pointers. used in the read/write phase to
                       * indicate where the data is stored in memory buffer
                       */
} GIOI_Access;

#if GIO_PROFILING_MODE == 1
#define NTIMERS 10
extern double     gio_wr_time[NTIMERS];
extern double     gio_rd_time[NTIMERS];
extern MPI_Offset gio_wr_count[NTIMERS];
extern MPI_Offset gio_rd_count[NTIMERS];
#endif

/*---- APIs -----------------------------------------------------------------*/

#define SANITY_CHECK(fh, fn, fo, fl, bn, bo, bl) \
    GIOI_sanity_check(__func__, __LINE__, fh, fn, fo, fl, bn, bo, bl)

extern int
GIOI_sanity_check(const char *func_name, int lineno, GIO_File fh,
              MPI_Offset file_npairs, const MPI_Offset *file_offs,
              const MPI_Offset *file_lens, MPI_Offset buf_npairs,
              const MPI_Offset *buf_offs, const MPI_Offset *buf_lens);

extern int
GIOI_set_info(GIO_File fh, MPI_Info info);

/* utility APIs */
extern char*
GIOI_remove_file_system_type_prefix(const char *filename);

extern void*
GIOI_Malloc_fn(size_t size, const int lineno, const char *func,
              const char *filename);

extern void*
GIOI_Malloc_align_fn(size_t size, size_t alignment, const int lineno,
              const char *func, const char *filename);

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
#define GIOI_Malloc_align(a,b) GIOI_Malloc_align_fn(a,b,__LINE__,__func__,__FILE__)

extern int
GIOI_inq_malloc_size(MPI_Offset *size);

extern int
GIOI_inq_malloc_max_size(MPI_Offset *size);

extern int
GIOI_inq_malloc_list(void);

extern int
GIOI_type_create_hindexed(MPI_Offset count, MPI_Offset *off, MPI_Offset *len,
                MPI_Datatype *newType);

extern int
GIOI_type_contiguous(MPI_Offset count, MPI_Datatype *newType);


/*---- gio_fstype.c ---------------------------------------------------------*/
extern int
GIOI_FileSysType(const char *filename);

extern void
GIOI_Calc_file_domains(int cb_nodes, int striping_unit,
                MPI_Offset min_st_off, MPI_Offset max_end_off,
                MPI_Offset **fd_end, MPI_Offset *fd_size);

extern void
GIOI_Calc_my_req(GIO_File fh, MPI_Offset min_st_off,
                const MPI_Offset *fd_end, MPI_Offset fd_size,
                MPI_Offset *my_req_naggr, MPI_Offset *count_per_aggr,
                GIOI_Access **my_req, MPI_Offset **buf_idx);

extern int
GIOI_Calc_others_req(GIO_File fh, MPI_Offset my_req_naggr,
                const MPI_Offset *count_per_aggr, const GIOI_Access *my_req,
                GIOI_Access **others_req);

extern int
GIOI_Calc_aggregator(int striping_unit, int cb_nodes, const int *cb_node_list,
                MPI_Offset min_st_off, MPI_Offset fd_size,
                const MPI_Offset *fd_end, MPI_Offset off, MPI_Offset *len);

extern void
GIOI_Heap_merge(GIOI_Access *others_req, const MPI_Offset *count,
                MPI_Offset *srt_off, MPI_Offset *srt_len,
                const MPI_Offset *start_pos, int nprocs, int nprocs_recv,
                MPI_Offset total_elements);

/* File lock APIs */
extern int
GIOI_GEN_SetLock(GIO_File fh, int cmd, int type, MPI_Offset offset,
                int whence, MPI_Offset len);

extern int
GIOI_GEN_SetLock64(GIO_File fh, int cmd, int type, MPI_Offset offset,
                int whence, MPI_Offset len);

/* UFS driver APIs */
extern int
GIOI_UFS_open(GIO_File fh);

extern int
GIOI_UFS_open_on_demand(GIO_File fh);

extern MPI_Offset
GIOI_UFS_write_indep(GIO_File fh, const void *buf);

extern MPI_Offset
GIOI_UFS_write_coll(GIO_File fh, const void *buf);

extern MPI_Offset
GIOI_UFS_read_indep(GIO_File fh, void *buf);

extern MPI_Offset
GIOI_UFS_read_coll(GIO_File fh, void *buf);

extern MPI_Offset
GIOI_UFS_write_contig(GIO_File fh, const void *buf, MPI_Offset w_size,
                MPI_Offset offset, int is_coll);

extern MPI_Offset
GIOI_UFS_read_contig(GIO_File fh, void *buf, MPI_Offset r_size,
                MPI_Offset offset);

#if HAVE_DECL_PREAD == 1
#define GIOI_pread pread
#else
extern ssize_t
GIOI_pread(int fd, void *buf, size_t count, off_t offset);
#endif

#if HAVE_DECL_PWRITE == 1
#define GIOI_pwrite pwrite
#else
extern ssize_t
GIOI_pwrite(int fd, const void *buf, size_t count, off_t offset);
#endif

/* Lustre driver APIs */
extern int
GIOI_Lustre_create(GIO_File fh);

extern int
GIOI_Lustre_open(GIO_File fh);

extern int
GIOI_Lustre_open_on_demand(GIO_File fh);

extern MPI_Offset
GIOI_Lustre_write_coll(GIO_File fh, const void *buf);

/* Error handle subroutines */
extern int
GIOI_error_posix(char *err_msg);

extern int
GIOI_error_mpi(int mpi_errorcode, const char *err_msg);

#endif
