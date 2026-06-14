dnl
dnl Copyright (C) 2026, Northwestern University
dnl See COPYRIGHT notice in top-level directory.
dnl

dnl -*- Mode: shell-script-mode; -*-
dnl Process this file with GNU autoconf(1) to produce a configure script.
dnl

AC_REVISION([m4_esyscmd_s([if test -d .git ; then git describe --always ; fi])])

AC_PREREQ([2.70])
AC_INIT([GIO], [1.0.0],
        [],
        [gio],
        [])

dnl config.h.in will be created by autoreconf (autoheader)
dnl call it right after AC_INIT, as suggested by autoconf
AC_CONFIG_HEADERS([src/config.h])
AH_TOP([/*
 * Copyright (C) 2026, Northwestern University
 * See COPYRIGHT notice in top-level directory.
 */
])
AC_CONFIG_SRCDIR([src/gio.h.in])

AC_CONFIG_AUX_DIR([./scripts])

dnl call env or printenv to print all user environment variables in config.log
echo '-------------------------------' >& AS_MESSAGE_LOG_FD
echo '  User environment variables:'   >& AS_MESSAGE_LOG_FD
echo '-------------------------------' >& AS_MESSAGE_LOG_FD
env >& AS_MESSAGE_LOG_FD
echo '-------------------------------' >& AS_MESSAGE_LOG_FD
echo '' >& AS_MESSAGE_LOG_FD

# search all local macro files in folder m4
AC_CONFIG_MACRO_DIRS([m4])

dnl Note getting command line should be done before calling AM_INIT_AUTOMAKE
dnl as AM_INIT_AUTOMAKE modifies command line $*
if test "x$ac_configure_args_raw" = x ; then
   CONFIGURE_ARGS_CLEAN=`echo $* | tr '"' ' '`
else
   CONFIGURE_ARGS_CLEAN=$ac_configure_args_raw
fi

# automake version 1.16.5 was released in 2021-10-03
AM_INIT_AUTOMAKE([1.16.5 foreign])

# enable silent rules by default
AM_SILENT_RULES([yes])

# if maintainer mode is disabled, make will *never* attempt to rebuild
# configure, Makefile.ins, etc.
AM_MAINTAINER_MODE([enable])

# AM_PROG_AR is first recognized/traced in autoconf 2.68
# Use it only if we want support for unusual archivers such as Microsoft
# lib.. AM_PROG_AR must be called before LT_INIT
AM_PROG_AR

LT_PREREQ([2.5.4])
LT_INIT

# Overwrite func_cc_basename() defined in libtool.m4
# This step is necessary, as all MPI compilers are wrappers and libtool needs
# the exact compiler names to select proper compile and link flags.
UD_FUNC_CC_BASENAME

# AM_EXTRA_RECURSIVE_TARGETS macro was introduced into automake 1.13
# This enable to run command "make tests" to build all test programs.
AM_EXTRA_RECURSIVE_TARGETS([tests])

# To debug configure process (not compilation process), add "gio_ac_debug=yes"
# at the configure command line
AC_DEFUN([UD_MSG_DEBUG],
         [if test "x${gio_ac_debug}" = xyes ; then
             AC_MSG_NOTICE([DEBUG: $1])
          fi
         ]
)

# parse the package version numbers to 4 env variables
GIO_VERSION_MAJOR=`echo ${PACKAGE_VERSION} | cut -d. -f1`
GIO_VERSION_MINOR=`echo ${PACKAGE_VERSION} | cut -d. -f2`
GIO_VERSION_SUB=`echo ${PACKAGE_VERSION} | cut -d. -f3 | cut -d'-' -f1`
GIO_VERSION_PRE=`echo ${PACKAGE_VERSION} | cut -d'-' -f2`

# Note major, minor, and sub are required, but pre is not.
GIO_VERSION=${PACKAGE_VERSION}

AC_SUBST(GIO_VERSION_MAJOR)
AC_SUBST(GIO_VERSION_MINOR)
AC_SUBST(GIO_VERSION_SUB)
AC_SUBST(GIO_VERSION_PRE)
AC_SUBST(GIO_VERSION)

# Note that command 'date' is not portable across Unix platforms.
# But release date only matters to GIO developers who make the releases.
GIO_RELEASE_DATE="`date '+%B %-d, %Y'`"
AC_SUBST(GIO_RELEASE_DATE)
GIO_RELEASE_DATE_FULL="`date '+%Y-%m-%d'`"
AC_SUBST(GIO_RELEASE_DATE_FULL)

AC_SUBST(PACKAGE_VERSION)
AC_SUBST(CONFIGURE_ARGS_CLEAN)

# autoheader only adds templates set below to the first invocation of
# AC_CONFIG_HEADERS
AH_TEMPLATE([GIOI_MALLOC_TRACE],   [Define if to enable malloc tracing])
AH_TEMPLATE([HAVE_MPI_LARGE_COUNT], [Define if MPI APIs have arguments of type MPI_Count])

# an option to use a customized rm command
RM=rm
AC_ARG_VAR(RM, Command for deleting files or directories. @<:@default: rm@:>@)
if test "x${RM}" != x ; then
   dnl ${RM} set by user may contain flags
   RM_CMD=`echo ${RM} | cut -d' ' -f1`
   # use "if ! test" is not portable, according to autoconf user guide
   if test -f ${RM_CMD} ; then :; else
      AC_CHECK_PROG([rm_cmd], [${RM_CMD}], [yes], [no])
      if test "x${rm_cmd}" = xno ; then
         RM=rm
      fi
   fi
fi
UD_MSG_DEBUG([RM=$RM])

# AC_PROG_SED and AC_PROG_GREP are only available on autoconf 2.60 and later
AC_PROG_AWK
AC_PROG_GREP
AC_PROG_EGREP
AC_PROG_SED
# check sed command option -i and set SED_I (this requires RM defined)
UD_PROG_SED_I

AC_ARG_ENABLE(echo,
   [AS_HELP_STRING([--enable-echo],
                   [Turn on strong echoing. @<:@default: disabled@:>@])],
   [set -x]
)

MPI_INSTALL=
AC_ARG_WITH(mpi,
   [AS_HELP_STRING([--with-mpi=DIR],
                   [Provide the MPI installation path in DIR.])],
   [ dnl this clause is run when --with-mpi or --without-mpi is used
   if test "x${withval}" = x ; then
      # when "--with-mpi=" is used
      AC_MSG_ERROR([--with-mpi= is set but the value is missing])
   elif test "x${withval}" != xyes && test ! -d "${withval}" ; then
      # user may use --with-mpi without an argument, which results in withval
      # being "yes". If an argument is given, check if the folder exists.
      AC_MSG_ERROR(Directory '${withval}' specified in --with-mpi does not exist or is not a directory)
   fi
   MPI_INSTALL=${withval}
   ]
)

AC_ARG_VAR(MPICC, [MPI C compiler, @<:@default: CC@:>@])

# MPI_INSTALL can be 'yes', 'no', or a path
if test "x$MPI_INSTALL" != xno ; then
   # Search MPICC's path and check if MPICC is valid

   # MPI_INSTALL will be referred in UD_MPI_PATH_PROGS and UD_MPI_PATH_PROG
   # defined in acinclude.m4

   CANDIDATE_MPICC="mpicc mpicc_r"

   # add GNU MPI compilers
   CANDIDATE_MPICC="$CANDIDATE_MPICC mpigcc mpgcc mpigcc_r mpgcc_r"

   # add IBM MPI compilers
   CANDIDATE_MPICC="$CANDIDATE_MPICC mpcc_r mpcc mpixlc_r mpixlc"

   # add IBM BGL MPI compilers
   CANDIDATE_MPICC="$CANDIDATE_MPICC blrts_xlc mpxlc_r mpxlc"

   # add Fujitsu MPI compilers
   CANDIDATE_MPICC="$CANDIDATE_MPICC mpifccpx"

   # add Cray MPI compiler wrappers
   CANDIDATE_MPICC="$CANDIDATE_MPICC cc"

   # add Intel MPI compiler wrappers
   CANDIDATE_MPICC="$CANDIDATE_MPICC mpiicc icc"

   # add PGI MPI compiler wrappers
   CANDIDATE_MPICC="$CANDIDATE_MPICC mpipgcc mppgcc"

   # save MPICC, MPICXX, MPIF77, MPIF90 set by user before modifying them
   ac_user_MPICC=$MPICC

   # Logic of checking MPICC:
   #   if MPICC environment variable is set
   #   then use MPICC
   #   else if --with-mpi is set
   #        then use CANDIDATE_MPICC to find MPICC in MPI_INSTALL
   #        else use CANDIDATE_MPICC to find MPICC in PATH
   #   if MPICC still not set and CC is set
   #   then set MPICC to CC
   #   else MPICC is not found and error out

   # Obtain full paths of MPICC
   if test "x${MPICC}" = x ; then
      # if MPICC has not been set by users, then search from
      # CANDIDATE_MPICC, and find the full path of MPICC
      # UD_MPI_PATH_PROGS also check MPI compilers in MPI_INSTALL
      UD_MPI_PATH_PROGS([MPICC], [$CANDIDATE_MPICC])
   else
      # check whether user specified MPICC exists
      UD_MPI_PATH_PROG([MPICC], [$MPICC])
   fi

   # If MPICC is still not set and CC is set, then set MPICC to CC.
   if test "x$MPICC"  = x && test "x$CC"  != x ; then
      # check whether user specified CC exists
      UD_MPI_PATH_PROG([MPICC], [$CC])
   fi

   if test "x${MPICC}" = x ; then
      if test "x$ac_user_MPICC" = x ; then
         ERR_MSG="No MPI C compiler can be found"
      else
         ERR_MSG="Specified MPI C compiler \"$ac_user_MPICC\" cannot be found"
      fi
      if test "x$MPI_INSTALL" != x ; then
         ERR_MSG="$ERR_MSG under $MPI_INSTALL"
      fi
      AC_MSG_ERROR([
      -----------------------------------------------------------------------
        $ERR_MSG
        GIO requires a working MPI C compiler. Please specify the location
        of an MPI C compiler, either in the MPICC environment variable
        (not CC variable) or through --with-mpi configure flag. Abort.
      -----------------------------------------------------------------------])
   fi
   UD_MSG_DEBUG([MPICC=$MPICC])
   mpi_mode="enabled"
   AC_SUBST([GIO_MPI_ENABLED], [1])
else
   # Parallel I/O feature is disbaled.
   AC_MSG_WARN([Parallel I/O feature is disabled])
   MPICC=
   mpi_mode="disabled"
   AC_SUBST([GIO_MPI_ENABLED], [0])
fi

# Now MPICC is configured if parallel I/O feature is enabled, do some basic
# compiler tests
CC=${MPICC}
AC_PROG_CC([${MPICC}])
# AM_PROG_CC_C_O

# get compiler vendor in ax_cv_c_compiler_vendor (e.g. gnu, intel)
AX_COMPILER_VENDOR

if test "x${ax_cv_c_compiler_vendor}" = xgnu ; then
   # gcc command-line option "-dumpversion" can also show version.
   AC_MSG_CHECKING([whether gcc version is greater than 10.0.0])
   AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#if __GNUC__ < 10
#error gcc version < 10.0.0
#endif
   ]])], [gcc_ge_10=yes], [gcc_ge_10=no])
   AC_MSG_RESULT([$gcc_ge_10])
fi

if test "x$MPICC" != x ; then
   # get base compiler command of MPI C compiler wrapper in
   # ac_cv_mpi_compiler_base_MPICC (e.g. gcc-14, nvc, icx)
   UD_MPI_COMPILER_BASENAME(MPICC)
   UD_MSG_DEBUG(MPICC=${MPICC} basename=$ud_cv_mpi_compiler_basename_MPICC)

   # MPICXX=CC
   # UD_MPI_COMPILER_BASENAME(MPICXX)
   # UD_MSG_DEBUG(MPICXX=${MPICXX} basename=$ud_cv_mpi_compiler_basename_MPICXX)
   # MPIF90=ftn
   # UD_MPI_COMPILER_BASENAME(MPIF90)
   # UD_MSG_DEBUG(MPIF90=${MPIF90} basename=$ud_cv_mpi_compiler_basename_MPIF90)
fi

# Set output variable CPP to a command that runs the C preprocessor.
# Some C compilers require -E to be used as C preprocessor.
AC_PROG_CPP

# this call needs at least autoconf version 2.60
# AC_USE_SYSTEM_EXTENSIONS

# enable large file support
AC_SYS_LARGEFILE

# UD_PROG_CC_MAKEDEPEND

# AC_HEADER_STDC
# AC_CHECK_HEADERS([malloc.h])

if test "x$MPICC" != x ; then
   # check if MPICC works for basic MPI call: MPI_Comm_rank()
   AC_CHECK_FUNC([MPI_Comm_rank], [],
      dnl maybe -lmpi is needed at link stage
      [AC_SEARCH_LIBS([MPI_Comm_rank], [mpi mpich], [], [AC_MSG_ERROR([
      -----------------------------------------------------------------------
        Invalid MPI compiler specified or detected: "${MPICC}"
        A working MPI C compiler is required. Please specify the location
        of one either in the MPICC environment variable (not CC variable) or
        through --with-mpi configure flag. Abort.
      -----------------------------------------------------------------------])
   ])])

   # print out MPI version and vendor information
   CHECK_MPI_VERSION
fi

# compute canonical system types
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

if test "x$MPICC" != x && test "x$enable_shared" = xyes ; then
   # Call LT_OUTPUT to produce file ./libtool which is used later in
   # LT_AC_CHECK_SHLIB. Note LT_OUTPUT must be called after all compilers
   # checked, such as AC_PROG_CC, AC_PROG_CXX, AC_PROG_FC
   LT_OUTPUT

   # Check whether MPICC supports shared libraries.
   LT_MPI_CHECK_SHLIB
   if test "x$ac_cv_lt_mpi_check_shlib" = xno ; then
      AC_MSG_ERROR([
   -----------------------------------------------------------------------
     configure option --enable-shared is set, but the MPI library is not
     built with shared library support. Either remove option --enable-shared
     or use an MPI library built with shared library support. Abort.
   -----------------------------------------------------------------------])
   fi
fi

# AC_PROG_INSTALL
# AC_PROG_LN_S
# AC_PROG_MAKE_SET
# AC_SUBST(SET_MAKE)

# Check for <stdbool.h> that conforms to C99 requirements
AC_HEADER_STDBOOL

# AC_C_CONST
AC_C_INLINE

AC_CHECK_MEMBERS([struct stat.st_blksize])

AC_CHECK_HEADERS([unistd.h fcntl.h malloc.h stddef.h sys/types.h limits.h time.h dirent.h])

AC_CHECK_DECL([strerror], [], [], [[#include <string.h>]])
if test "x$ac_cv_have_decl_strerror" = xyes ; then
   AC_CHECK_FUNCS([strerror])
fi
AC_CHECK_DECL([lstat], [], [], [[#include <sys/types.h>
                                 #include <sys/stat.h>
                                 #include <unistd.h>]])
if test "x$ac_cv_have_decl_lstat" = xyes ; then
   AC_CHECK_FUNCS([lstat])
fi
AC_CHECK_DECL([open], [], [], [[#include <sys/types.h>
                                #include <sys/stat.h>
                                #include <fcntl.h>]])
if test "x$ac_cv_have_decl_open" = xyes ; then
   AC_CHECK_FUNCS([open])
fi
AC_CHECK_DECL([access], [], [], [[#include <unistd.h>]])
if test "x$ac_cv_have_decl_access" = xyes ; then
   AC_CHECK_FUNCS([access])
fi
AC_CHECK_DECL([truncate], [], [], [[#include <unistd.h>
#include <sys/types.h>]])
if test "x$ac_cv_have_decl_truncate" = xyes ; then
   AC_CHECK_FUNCS([truncate])
fi
AC_CHECK_DECL([unlink], [], [], [[#include <unistd.h>]])
if test "x$ac_cv_have_decl_unlink" = xyes ; then
   AC_CHECK_FUNCS([unlink])
fi
AC_CHECK_DECL([symlink], [], [], [[#include <unistd.h>]])
if test "x$ac_cv_have_decl_symlink" = xyes ; then
   AC_CHECK_FUNCS([symlink])
fi

AM_CONDITIONAL(TEST_SYMLINK, [test "x$ac_cv_func_unlink" = xyes &&
                              test "x$ac_cv_func_symlink" = xyes &&
                              test "x$ac_cv_func_lstat" = xyes])

AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
                    [Enable GIO internal debug mode. @<:@default: disabled@:>@])],
    [debug=${enableval}], [debug=no]
)

dnl malloc memory allocation tracing relies on tdelete and tsearch
AC_CHECK_HEADERS([search.h])
if test "x$ac_cv_header_search_h" = xyes ; then
   AC_CHECK_DECL([tsearch], [], [], [[#include <search.h>]])
   if test "x$ac_cv_have_decl_tsearch" = xyes ; then
      AC_CHECK_FUNCS([tsearch])
   fi
   AC_CHECK_DECL([tdelete], [], [], [[#include <search.h>]])
   if test "x$ac_cv_have_decl_tdelete" = xyes ; then
      AC_CHECK_FUNCS([tdelete])
   fi
fi

debug_mode=0
if test "x${debug}" = xyes; then
   dnl check required functions for enabling malloc tracing
   if test "x${ac_cv_func_tsearch}" = xyes &&
      test "x${ac_cv_func_tdelete}" = xyes ; then
      AC_DEFINE(GIOI_MALLOC_TRACE)
   fi
   debug_mode=1
fi
AC_SUBST([GIO_DEBUG], [$debug_mode])

if test "x$MPICC" != x ; then
   AC_CHECK_TYPE([MPI_Offset], [], [], [#include <mpi.h>])
   AC_CHECK_SIZEOF([MPI_Offset], [], [#include <mpi.h>])
   AC_CHECK_SIZEOF([MPI_Aint], [], [#include <mpi.h>])

   # check availability of MPI_Count, which is added in MPI 3.0 standard.
   # The new MPI functions added in 3.0 that have arguments of type MPI_Count:
   # MPI_Type_size_x, MPI_Type_get_extent_x, MPI_Type_get_true_extent_x, and
   # MPI_Get_elements_x
   AC_CHECK_TYPE([MPI_Count], [], [], [#include <mpi.h>])
   if test "x${ac_cv_type_MPI_Count}" != xyes; then
      AC_MSG_ERROR([
      -----------------------------------------------------------------------
      An MPI library supporting MPI_Count is required, but the supplied MPI
      compiler, $MPICC, does not support MPI_Count.
      -----------------------------------------------------------------------])
   fi
   AC_CHECK_SIZEOF([MPI_Count], [], [#include <mpi.h>])

   if test $ac_cv_sizeof_MPI_Offset != $ac_cv_sizeof_MPI_Count ; then
      AC_MSG_ERROR([
      -----------------------------------------------------------------------
      Size of data type MPI_Count ($ac_cv_sizeof_MPI_Count) must be equal to
      the size of MPI_Offset ($ac_cv_sizeof_MPI_Offset).  An MPI library
      meeting such requirement is required. Abort...
      -----------------------------------------------------------------------])
   fi

   # MPI_Count was first introduced in MPI 3.0. Check MPI functions used by
   # GIO that contain arguments of type MPI_Count. If one of the below APIs is
   # not available, have_mpi_large_count_apis will be set to no.
   have_mpi_large_count_apis=yes
   AC_CHECK_FUNCS([MPI_Isend_c \
                   MPI_Issend_c \
                   MPI_Irecv_c \
                   MPI_Alltoallv_c \
                   MPI_Alltoallw_c \
                   MPI_Type_create_hindexed_c \
                   MPI_Get_count_c \
                   MPI_Unpack_c], [], [have_mpi_large_count_apis=no])
   UD_MSG_DEBUG([have_mpi_large_count_apis=$have_mpi_large_count_apis])
   if test "x$have_mpi_large_count_apis" = "xyes" ; then
      AC_DEFINE(HAVE_MPI_LARGE_COUNT, 1)
   fi
   AM_CONDITIONAL(TEST_LARGE_COUNT, [test x$have_mpi_large_count_apis = xyes])

   AC_MSG_CHECKING([whether MPI_Waitall takes MPI_STATUSES_IGNORE])
   if test "x${GCC}" = xyes; then
      saved_CFLAGS=${CFLAGS}
      CFLAGS="-Werror -Wstringop-overflow=2"
      AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <mpi.h>]],[[
                         int count;
                         MPI_Request *reqs;
                         MPI_Waitall(count, reqs, MPI_STATUSES_IGNORE);
         ]])], [MPI_STATUSES_IGNORE=yes], [MPI_STATUSES_IGNORE=no])
      CFLAGS=${saved_CFLAGS}
   else
      AC_CHECK_DECL([MPI_STATUSES_IGNORE], [MPI_STATUSES_IGNORE=yes],
                    [MPI_STATUSES_IGNORE=no] [[#include <mpi.h>]])
   fi
   AC_MSG_RESULT([$MPI_STATUSES_IGNORE])
   if test "x$MPI_STATUSES_IGNORE" = xyes ; then
      AC_DEFINE(HAVE_MPI_STATUSES_IGNORE, 1,
                [Whether MPI_Waitall takes argument MPI_STATUSES_IGNORE])
   fi
fi

#
# Check for statfs (many) and specifically f_fstypename field (BSD)
#
AC_CHECK_HEADERS(sys/vfs.h sys/param.h sys/mount.h sys/statvfs.h sys/stat.h sys/type.h unistd.h)

AC_CHECK_FUNCS([statvfs statfs stat])

AC_CHECK_MEMBERS([struct statvfs.f_basetype,
                  struct statfs.f_fstypename,
                  struct statfs.f_type,
                  struct stat.st_fstype],[],[],
             AC_INCLUDES_DEFAULT
             [#ifdef HAVE_SYS_VFS_H
              #include <sys/vfs.h>
              #endif
              #ifdef HAVE_SYS_PARAM_H
              #include <sys/param.h>
              #endif
              #ifdef HAVE_SYS_MOUNT_H
              #include <sys/mount.h>
              #endif
              #ifdef HAVE_SYS_STATFS_H
              #include <sys/statfs.h>
              #endif
              #ifdef HAVE_SYS_STAT_H
              #include <sys/stat.h>
              #endif
              #ifdef HAVE_SYS_TYPE_H
              #include <sys/type.h>
              #endif
              #ifdef HAVE_UNISTD_H
              #include <unistd.h>
              #endif
              ])

AC_CHECK_TYPE([blksize_t],[],[AC_DEFINE_UNQUOTED([blksize_t],[__blksize_t],[Provide blksize_t if not available]) ], [[
               #ifdef HAVE_SYS_TYPES_H
               #include <sys/types.h>
               #endif
               #ifdef HAVE_SYS_STAT_H
               #include <sys/stat.h>
               #endif
               #ifdef HAVE_UNISTD_H
               #include <unistd.h>
               #endif]] )

AC_CHECK_DECLS([pwrite, pread])

#
# Check if Lustre is available by verifying presence of lustre/lustre_user.h
#
has_lustre=no
AC_CHECK_HEADERS([lustre/lustre_user.h linux/lustre/lustre_user.h],
                 [has_lustre=yes ; break])
if test "x$has_lustre" = xyes ; then
   AC_DEFINE(HAVE_LUSTRE, 1, [Define for LUSTRE])
   LIBS="$LIBS -llustreapi"
   # llapi_get_obd_count() can get the total number of available OSTs
   AC_CHECK_FUNCS([llapi_get_obd_count])
fi
AM_CONDITIONAL(HAVE_LUSTRE, [test x$has_lustre = xyes])

minicking_lustre=0
if test "x$has_lustre" = xno ; then
   AC_MSG_CHECKING([for whether mimicking Lustre])
   if test "x$MIMIC_LUSTRE" = xyes ; then
      AC_DEFINE(MIMIC_LUSTRE, 1, [Define for mimicking LUSTRE file system])
      minicking_lustre=1
      AC_MSG_RESULT(yes)
   else
      AC_MSG_RESULT(no)
   fi
fi
# AC_SUBST(MIMIC_LUSTRE, [$minicking_lustre])
AM_CONDITIONAL(MIMIC_LUSTRE, [test x$minicking_lustre = x1])

#
# Check if DAOS is available by verifying presence of DAOS header files
#
has_daos=yes
AC_CHECK_HEADERS([daos_prop.h],, [has_daos=no])
if test "x$has_daos" = xyes ; then
   AC_CHECK_HEADERS([daos_obj_class.h],, [has_daos=no])
fi
if test "x$has_daos" = xyes ; then
   AC_CHECK_HEADERS([daos_uns.h],, [has_daos=no])
fi
if test "x$has_daos" = xyes ; then
   AC_CHECK_LIB([gurt], [d_hash_table_create],, [has_daos=no])
fi
if test "x$has_daos" = xyes ; then
   AC_CHECK_LIB([uuid], [uuid_generate],, [has_daos=no])
fi
if test "x$has_daos" = xyes ; then
   AC_CHECK_LIB([daos], [daos_init],, [has_daos=no])
fi
if test "x$has_daos" = xyes ; then
   AC_CHECK_LIB([dfs], [dfs_mount],, [has_daos=no])
fi
if test "x$has_daos" = xyes ; then
   AC_CHECK_LIB([duns], [duns_resolve_path],, [has_daos=no])
fi
if test "x$has_daos" = xyes ; then
   AC_DEFINE(HAVE_DAOS, 1, [Define for DAOS])
fi
AM_CONDITIONAL(HAVE_DAOS, [test x$has_daos = xyes])

if test "x${debug}" = xyes; then
   # add -g flag if not presented
   # remove all -O and -fast flags
   # add -O0 to all flags
   # check exit status of grep command is more portable than using -q
   str_found=`echo "${CFLAGS}" | ${EGREP} -- "-g"`
   if test "x$?" != x0 ; then
      CFLAGS="$CFLAGS -g"
   fi

   # remove -fast if set by user
   CFLAGS=`echo $CFLAGS | ${SED} 's/-fast *//g'`

   # check if -O is set by user, if not, then add -O0
   str_found=`echo "${CFLAGS}" | ${EGREP} -- "-O"`
   if test "x$str_found" = x ; then
      CFLAGS="$CFLAGS -O0"
   fi
   unset str_found
fi

chmod u+x ${srcdir}/scripts/install-sh

AC_ARG_ENABLE([thread-safe],
    [AS_HELP_STRING([--enable-thread-safe],
                    [Enable thread-safe capability. @<:@default: disabled@:>@])],
    [thread_safe=${enableval}], # --enable-thread-safe or --disable-thread-safe is used
    [thread_safe=no]            # neither --enable-thread-safe nor --disable-thread-safe is used
)

enable_thread_safe=0
if test "x$MPICC" != x && test "x${thread_safe}" = xyes ; then
   if test "x${ax_cv_mpi_compiler_vendor}" = xMPICH ; then
      AX_COMPARE_VERSION([${ax_cv_mpi_compiler_version}],[ge],[4.0.3])
      if test "$ax_compare_version" = "false" ; then
         AC_MSG_ERROR([
    -----------------------------------------------------------------------
      Thread-safety feature, --enable-thread-safe, requires MPICH version
      4.0.3 and later. The version of provided MPICH is $ax_cv_mpi_compiler_version.
      See bug fix in https://github.com/pmodels/mpich/pull/5954
      Abort.
    -----------------------------------------------------------------------])
      fi
   fi
   enable_thread_safe=1
fi
AC_SUBST([ENABLE_THREAD_SAFE], [$enable_thread_safe])

AC_ARG_WITH([pthread],
   [AS_HELP_STRING([--with-pthread=DIR],
                   [Search Pthreads library within the supplied path DIR,
                    when --enable-thread-safe is enabled.])],
   [ dnl this clause is run when --with-pthread or --without-pthread is used
   if test "x$withval" = xno && test "x$thread_safe" = xyes ; then
      AC_MSG_ERROR([conflicted options used: --enable-thread-safe and --without-pthread])
   elif test "x$withval" = xyes ; then
      pthread_dir=check
   else
      pthread_dir=$withval
   fi
   ],[ dnl this clause is run when neither --with-pthread nor --without-pthread is used
   pthread_dir=check
   ]
)

if test "x${thread_safe}" = xyes ; then
   if test "x${pthread_dir}" = xcheck ; then
      AC_CHECK_HEADER([pthread.h],,[AC_MSG_ERROR([Cannot find pthread.h])])
      AC_CHECK_LIB([pthread], [pthread_self],, [AC_MSG_ERROR([Cannot find pthread library])])
   else
      case "$pthread_dir" in
         *,*)
            pthread_inc="`echo $pthread_dir | cut -f1 -d,`"
            pthread_lib="`echo $pthread_dir | cut -f2 -d, -s`"
            ;;
         *)
            # check $pthread_dir for empty is done above
            pthread_inc="$pthread_dir/include"
            pthread_lib="$pthread_dir/lib"
            ;;
      esac
      CPPFLAGS="$CPPFLAGS -I$pthread_inc"
      AC_CHECK_HEADER([pthread.h],, [AC_MSG_ERROR([Cannot find pthread.h under $pthread_dir])])
      LDFLAGS="$LDFLAGS -L$pthread_lib"
      AC_CHECK_LIB([pthread], [pthread_self],, [AC_MSG_ERROR([Cannot find pthread library under $pthread_dir])])
   fi
fi

AC_ARG_ENABLE([profiling],
   [AS_HELP_STRING([--enable-profiling],
                   [Enable time and memory profiling. @<:@default: disabled@:>@])],
   [enable_profiling=${enableval}], [enable_profiling=no]
)

profiling_mode=0
if test "x$enable_profiling" = "xyes" ; then
   profiling_mode=1

   if test "x${debug}" = xno; then
      dnl check required functions for enabling malloc tracing
      if test "x${ac_cv_func_tsearch}" = xyes && test "x${ac_cv_func_tdelete}" = xyes ; then
         AC_DEFINE(GIOI_MALLOC_TRACE)
      fi
   fi
fi
AC_SUBST([GIO_PROFILING], [$profiling_mode])

dnl build test programs and benchmark programs
AM_CONDITIONAL(BUILD_TESTSETS, [true])

AC_ARG_VAR(TESTSEQRUN, [Run command (on one MPI process) for "make check" on cross-compile environment. Example: "aprun -n 1". @<:@default: none@:>@])

AC_ARG_VAR(TESTMPIRUN, [MPI run command for "make ptest", @<:@default: mpiexec -n NP@:>@])
if test "x${TESTMPIRUN}" = x ; then
   dnl if TESTMPIRUN has not been set by users, then
   dnl set default to "mpiexec -n NP"
   UD_MPI_PATH_PROGS([TESTMPIRUN], [mpiexec mpirun srun])
   if test "x${TESTMPIRUN}" != x ; then
      if test "x${ax_cv_mpi_compiler_vendor}" = xOpenMPI ; then
         TESTMPIRUN="$TESTMPIRUN --oversubscribe"
      fi
      TESTMPIRUN="$TESTMPIRUN -n NP"
   fi
fi

AC_ARG_VAR(TESTOUTDIR, [Output file directory for "make check" and "make ptest", @<:@default: ./@:>@])
if test "x${TESTOUTDIR}" = x ; then
   dnl set default to current directory
   TESTOUTDIR=.
else
   FSTYPE_PREFIX=`echo ${TESTOUTDIR} | cut -d: -f1`
   if test $FSTYPE_PREFIX = $TESTOUTDIR ; then
      # no name prefix end with ':'
      FSTYPE_PREFIX=
   else
      # check if name prefix is one of file system types known to GIO
      known_fstypes=(ufs nfs xfs pvfs2 gpfs panfs lustre daos testfs ime quobyte)
      known_fstype=
      for pre in $known_fstypes ; do
          if test "$FSTYPE_PREFIX" = $pre ; then
             known_fstype=$pre
             break
          fi
      done
      if test "x$known_fstype" = x ; then
         FSTYPE_PREFIX=
      else
         FSTYPE_PREFIX+=":"
         # remove name prefix from test output directory name
         TESTOUTDIR=${TESTOUTDIR#"$FSTYPE_PREFIX"}
      fi
   fi
fi
AC_SUBST([FSTYPE_PREFIX])
AC_DEFINE_UNQUOTED(TESTOUTDIR, ["$TESTOUTDIR"], [Output directory for tests])

# SEQ_CC is used to compile programs to be run sequentially, such as
# gio_version. Below we use the same C compiler as MPICC.
AC_ARG_VAR(SEQ_CC, [C compiler for building sequential utility programs, @<:@default: gcc@:>@])
if test "x$SEQ_CC" = x ; then
   if test "x$ac_cv_mpi_compiler_base_MPICC" = x ; then
      # gcc should be available on most of the machines today
      AC_PATH_PROG([SEQ_CC], [gcc], [$MPICC])
   else
      SEQ_CC=$ac_cv_mpi_compiler_base_MPICC
   fi
fi
AC_MSG_CHECKING([C compiler for serial utility programs])
AC_MSG_RESULT([$SEQ_CC])
AC_SUBST(SEQ_CC)

AC_ARG_VAR(SEQ_CFLAGS, [C compile flags for building sequential utility programs, @<:@default: none@:>@])
AC_SUBST(SEQ_CFLAGS)
AC_ARG_VAR(SEQ_LDFLAGS, [Linker flags for building sequential utility programs, @<:@default: none@:>@])
AC_SUBST(SEQ_LIBS)
AC_ARG_VAR(SEQ_LIBS, [Libraries for building sequential utility programs, @<:@default: none@:>@])
AC_SUBST(SEQ_LIBS)

# Configuration Date
# Note that command 'date' is not portable across Unix platforms
if test "x$SOURCE_DATE_EPOCH" != x ; then
    date_str=`date -d @${SOURCE_DATE_EPOCH}`
    if test "x$date_str" != x ; then
       AC_SUBST([CONFIG_DATE], ["$date_str"])
    else
       date_str=`date -r $SOURCE_DATE_EPOCH`
       if test "x$date_str" != x ; then
          AC_SUBST([CONFIG_DATE], ["$date_str"])
       else dnl ignore SOURCE_DATE_EPOCH
          AC_SUBST([CONFIG_DATE], ["`date`"])
       fi
    fi
else
    AC_SUBST([CONFIG_DATE], ["`date`"])
fi
UD_MSG_DEBUG([CONFIG_DATE=$CONFIG_DATE])

# Enable creation of libtool-style versioning or no versioning
AC_ARG_ENABLE(versioning,
        [AS_HELP_STRING([--disable-versioning],[Disable library versioning. @<:@default: enabled@:>@])],
        [enable_versioning=${enableval}], [enable_versioning=yes]
)
# For libtool ABI versioning rules see:
# http://www.gnu.org/software/libtool/manual/libtool.html#Updating-version-info
# Update the version information only immediately before a public release.
ABIVERSION="0:0:0"
AC_SUBST(ABIVERSION)
if test "$enable_versioning" = "yes" ; then
   ABIVERSIONFLAGS="-version-info \$(ABIVERSION)"
else
   ABIVERSIONFLAGS="-avoid-version"
fi
AC_SUBST(ABIVERSIONFLAGS)

# Check if GIO is being built as a subproject
AM_CONDITIONAL(BUILD_AS_SUBPROJECT, [test "x$GIO_SUB_BUILD" = xyes])

AC_CONFIG_FILES(Makefile \
                src/Makefile \
                src/gio.h \
                utils/Makefile \
                utils/gio-config \
                utils/packaging/Makefile \
                utils/packaging/gio.pc \
                test/Makefile \
                examples/Makefile \
)

AC_SUBST(enable_static)
AC_SUBST(enable_shared)
AM_CONDITIONAL(BUILD_SHARED_LIB, [test x${enable_shared} = xyes])

AC_OUTPUT
if test "x$GIO_SUB_BUILD" != xyes ; then
echo "------------------------------------------------------------------------------"
echo \
"
   ${PACKAGE_NAME} Version ${PACKAGE_VERSION}

   Features:  Parallel feature (MPI)                      - ${mpi_mode}
              Build static libraries                      - ${enable_static}
              Build shared libraries                      - ${enable_shared}"
if test "x${thread_safe}" = xyes ; then
   echo "\
              Thread-safe capability                      - enabled"
fi
if test "x${debug}" = xyes; then
   echo "\
              Internal debug mode                         - enabled"
fi
if test "x${enable_profiling}" = xyes ; then
   echo "\
              Internal profiling mode                     - enabled"
fi
echo ""
if test "x$MPICC" != x ; then
   echo "\
   Compilers: MPICC       = ${MPICC}"
else
echo "\
   Compilers: CC          = ${CC}"
fi
if test "x${CPPFLAGS}" != x ; then
   echo "\
              CPPFLAGS    = ${CPPFLAGS}"
fi
echo "\
              CFLAGS      = ${CFLAGS}"
if test "x${LDFLAGS}" != x ; then
   echo "\
              LDFLAGS     = ${LDFLAGS}"
fi
if test "x${LIBS}" != x ; then
   echo "\
              LIBS        = ${LIBS}"
fi
if test "x${SEQ_CC}" != x ; then
   echo "\
              SEQ_CC      = ${SEQ_CC}"
fi
if test "x${SEQ_CFLAGS}" != x ; then
   echo "\
              SEQ_CFLAGS  = ${SEQ_CFLAGS}"
fi
if test "x${SEQ_LDFLAGS}" != x ; then
   echo "\
              SEQ_LDFLAGS = ${SEQ_LDFLAGS}"
fi
if test "x${SEQ_LIBS}" != x ; then
   echo "\
              SEQ_LIBS    = ${SEQ_LIBS}"
fi
echo "\

   Now run 'make' to build the library and utility tools.
   Then run 'make @<:@<target>@:>@' for testing and installation, where the
   optional <target> can be:
              tests    - build all test programs (build only, no run)
              check    - run sequential test programs
              ptest    - run parallel test programs on 4 MPI processes
              ptests   - run parallel test programs on 3,4,6,8 MPI processes
              install  - install GIO library in ${prefix}

------------------------------------------------------------------------------"
fi
