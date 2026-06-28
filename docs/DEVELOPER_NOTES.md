## Notes for GIO developers
---

### Tasks immediately before a new release (must run in the following order)

1. Set the release version. See section "Convention of setting version numbers"
   below for instructions on how to set the numbers.
   ```
   AC_INIT([GIO], [1.0.0], [], [gio], [https://github.com/wkliao/gio])
                   ^^^^^
   ```

2. Update file `RELEASE_NOTES.md`
   Move the contents of file `sneak_peek.md` to file `RELEASE_NOTES.md`.
   Clear up file `sneak_peek.md` (reset all items to none).

3. Commit all changes to repo at Github.
   ```
   git commit -m "prepare release of 1.0.0"
   git push origin main
   ```

4. Create a new tag
   * Run command below to duplicate the current master branch to a new tag:
     ```
     git tag -a v1.0.0 -m "version 1.0.0"
     git push origin v1.0.0
     ```
   * FYI. When needed, one can `force` update a tag (local and remote):
     ```
     git tag -fa v1.0.0 -m "version 1.0.0"
     git push -f --tags
     ```

5. Generate release tar ball
   ```
   autoreconf -i
   ./configure --silent
   make dist
   ```
   * A newly created tar ball file will have a name like `gio-1.0.0.tar.gz`.
   * Note command `make dist` will automatically set the current date as the
     software release date into several source files in the tar ball, such as
     `utils/gio_version.c`, `src/gio_lib_version.c`. This effect is because of
     the automake's feature for setting `dist-hook`, which cannot be achieved
     when building from a clone of GIO repo.

6. Generate SHA1 checksums. It is like a unique ID for a file.
   * Run command:
     ```
     openssl sha1 gio-1.0.0.tar.gz
     ```
   * Example command-line output:
     ```
     SHA1(gio-1.0.0.tar.gz)= 495d42f0a41abbd09d276262dce0f7c1c535968a
     ```
   * Or use SHA 256
     ```
     sha256sum gio-1.0.0.tar.gz
     8a039a810caffad9bd1e7ad82572eef4d02e3abd6907a8b66ce5953af78627ce gio-1.0.0.tar.gz
     ```
   * The above commands can also be used to verify the SHA numbers.

7. Upload the new tar ball to GIO Github wiki page repo.
   * GIO's github wiki pages are stored in a different repo:
     git@github.com:wkliao/gio.wiki.git
   * Clone that repo if have not done already.
   * Go to that repo and make changes there.
   * Add a new row to file 'GIO-software-release-tarball-files.md' and fill
     in the release date, tarball size, SHA1 number, and reference to the
     tarball file.
   * Add the tarball file, commit changes, and push to Github
     ```
     git clone git@github.com:wkliao/gio.wiki.git
     cd gio.wiki
     edit file GIO-software-release-tarball-files.md
     cp gio-1.0.0.tar.gz releases/
     git add releases/gio-1.0.0.tar.gz
     git commit -m "Add release 1.0.0"
     git push origin master
     ```
   * FYI note: deleting a Github wiki page must use the "Edit view" of that
     specific page.
     + Click on the Wiki tab
     + Select the page
     + Click the Edit button
     + Click the red Delete Page

8. Update file README.md of the GIO repo (not gio.wiki repo):
   * Update the latest release's URL pointing to its wiki page.

---
### Convention of setting version numbers
* For software release versioning
  * See http://semver.org/
    * Given a version number MAJOR.MINOR.PATCH, increment the:
      1. MAJOR version when you make incompatible API changes,
      2. MINOR version when you add functionality in a backwards-compatible
         manner, and
      3. PATCH version when you make backwards-compatible bug fixes.
    * Additional labels for pre-release and build metadata are available as
      extensions to the MAJOR.MINOR.PATCH format.

* For shared library versioning
  1. For libtool ABI versioning rules see:
     http://www.gnu.org/software/libtool/manual/libtool.html#Updating-version-info
  2. Update the version information only immediately before a public release.
  3. In configure.ac, change/set variable ABIVERSION to the new version.
  ```
   Here are a set of rules to help you update your library version information:
   1. Start with version information of '0:0:0' for each libtool library.
   2. Update the version information only immediately before a public release
      of your software. More frequent updates are unnecessary, and only
      guarantee that the current interface number gets larger faster.
   3. If the library source code has changed at all since the last update, then
      increment revision ('c:r:a' becomes 'c:r+1:a').
   4. If any interfaces have been added, removed, or changed since the last
      update, increment current, and set revision to 0.
   5. If any interfaces have been added since the last public release, then
      increment age.
   6. If any interfaces have been removed or changed since the last public
      release, then set age to 0.

   libtool Chapter 7.1 What are library interfaces?
      Interfaces for libraries may be any of the following (and more):
      global variables: both names and types
      global functions: argument types and number, return types, and function
      names standard input, standard output, standard error, and file formats
      sockets, pipes, and other inter-process communication protocol formats

      The following explanation may help to understand the above rules a bit
      better: consider that there are three possible kinds of reactions from
      users of your library to changes in a shared library:
      1. Programs using the previous version may use the new version as drop-in
         replacement, and programs using the new version can also work with the
         previous one. In other words, no recompiling nor relinking is needed.
         In this case, bump revision only, don't touch current nor age.
      2. Programs using the previous version may use the new version as drop-in
         replacement, but programs using the new version may use APIs not
         present in the previous one. In other words, a program linking against
         the new version may fail with 'unresolved symbols' if linking against
         the old version at runtime: set revision to 0, bump current and age.
      3. Programs may need to be changed, recompiled, and relinked in order to
         use the new version. Bump current, set revision and age to 0.

   libtool Chapter 7.2 Libtool’s versioning system
      So, libtool library versions are described by three integers:

      current
          The most recent interface number that this library implements.

      revision
          The implementation number of the current interface.

      age
          The difference between the newest and oldest interfaces that this
          library implements. In other words, the library implements all the
          interface numbers in the range from number current - age to current.

      If two libraries have identical current and age numbers, then the dynamic
      linker chooses the library with the greater revision number.
  ```
---
### Note on autotools version used for software development
When seeing the error message below when running command 'autoreconf -i',
delete file 'm4/ltversion.m4' first. This is because ltversion.m4 is residue
from a previous build using an earlier version of libtool.

---
### Working on configure.in or configure.ac
Debugging: add "gio_ac_debug=yes" to the configure command line to print
debugging messages on screen.

---
### Note on adding a new configure command-line option
* `configire.ac` -- The AC_OUTPUT section, add "enabled" on screen if the
  new feature is enabled. No output when disabled. For example,
  ```
     if test "x${enable_profiling}" = xyes; then
        echo "\
                    Internal profiling mode                     - enabled"
     fi
  ```
* `src/utils/gio-config.in` -- add similar outputs as in `configure.ac`.
  Note unlike `configure.ac`, in `gio-config.in` the new feature must show
  either "enabled" or "disabled".
* `src/gio.h.in` -- essential configure-time options are now explicitly set
  in the public header file. For example,
  ```
  #define GIO_PROFILING_MODE @GIO_PROFILING@
  ```

---
### Note on creating new GIO APIs
If a new GIO public API is created, add the declaration of new APIs in file
`src/gio.h.in` and prepend `GIO_PUBLIC_API` to it. For example,
```
   GIO_PUBLIC_API const char* GIO_inq_libvers(void);
```

Without `GIO_PUBLIC_API` prepended to an API's declaration, all subroutines
will be considered private, i.e. not visible to external user applications.

---
### Note on debugging
Enable debugging option (--enable-debug) at the configure time can trace the
usage of malloc and whether there is a malloc residue. All GIO internal
subroutines should call GIOI_Malloc, GIOI_Calloc, GIOI_Realloc, and GIOI_Free,
instead of malloc, calloc, realloc, and free. When adding a new test or example
program, please add a check for any malloc residue at the end. This is to make
sure GIO properly free up all malloc used internally. The code fragment is
something like below.
```
   /* check if there is any GIO internal malloc residue */
   MPI_Offset malloc_size, sum_size;
   int err = gio_inq_malloc_size(&malloc_size);
   if (err == GIO_NOERR) {
       MPI_Reduce(&malloc_size, &sum_size, 1, MPI_OFFSET, MPI_SUM, 0, MPI_COMM_WORLD);
       if (rank == 0 && sum_size > 0)
           printf("heap memory allocated by GIO internally has %lld bytes yet to be freed\n",
                  sum_size);
   }
```

---
### Note on adding a new error code
GIO error codes are all negative integral numbers, except `GIO_NOERR` which is
0. All error codes must be defined in the public header file `src/gio.h.in`.

---
### Testing with valgrind
Valgrind checks memory leak (e.g. command: valgrind --leak-check=full -q), and
should be used to test from time to time. Must ensure there is no memory leak
before an official release.

---
### Note on including config.h
* When doing VPATH build, remember to pass the C compiler a -I. option. Even if
  you use #include "config.h", the preprocessor searches only the source
  directory, not the build directory. Thus, we should use #include <config.h>
  instead of #include "config.h". In addition, use -I. -I$(srcdir) in
  Makefile.in.

* Autoconf manual suggests it is a good habit to use angle brackets, because in
  the rare case when the source directory contains another config.h, the build
  directory should be searched first.

---
### Note on M4 flags
* M4 has a nice feature called synclines that adds line numbers into m4 files
  so compilers can report the error locations in the m4 files, instead of the
  derived C/Fortran files. To enable this feature, developers add M4FLAGS=-s to
  the configure command line. Note that synclines currently only take effect for
  C files. There is still some issues needed to be resolved for Fortran files.
  Developers are also warned that when m4 macro functions are used, the line
  numbers reported are the locations the functions are invoked, not the lines
  inside the functions.

