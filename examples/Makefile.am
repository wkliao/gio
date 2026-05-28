#
# Copyright (C) 2026, Northwestern University
# See COPYRIGHT notice in top-level directory.
#
# @configure_input@

SUFFIXES = .o .c

AM_DEFAULT_SOURCE_EXT = .c

AM_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/src -I$(top_builddir)/src

LDADD = $(top_builddir)/src/libgio.la

check_PROGRAMS = ex_write_coll \
				 ex_write_indep \
				 ex_read_coll \
				 ex_read_indep

TESTS_ENVIRONMENT = export SED="$(SED)"; \
		 			export srcdir="$(srcdir)"; \
		 			export TESTOUTDIR="$(FSTYPE_PREFIX)$(TESTOUTDIR)"; \
		 			export TESTSEQRUN="$(TESTSEQRUN)"; \
		 			export TESTMPIRUN="$(TESTMPIRUN)"; \
		 			export check_PROGRAMS="$(check_PROGRAMS)"; \
					export MIMIC_LUSTRE=@MIMIC_LUSTRE@;

# skip 'make check', enabled only when 'make ptest'
# TESTS = $(check_PROGRAMS)

check_SCRIPTS = parallel_run.sh

EXTRA_DIST = parallel_run.sh

CLEANFILES = *.dat core core.* *.gcda *.gcno *.gcov gmon.out

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

