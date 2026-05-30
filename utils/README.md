## Utility Programs

* [gio-config](#gio-config)
* [gio_version](#gio_version)

### gio-config
<ul>
  <li>A utility program to display the build and installation information of
      the GIO library.
  </li>
  <li> <details>
  <summary>Manual page (click to expand)</summary>

```
gio-config is a utility program to display the build and installation
information of the GIO library.

Usage: gio-config [OPTION]

Available values for OPTION include:

  --help                      display this help message and exit
  --all                       display all options
  --cc                        C compiler used to build GIO
  --cflags                    C compiler flags used to build GIO
  --cppflags                  C pre-processor flags used to build GIO
  --ldflags                   Linker flags used to build GIO
  --libs                      Extra libraries used to build GIO
  --profiling                 Whether internal profiling is enabled or not
  --thread-safe               Whether thread-safe capability is enabled or not
  --debug                     Whether GIO is built with debug mode
  --prefix                    Installation directory
  --includedir                Installation directory containing header files
  --libdir                    Installation directory containing library files
  --version                   Library version
  --release-date              Date of GIO source was released
  --config-date               Date of GIO library was configured
```
</details></li>
</ul>

### gio_version
<ul>
  <li>A utility program to print the version information of GIO library and the
  configure command line used to build the library
  </li>
  <li> <details>
  <summary>Manual page (click to expand)</summary>

```
 gio_version(1)             GIO utilities            gio_version(1)

 NAME
        gio_version - print the version information of GIO library

 SYNOPSIS
        gio_version [-v] [-d] [-c] [-b] [-h]

 DESCRIPTION
        gio_version  prints  the version information of GIO library and
        the configure command line used to build the library

        If no argument is given, all information is printed.

 OPTIONS
        -v     Version number of this GIO release.

        -d     Release date.

        -c     Configure command-line arguments used to build this GIO

        -b     MPI compilers used to build this GIO library

        -h     Print the available command-line options of gio_version

 EXAMPLES
        Print all information about the GIO library by running the  command
        with no options.

        % gio_version

        GIO Version:         1.1.0
        GIO Release date:    May 29, 2026
        GIO configure:  --with-mpi=/usr/local/bin
        MPICC:  /usr/local/bin/mpicc -g -O2

 SEE ALSO
        gio(3)

 DATE
        May 29, 2026
```
</details></li>
</ul>

Copyright (C) 2026, Northwestern University.
See COPYRIGHT notice in top-level directory.

