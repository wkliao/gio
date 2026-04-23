#
# Copyright (C) 2026, Northwestern University
# See COPYRIGHT notice in top-level directory.
#
# @configure_input@

bin_PROGRAMS = gio_version

XDEFS = -DMPICC="\"$(MPICC)\""   -DCFLAGS="\"$(CFLAGS)\""
XDEFS += -DCONFIGURE_ARGS_CLEAN="\"$(CONFIGURE_ARGS_CLEAN)\""
XDEFS += -DGIO_VERSION="\"$(GIO_VERSION)\""

gio_version$(EXEEXT): gio_version.c
	$(SEQ_CC) $(SEQ_CFLAGS) $(XDEFS) -o $@ $< $(SEQ_LDFLAGS) $(SEQ_LIBS)

dist_man_MANS = gio_version.1

tests-local: all

dist-hook:
	$(SED_I) -e "s|GIO_RELEASE_DATE|@GIO_RELEASE_DATE@|g"           $(distdir)/gio_version.c
	$(SED_I) -e "s|GIO_RELEASE_VERSION|$(GIO_VERSION)|g"            $(distdir)/gio_version.1
	$(SED_I) -e "s|GIO_RELEASE_DATE_FULL|@GIO_RELEASE_DATE_FULL@|g" $(distdir)/gio_version.1
	$(SED_I) -e "s|GIO_RELEASE_DATE|@GIO_RELEASE_DATE@|g"           $(distdir)/gio_version.1

