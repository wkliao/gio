#
# Copyright (C) 2026, Northwestern University
# See COPYRIGHT notice in top-level directory.
#
# @configure_input@

SUBDIRS = packaging

if BUILD_AS_SUBPROJECT
   noinst_SCRIPTS = gio-config
   noinst_PROGRAMS = gio_version
else
   # The script shows the end users how GIO is built
   bin_SCRIPTS = gio-config
   bin_PROGRAMS = gio_version
   dist_man_MANS = gio_version.1
endif

XDEFS = -DMPICC="\"$(MPICC)\""   -DCFLAGS="\"$(CFLAGS)\""
XDEFS += -DCONFIGURE_ARGS_CLEAN="\"$(CONFIGURE_ARGS_CLEAN)\""
XDEFS += -DGIO_VERSION="\"$(GIO_VERSION)\""
XDEFS += -DMPI_VERSION="\"@MPI_VERSION@\""
XDEFS += -DMPI_VENDOR="\"@MPI_VENDOR@\""
XDEFS += -DMPI_VENDOR_VERSION="\"@MPI_VENDOR_VERSION@\""
XDEFS += -DMPICC_BASE="\"@MPICC_BASE@\"" -DMPICC_BASE_VERSION="\"@MPICC_BASE_VERSION@\""

if HAVE_LMOD_ENV
   XDEFS += -DLMOD_VERSION="\"@LMOD_VERSION@\""
   XDEFS += -DLMOD_FAMILY_PRGENV="\"@LMOD_FAMILY_PRGENV@\""
   XDEFS += -DLMOD_FAMILY_PRGENV_VERSION="\"@LMOD_FAMILY_PRGENV_VERSION@\""
   XDEFS += -DLMOD_FAMILY_COMPILER="\"@LMOD_FAMILY_COMPILER@\""
   XDEFS += -DLMOD_FAMILY_COMPILER_VERSION="\"@LMOD_FAMILY_COMPILER_VERSION@\""
   XDEFS += -DLMOD_FAMILY_MPI="\"@LMOD_FAMILY_MPI@\""
   XDEFS += -DLMOD_FAMILY_MPI_VERSION="\"@LMOD_FAMILY_MPI_VERSION@\""
endif

gio_version$(EXEEXT): gio_version.c
	$(SEQ_CC) $(SEQ_CFLAGS) $(XDEFS) -o $@ $< $(SEQ_LDFLAGS) $(SEQ_LIBS)

tests-local: all

dist-hook:
if ! BUILD_AS_SUBPROJECT
	$(SED_I) -e "s|GIO_RELEASE_DATE|@GIO_RELEASE_DATE@|g" $(distdir)/gio-config.in
	$(SED_I) -e "s|GIO_RELEASE_DATE|@GIO_RELEASE_DATE@|g"           $(distdir)/gio_version.c
	$(SED_I) -e "s|GIO_RELEASE_VERSION|$(GIO_VERSION)|g"            $(distdir)/gio_version.1
	$(SED_I) -e "s|GIO_RELEASE_DATE_FULL|@GIO_RELEASE_DATE_FULL@|g" $(distdir)/gio_version.1
	$(SED_I) -e "s|GIO_RELEASE_DATE|@GIO_RELEASE_DATE@|g"           $(distdir)/gio_version.1
endif

# GNU autoconf allows DESTDIR variable when running 'make install' which will
# prepend it before all installation names.
install-exec-hook:
if ! BUILD_AS_SUBPROJECT
	$(SED_I) -e 's|INSTALL_PREFIX|$(prefix)|g ; s|INSTALL_EXEC_PREFIX|$(exec_prefix)|g' $(DESTDIR)$(bindir)/gio-config ; \
	chmod +x $(DESTDIR)$(bindir)/gio-config
endif

# For VPATH build (parallel build), try delete all sub-directories
distclean-local:
	@for d in $(SUBDIRS) ; do \
        if [ "$$d" = "." ] ; then continue ; fi ; \
        if [ -d $$d/.deps ] ; then rmdir $$d/.deps || true ; fi ; \
    done ; \
    if [ "$(abs_builddir)" != "$(abs_srcdir)" ] ; then \
       for d in $(SUBDIRS) ; do \
           if [ "$$d" = "." ] ; then \
              continue ; \
           fi ; \
           rmdir $$d || true ; \
       done ; \
    fi

