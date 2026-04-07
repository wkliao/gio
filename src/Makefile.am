#
# Copyright (C) 2026, Northwestern University
# See COPYRIGHT notice in top-level directory.
#
# @configure_input@

SUFFIXES = .a .o .c .h

AM_CPPFLAGS  = -I${srcdir}/src

if GIO_DEBUG
   AM_CPPFLAGS += -DGIO_DEBUG
endif

noinst_LTLIBRARIES = libgio.la

H_SRCS = gio.h

C_SRCS = gio_open.c \
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
         gio_lustre_wr_coll.c


libgio_la_SOURCES = $(C_SRCS) $(H_SRCS)

EXTRA_DIST = gioi.h

CLEANFILES = core core.* *.gcda *.gcno *.gcov gmon.out

tests-local: all

