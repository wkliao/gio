#
# Copyright (C) 2026, Northwestern University
# See COPYRIGHT notice in top-level directory.
#
# @configure_input@

SUFFIXES = .a .o .c .h

AM_CPPFLAGS  = -I${srcdir}/src

AM_CFLAGS = @C_VISIBILITY_FLAGS@

if BUILD_AS_SUBPROJECT
   noinst_LTLIBRARIES = libgio.la
else
   lib_LTLIBRARIES = libgio.la
   nodist_include_HEADERS = gio.h
endif

C_SRCS = gio_lib_version.c \
         gio_open.c \
         gio_close.c \
         gio_fstype.c \
         gio_lock.c \
         gio_set_size.c \
         gio_sync.c \
         gio_delete.c \
         gio_hints.c \
         gio_utils.c \
         gio_mem_alloc.c \
         gio_errors.c \
         gio_aggregate.c \
         gio_read_coll.c \
         gio_read_indep.c \
         gio_write_coll.c \
         gio_write_indep.c \
         gio_ufs_open.c \
         gio_ufs_rd_indep.c \
         gio_ufs_rd_coll.c \
         gio_ufs_wr_indep.c \
         gio_ufs_wr_coll.c \
         gio_lustre_open.c \
         gio_lustre_wr_coll.c \
         gio_flatten_subarray.c


libgio_la_SOURCES = $(C_SRCS)

EXTRA_DIST = gioi.h

CLEANFILES = core core.* *.gcda *.gcno *.gcov gmon.out

tests-local: all

# For VPATH build (parallel build), try delete all sub-directories
distclean-local:
	@for d in $(DIST_SUBDIRS) ; do \
       if [ -d $$d/.deps ] ; then rmdir $$d/.deps || true ; fi ; \
    done ; \
    if [ "$(abs_builddir)" != "$(abs_srcdir)" ] ; then \
       for d in $(DIST_SUBDIRS) ; do \
           rmdir $$d || true ; \
       done ; \
    fi

dist-hook:
	$(SED_I) -e "s|DIST_DATE|@GIO_RELEASE_DATE@|g" $(distdir)/gio.h.in

