/*
 *  Copyright (C) 2026, Northwestern University
 *  See COPYRIGHT notice in top-level directory.
 */

/* These are routines for allocating and deallocating heap memory dynamically.
   They should be called as
       GIOI_Malloc(size)
       GIOI_Calloc(nelems, esize)
       GIOI_Realloc(ptr, size)
       GIOI_Strdup(ptr)
       GIOI_Free(ptr)

   In macro.h, they are macro-replaced to
       GIOI_Malloc_fn(size, __LINE__, __func__, __FILE__)
       GIOI_Calloc_fn(nelems, esize, __LINE__, __func__, __FILE__)
       GIOI_Realloc_fn(ptr, size, __LINE__, __func__, __FILE__)
       GIOI_Strdup_fn(ptr, __LINE__, __func__, __FILE__)
       GIOI_Free_fn(ptr, __LINE__, __func__, __FILE__).
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strcpy(), strlen() */

#ifdef HAVE_STDINT_H
#include <stdint.h> /*PTRDIFF_MAX */
#endif
#ifndef PTRDIFF_MAX
#define PTRDIFF_MAX SIZE_MAX
#endif

#include <errno.h>  /* EINVAL */
#include <assert.h>

#include <gioi.h>

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

/* GIO_MALLOC_TRACE is set at the configure time when --enable-debug is used */
#ifdef GIO_MALLOC_TRACE

#ifdef HAVE_SEARCH_H
#include <search.h> /* tfind(), tsearch() and tdelete() */
#endif

/* static variables for malloc tracing (initialized to 0s) */
static void   *gioi_mem_root;
static size_t  gioi_mem_alloc;
static size_t  gioi_max_mem_alloc;

#ifdef ENABLE_THREAD_SAFE
/* updating the binary tree used in tfind()/tsearch()/tdelete() is not
 * thread-safe, protect these subroutines with a mutex */
#include<pthread.h>
static pthread_mutex_t gioi_lock = PTHREAD_MUTEX_INITIALIZER;
#endif

#if 0
/*----< gioi_init_malloc_tracing() >---------------------------------------*/
void gioi_init_malloc_tracing(void)
{
    gioi_mem_alloc     = 0;
    gioi_max_mem_alloc = 0;
    gioi_mem_root      = NULL;
}
#endif

typedef struct {
    void   *self;
    void   *buf;
    size_t  size;
    int     lineno;
    char   *func;
    char   *filename;
} gioi_mem_entry;

static
int gioi_cmp(const void *a, const void *b) {
    gioi_mem_entry *fa = (gioi_mem_entry*)a;
    gioi_mem_entry *fb = (gioi_mem_entry*)b;

    if ((char*)fa->buf > (char*)fb->buf) return  1;
    if ((char*)fa->buf < (char*)fb->buf) return -1;
    return 0;
}

static
void gioi_walker(const void *node, const VISIT which, const int depth) {
    gioi_mem_entry *f;
    f = *(gioi_mem_entry **)node;
    if (which == preorder || which == leaf)
        fprintf(stdout, "Warning: malloc yet to be freed (buf=%p size=%zd filename=%s func=%s line=%d)\n", f->buf, f->size, f->filename, f->func, f->lineno);
}

/*----< gioi_add_mem_entry() >-----------------------------------------------*/
/* add a new malloc entry to the table */
static
void gioi_add_mem_entry(void       *buf,
                        size_t      size,
                        const int   lineno,
                        const char *func,
                        const char *filename)
{
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_lock(&gioi_lock);
#endif

    /* use C tsearch utility */
    gioi_mem_entry *node = (gioi_mem_entry*) malloc(sizeof(gioi_mem_entry));

    node->self     = node;
    node->buf      = buf;
    node->size     = size;
    node->lineno   = lineno;
    node->func     = (char*)malloc(strlen(func)+1);
    node->filename = (char*)malloc(strlen(filename)+1);
    strcpy(node->func, func);
    node->func[strlen(func)] = '\0';
    strcpy(node->filename, filename);
    node->filename[strlen(filename)] = '\0';

    /* search and add a new item */
    void *ret = tsearch(node, &gioi_mem_root, gioi_cmp);
    if (ret == NULL) {
        fprintf(stderr, "Error at line %d file %s: tsearch()\n",
                __LINE__,__FILE__);
    }
    else {
        gioi_mem_alloc += size;
        gioi_max_mem_alloc = MAX(gioi_max_mem_alloc, gioi_mem_alloc);
    }
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_unlock(&gioi_lock);
#endif
}

/*----< gioi_del_mem_entry() >---------------------------------------------*/
/* delete a malloc entry from the table */
static
int gioi_del_mem_entry(void *buf)
{
    int err=0;

#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_lock(&gioi_lock);
#endif
    /* use C tsearch utility */
    if (gioi_mem_root != NULL) {
        gioi_mem_entry node;
        node.buf  = buf;
        void *ret = tfind(&node, &gioi_mem_root, gioi_cmp);
        gioi_mem_entry **found = (gioi_mem_entry**) ret;
        if (ret == NULL) {
            fprintf(stderr, "Error at line %d file %s: tfind() buf=%p\n",
                    __LINE__,__FILE__,buf);
            err = 1;
            goto fn_exit;
        }
        /* free space for func and filename */
        free((*found)->func);
        free((*found)->filename);

        /* subtract the space amount to be freed */
        gioi_mem_alloc -= (*found)->size;
        void *tmp = (*found)->self;
        ret = tdelete(&node, &gioi_mem_root, gioi_cmp);
        if (ret == NULL) {
            fprintf(stderr, "Error at line %d file %s: tdelete() buf=%p\n",
                    __LINE__,__FILE__,buf);
            err = 1;
            goto fn_exit;
        }
        free(tmp);
    }
    else
        fprintf(stderr, "Error at line %d file %s: gioi_mem_root is NULL\n",
                __LINE__,__FILE__);
fn_exit:
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_unlock(&gioi_lock);
#endif
    return err;
}
#endif

/*----< GIOI_Malloc_fn() >----------------------------------------------------*/
/* This subroutine is essentially the same as calling malloc().
 * According to malloc man page, If size is 0, then malloc() returns either
 * NULL, or a unique pointer value that can later be successfully passed to
 * free(). Thus, there is no need to check whether size is zero.
 */
void* GIOI_Malloc_fn(size_t      size,
                     const int   lineno,
                     const char *func,
                     const char *filename)
{
    void *buf;

    /* Many C library implementations, particularly those designed to work with
     * GCC and Clang, limit single malloc allocations to PTRDIFF_MAX - 1 bytes.
     * ptrdiff_t is a signed integer type used for the difference between two
     * pointers, and PTRDIFF_MAX is its maximum value. This limitation exists
     * because some compiler optimizations and internal library operations may
     * rely on pointer differences being representable within ptrdiff_t.
     */
    if (size > PTRDIFF_MAX - 1) {
        fprintf(stderr, "GIOI_Malloc(%zd) failed in file %s func %s line %d\n",
                size, filename, func, lineno);
        return NULL;
    }

    buf = malloc(size);

    if (size > 0 && buf == NULL) {
        fprintf(stderr, "GIOI_Malloc(%zd) failed in file %s func %s line %d\n",
                size, filename, func, lineno);
        return NULL;
    }

#ifdef GIO_MALLOC_TRACE
    gioi_add_mem_entry(buf, size, lineno, func, filename);
#endif

    return buf;
}


/*----< GIOI_Strdup() >------------------------------------------------------*/
/* This subroutine is essentially the same as calling strdup().
 */
void* GIOI_Strdup_fn(const char *src,
                     const int   lineno,
                     const char *func,
                     const char *filename)
{
    size_t len;
    void *buf;

    if (src == NULL) return NULL;

    len = strlen(src) + 1;

    if (len > PTRDIFF_MAX - 1) {
        fprintf(stderr, "GIOI_Strdup(%zd) failed in file %s func %s line %d\n",
                len, filename, func, lineno);
        return NULL;
    }

    buf = malloc(len);

    if (buf == NULL) {
        fprintf(stderr, "GIOI_Strdup(%zd) failed in file %s func %s line %d\n",
                len, filename, func, lineno);
        return NULL;
    }

    memcpy(buf, src, len - 1);

    ((char*)buf)[len - 1] = '\0';

#ifdef GIO_MALLOC_TRACE
    gioi_add_mem_entry(buf, len, lineno, func, filename);
#endif

    return buf;
}

/*----< GIOI_Calloc_fn() >----------------------------------------------------*/
/* This subroutine is essentially the same as calling calloc().
 * According to calloc man page, If nelem is 0, then calloc() returns either
 * NULL, or a unique pointer value that can later be successfully passed to
 * free(). Thus, there is no need to check whether nelem is zero.
 */
void* GIOI_Calloc_fn(size_t      nelem,
                     size_t      elsize,
                     const int   lineno,
                     const char *func,
                     const char *filename)
{
    void *buf;

    if (nelem * elsize > PTRDIFF_MAX - 1) {
        fprintf(stderr, "GIOI_Calloc(%zd, %zd) failed in file %s func %s line %d\n",
                nelem, elsize, filename, func, lineno);
        return NULL;
    }

    /* calloc() accepts zero size, to allow free() to be called later */
    buf = calloc(nelem, elsize);

    if (nelem > 0 && buf == NULL) {
        fprintf(stderr, "GIOI_Calloc(%zd, %zd) failed in file %s func %s line %d\n",
                nelem, elsize, filename, func, lineno);
        return NULL;
    }

#ifdef GIO_MALLOC_TRACE
    gioi_add_mem_entry(buf, nelem * elsize, lineno, func, filename);
#endif

    return buf;
}


/*----< GIOI_Realloc_fn() >---------------------------------------------------*/
/* This subroutine is essentially the same as calling realloc().
 * According to realloc man page, if ptr is NULL, then the call is equivalent
 * to malloc(size), for all values of size; if size is equal to zero, and ptr
 * is not NULL, then the call is equivalent to free(ptr). Unless ptr is NULL,
 * it must have been returned by an earlier call to malloc(), calloc() or
 * realloc(). If size was equal to 0, either NULL or a pointer suitable to be
 * passed to free() is returned.
 */
void* GIOI_Realloc_fn(void       *ptr,
                      size_t      size,
                      const int   lineno,
                      const char *func,
                      const char *filename)
{
    void *buf;

    if (size > PTRDIFF_MAX - 1) {
        fprintf(stderr, "GIOI_Realloc(_, %zd) failed in file %s func %s line %d\n",
                size, filename, func, lineno);
        return NULL;
    }

    if (ptr == NULL) return GIOI_Malloc_fn(size, lineno, func, filename);

    if (size == 0) {
        GIOI_Free_fn(ptr, lineno, func, filename);
        return GIOI_Malloc_fn(0, lineno, func, filename);
    }

#ifdef GIO_MALLOC_TRACE
    if (gioi_del_mem_entry(ptr) != 0) {
        fprintf(stderr, "GIOI_Realloc(_, %zd) failed in file %s func %s line %d\n",
                size, filename, func, lineno);
        return NULL;
    }
#endif

    buf = (void *) realloc(ptr, size);
    if (buf == NULL) {
        fprintf(stderr, "GIOI_Realloc(_, %zd) failed in file %s func %s line %d\n",
                size, filename, func, lineno);
        return NULL;
    }

#ifdef GIO_MALLOC_TRACE
    gioi_add_mem_entry(buf, size, lineno, func, filename);
#endif

    return buf;
}


/*----< GIOI_Free_fn() >------------------------------------------------------*/
/* This subroutine is essentially the same as calling free().
 * According to free man page, free() frees the memory space pointed to by ptr,
 * which must have been returned by a previous call to malloc(), calloc() or
 * realloc(). Otherwise, or if free(ptr) has already been called before,
 * undefined  behavior occurs. If ptr is NULL, no operation is performed.
 */
void GIOI_Free_fn(void       *ptr,
                  const int   lineno,
                  const char *func,
                  const char *filename)
{
    if (ptr == NULL) return;

#ifdef GIO_MALLOC_TRACE
    if (gioi_del_mem_entry(ptr) != 0)
        fprintf(stderr, "GIOI_Free failed in file %s func %s line %d\n",
                filename, func, lineno);
#endif

    free(ptr);
}

/*----< GIO_inq_malloc_size() >----------------------------------------------*/
/* This is an independent subroutine, reporting the current aggregate size
 * allocated by malloc, yet to be freed.
 */
int GIO_inq_malloc_size(GIO_Count *size)
{
#ifdef GIO_MALLOC_TRACE
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_lock(&gioi_lock);
#endif
    *size = (GIO_Count)gioi_mem_alloc;
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_unlock(&gioi_lock);
#endif
    return GIO_NOERR;
#else
    DEBUG_RETURN_ERROR(GIO_ENOTENABLED)
#endif
}

/*----< GIO_inq_malloc_max_size() >------------------------------------------*/
/* This is an independent subroutine, reporting the max watermark ever
 * researched by malloc (aggregated amount).
 */
int GIO_inq_malloc_max_size(GIO_Count *size)
{
#ifdef GIO_MALLOC_TRACE
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_lock(&gioi_lock);
#endif
    *size = (GIO_Count)gioi_max_mem_alloc;
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_unlock(&gioi_lock);
#endif
    return GIO_NOERR;
#else
    DEBUG_RETURN_ERROR(GIO_ENOTENABLED)
#endif
}

/*----< GIO_inq_malloc_list() >---------------------------------------------*/
/* This is an independent subroutine, reporting yet-to-be-freed malloc-ed
 * spance by walking the malloc tree and print yet-to-be-freed residues.
 */
int GIO_inq_malloc_list(void)
{
#ifdef GIO_MALLOC_TRACE
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_lock(&gioi_lock);
#endif
    /* check if malloc tree is empty */
    if (gioi_mem_root != NULL)
        twalk(gioi_mem_root, gioi_walker);
#ifdef ENABLE_THREAD_SAFE
    pthread_mutex_unlock(&gioi_lock);
#endif
    return GIO_NOERR;
#else
    DEBUG_RETURN_ERROR(GIO_ENOTENABLED)
#endif
}

