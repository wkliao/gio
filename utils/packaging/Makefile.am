#
# Copyright (C) 2026, Northwestern University
# See COPYRIGHT notice in top-level directory.
#
# @configure_input@

if ! GIO_BUILT_AS_SUBPROJECT
   pkgconfig_DATA = gio.pc
endif

pkgconfigdir = $(libdir)/pkgconfig

# GNU autoconf allows DESTDIR variable when running 'make install' which will
# prepend it before all installation names.
install-data-hook:
if ! GIO_BUILT_AS_SUBPROJECT
	$(SED_I) -e 's|INSTALL_PREFIX|$(prefix)|g ; s|INSTALL_EXEC_PREFIX|$(exec_prefix)|g' $(DESTDIR)$(libdir)/pkgconfig/gio.pc
endif

