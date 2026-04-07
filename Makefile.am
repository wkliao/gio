#
# Copyright (C) 2026, Northwestern University
# See COPYRIGHT notice in top-level directory.
#
# @configure_input@

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = src
DIST_SUBDIRS = src

EXTRA_DIST = INSTALL.md \
             README.md \
             RELEASE_NOTES.md

ptest:
	@for d in $(PTEST_SUBDIRS) ; do \
		$(MAKE) $(MFLAGS) -C $$d ptest $$* || exit 1 ; \
	done

# Note on DESTDIR. GNU autoconf manual Section 16.4 Installation Names has the
# following statement. "The second method involves providing the 'DESTDIR'
# variable. For example, 'make install DESTDIR=/alternate/directory' will
# prepend '/alternate/directory' before all installation names. The approach of
# 'DESTDIR' overrides is not required by the GNU Coding Standards, and does not
# work on platforms that have drive letters. On the other hand, it does better
# at avoiding recompilation issues, and works well even when some directory
# options were not specified in terms of '${prefix}' at configure time."
# Note command "make install prefix=/path/of/install DESTDIR=/path/to/dest"
# does not install the library. It copies all install files into folder
# "$DESTDIR/$prefix", so one can cd to $DESTDIR and pack the folder $prefix
# there into a tar ball.
install-data-hook:
	@echo '+----------------------------------------------------------------------------+'
	@echo '|'
	@echo '|  GIO has been successfully installed under'
	@echo '|          $(prefix)'
	@echo '|'
	@echo '|  * GIO header files have been installed in'
	@echo '|          $(prefix)/include'
	@echo '|  * GIO library files have been installed in'
	@echo '|          $(exec_prefix)/lib'
	@echo '|  * GIO utility programs have been installed in'
	@echo '|          $(exec_prefix)/bin'
	@echo '|  * GIO man pages have been installed in'
	@echo '|          $(prefix)/share/man'
	@echo '|'
	@echo '|  To compile your GIO programs, please add the following to the command'
	@echo '|  line, so the compiler can find the GIO header files:'
	@echo '|      -I$(prefix)/include'
	@echo '|'
	@echo '|  Add the following line to link your program to GIO library:'
	@echo '|      -L$(exec_prefix)/lib -lGIO'
	@echo '|'
	@if test "x$(enable_shared)" = xyes ; then \
	 echo '|  Add the following to your run-time environment variable LD_LIBRARY_PATH,' ; \
	 echo '|  when linking your executable with the GIO shared libraries.' ; \
	 echo '|      $(exec_prefix)/lib' ; \
	 echo '|' ; \
	fi
	@echo '|'
	@echo '|  GIO is developed at Northwestern University'
	@echo '|'
	@echo '|  Visit GIO project web site for more information'
	@echo '|      https://github.com/wkliao/gio'
	@echo '|'
	@echo '+----------------------------------------------------------------------------+'

# For VPATH build (parallel build), try delete all sub-directories
distclean-local:
	@if [ "$(abs_builddir)" != "$(abs_srcdir)" ] ; then \
	   for d in $(DIST_SUBDIRS) ; do \
	       rmdir $$d || true ; \
	   done ; \
	fi

.PHONY: ptest ptests tests

dist-hook:
	$(SED_I) -e "1,10s|_GIO_RELEASE_DATE_|@GIO_RELEASE_DATE@|" $(distdir)/RELEASE_NOTES.md
	$(SED_I) -e "1,10s|_GIO_VERSION_|$(GIO_VERSION)|" $(distdir)/RELEASE_NOTES.md
	$(SED_I) -e "s|_GIO_VERSION_|$(GIO_VERSION)|" $(distdir)/INSTALL.md


