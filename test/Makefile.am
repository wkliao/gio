#
# Copyright (C) 2026, Northwestern University
# See COPYRIGHT notice in top-level directory.
#
# @configure_input@

SUFFIXES = .o .c

AM_DEFAULT_SOURCE_EXT = .c

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/src -I$(top_builddir)/src

LDADD = $(top_builddir)/src/libgio.la libtestutils.la

check_PROGRAMS = tst_ghost_cell

check_LTLIBRARIES = libtestutils.la
libtestutils_la_LIBADD = $(top_builddir)/src/libgio.la
libtestutils_la_SOURCES = tst_utils.c tst_utils.h

TESTS_ENVIRONMENT = export SED="$(SED)"; \
		 			export srcdir="$(srcdir)"; \
		 			export TESTOUTDIR="$(FSTYPE_PREFIX)$(TESTOUTDIR)"; \
		 			export TESTSEQRUN="$(TESTSEQRUN)"; \
		 			export TESTMPIRUN="$(TESTMPIRUN)"; \
		 			export check_PROGRAMS="$(check_PROGRAMS)"; \
					export MIMIC_LUSTRE=@MIMIC_LUSTRE@;

TESTS = $(check_PROGRAMS)
TEST_EXTENSIONS = .sh
LOG_COMPILER = $(srcdir)/seq_runs.sh
SH_LOG_COMPILER =

check_SCRIPTS = seq_runs.sh parallel_run.sh

EXTRA_DIST = seq_runs.sh parallel_run.sh

CLEANFILES = core core.* *.gcda *.gcno *.gcov gmon.out

ptest ptests ptest4: $(check_PROGRAMS)
	@echo "==========================================================="
	@echo "    $(subdir): Parallel testing on 4 MPI processes"
	@echo "==========================================================="
	@$(TESTS_ENVIRONMENT) \
	$(srcdir)/parallel_run.sh 4 || exit 1

ptest2 ptest6 ptest8 ptest10:

# build check targets but not invoke
tests-local: all $(check_PROGRAMS)

.PHONY: ptest ptests ptest2 ptest4 ptest6 ptest8 ptest10

