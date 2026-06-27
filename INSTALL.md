## GIO Installation Guide

* [Getting Started](#getting-started)
* [Alternate Configure Options](#alternate-configure-options)
* [Testing the GIO Installation](#testing-the-gio-installation)
* [Reporting Installation or Usage Problems](#reporting-installation-or-usage-problems)

---

### Getting Started

The following instructions take you through a sequence of steps to get the
default configuration of GIO up and running.

* You will need the following prerequisites.
  - REQUIRED: This compressed tar file, e.g. gio-1.0.0.tar.gz
  - REQUIRED: An MPI C compiler, e.g. [MPICH](https://www.mpich.org) and
    [OpenMPI](https://www.open-mpi.org)
  - [autoconfig](https://www.gnu.org/software/autoconf) version 2.71
  - [automake](https://www.gnu.org/software/automake) version 1.16.5
  - [libtool](https://www.gnu.org/software/libtool) version 2.5.4
  - [m4](https://www.gnu.org/software/m4) version 1.4.17
  - Also, you need to know what shell you are using since different shell has
    different command syntax. Command "echo $SHELL" prints out the current
    shell used by your terminal program.

* Unpack the tar file and go to the top level directory, for example:
  ```
  gzip -dc gio-1.0.0.tar.gz | tar -xf -
  cd gio-1.0.0
  ```

* Generate GNU build system files.
  ```
  autoreconf -i
  ```

* Choose an installation directory, for example $HOME/GIO/1.0.0, and configure
  GIO by specifying the installation directory:
  ```
  ./configure --prefix=$HOME/GIO/1.0.0
  ```
  + To use an MPI C compiler installed in a non-default location, add and
    set the environment variable MPICC in the same command line, e.g.
    ```
    ./configure --prefix=$HOME/GIO/1.0.0 MPICC=/path/to/mpicc
    ```

* Build GIO:
  ```
  make
  ```
  Or if "make" runs slow, try parallel make, e.g. (using 8 simultaneous jobs)
  ```
  make -j8
  ```
  For quiet mode, use the command below will produce minimum output on screen.
  ```
  make -s LIBTOOLFLAGS=--silent V=1 -j8
  ```

* Install GIO
  ```
  make install
  ```
  If an install directory different from the one set as `prefix` at the
  configure time is desired, use command:
  ```
  make install prefix=/OTHER/INSTALL/DIRECTORY
  ```

* Add the bin subdirectory of the installation directory to your path
  environment variable in your startup script (.bashrc for bash, .cshrc for
  csh, etc.):

  + for csh and tcsh:
    ```
    setenv PATH $HOME/GIO/1.0.0/bin:$PATH
    ```
  * for bash and sh:
    ```
    export PATH=$HOME/GIO/1.0.0/bin:$PATH
    ```
  * Check that everything is in order at this point by doing:
    ```
    which gio_version
    ```
    This above commands should display the path to your bin subdirectory of
    your install directory. An example output on screen is:
    ```
    $HOME/GIO/1.0.0/bin/gio_version
    ```
    If you have completed all of the above steps, you have successfully
    installed GIO.

---

### Alternate Configure Options

GIO has a number of configure features.  A complete list of configuration
options can be found using:
```
   ./configure --help
```
Here lists a few important options:
```
  --prefix=PREFIX         install architecture-independent files in PREFIX
                          [/usr/local]
  --exec-prefix=EPREFIX   install architecture-dependent files in EPREFIX
                          [PREFIX]
  --enable-shared[=PKGS]  build shared libraries [default=no]
  --enable-static[=PKGS]  build static libraries [default=yes]
  --enable-debug          Enable GIO internal debug mode. This also enables
                          safe mode. [default: disabled]
  --enable-thread-safe    Enable thread-safe capability. [default: disabled]
  --enable-profiling      Enable time and memory profiling. [default: disabled]
  --disable-versioning    Disable library versioning. [default: enabled]

Optional Packages:

  --with-mpi[=DIR]        Provide the MPI installation path in DIR.
  --with-pthread=DIR      Search Pthreads library within the supplied path
                          DIR, when --enable-thread-safe is enabled.

Some influential environment variables:
  RM          Command for deleting files or directories. [default: rm]
  MPICC       MPI C compiler, [default: CC]
  CC          C compiler command (used only if MPICC is not set)
  CFLAGS      C compiler flags
  LDFLAGS     linker flags, e.g. -L<lib dir> if you have libraries in a
              nonstandard directory <lib dir>
  LIBS        libraries to pass to the linker, e.g. -l<library>
  CPPFLAGS    (Objective) C/C++ preprocessor flags, e.g. -I<include dir> if
              you have headers in a nonstandard directory <include dir>
  LT_SYS_LIBRARY_PATH
              User-defined run-time library search path.
  CPP         C preprocessor
  TESTSEQRUN  Run command (on one MPI process) for "make check" on a cross-
              compile environment. Example: "aprun -n 1". [default: none]
  TESTMPIRUN  MPI run command for "make ptest", [default: mpiexec -n NP]
  TESTOUTDIR  Output file directory for "make check" and "make ptest",
              [default: ./]
```

Use the environment variables listed above to override the choices made by
`configure' or to help it to find libraries and programs with nonstandard
names/locations.

GIO can automatically detect the available MPI compilers and compile flags.
If alternate compilers or flags are desired, they can be specified by the
following environment variables and/or configure options.

Some influential environment variables:

    + CFLAGS, CPPFLAGS, LDFLAGS and LIBS

      Setting these compile flags would result in the GIO library being built
      with these flags.

    + MPICC

      Setting this variable would result in the GIO library being built
      with this compiler. CC will be overwritten by its corresponding MPI
      compiler variable.

Note the compile flags, such as -O2 or -g, should be given in CFLAGS
environment variables. Please do not set them in compiler variable. For
instance, setting MPICC="mpicc -O2" may result in the error of compiler not
found.

---

### Testing the GIO Installation

Two type of testings are implemented in GIO: sequential and parallel.

* For testing sequential programs, the command is
  ```
  make check
  ```

* For testing parallel programs, the command is
  ```
  make ptest
  ```
  which uses 4 MPI processes to run all test programs

* For more elaborated parallel testing, command
  ```
  make ptests
  ```
  will use up to 10 MPI processes to run all test programs.

Note the above commands can be run before "make install".

Command "make tests" will build executables of all the test programs. This can
be handy if testing must run through a batch job system. Having the testing
executables built before submitting a batch job could save a lot of time.

There are three environment variables that can be used to run "make check" and
"make ptest" on a cross compile platform.
* TESTMPIRUN : command to launch MPI jobs. default: "mpiexec -n NP"
* TESTSEQRUN : command to run MPI executable sequentially. default: none
* TESTOUTDIR : output directory. default: "./"

Examples:
```
make check TESTOUTDIR=/scratch
make ptest TESTMPIRUN="aprun -n NP" TESTOUTDIR=/scratch
```

Note the keyword "NP" used in the environment variable string TESTMPIRUN.  It
will be replaced with the different numbers of MPI processes used in testing.
Currently, the testing uses up to 10 processes. Hence, please make sure the
process allocation at least contains 10 processes.

One can also run "make ptest" on batch queue systems. It is recommended to
build all the testing executables before submitting the batch job. This can be
done by running the below command.
```
make tests
```

Note on setting TESTOUTDIR. In order to run parallel test correctly, the output
directory must be on a file system accessible to all MPI processes.  We
recommend to use parallel file systems or POSIX compliant file systems (Using
NFS will most likely fail the parallel test.)

