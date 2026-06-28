# GIO - A Shared-file Parallel I/O Library

GIO is an I/O library designed for supporting accessing a shared file by
multiple processes in parallel. GIO follows the I/O semantics of
[MPI](https://www.mpi-forum.org) standard, but provides only a subset of its
features and a slightly different API syntax.

* Similar to MPI-IO, GIO provides collective and independent read and write
  APIs. It also supports I/O hints through setting the MPI info objects, and
  follows the MPI-IO data consistency semantics.
* Unlike MPI-IO, GIO does not support features such as, fileview, non-blocking
  APIs, asynchronous APIs, split APIs, or shared-file pointers.

## GIO Software Releases
* The latest version
  [1.0.0](https://github.com/wkliao/gio/wiki/releases/gio-1.0.0.tar.gz)
  was released on June 27, 2026.
* All previous release tarball files are available from the
  [download page](https://github.com/wkliao/gio/wiki/GIO-software-release-tarball-files)

## Software Dependency

GIO is built on top of MPI and thus requires an MPI C compiler. Shared-file
access requires coordination of processes in order to achieve consistency
results and a good performance. In particular, GIO adopts the two-phase I/O
strategy to implement its collective read and write APIs.

## GIO Application Programming Interfaces (APIs)

The names of all GIO public APIs contain the same prefix, "GIO_". For example,
the two APIs below are for opening and closing a shared file, respectively
```
int GIO_open(MPI_Comm    comm,      /* MPI communicator */
             const char *filename,  /* name of the file to be opened/created */
             int         amode,     /* file access mode */
             MPI_Info    info,      /* I/O hints */
             GIO_File   *fh);       /* file handler returend to the caller */

int GIO_close(GIO_File *fh);
```

There are 4 APIs for performing file reads and writes. Similar to MPI-IO, the
ones with suffix name '_all' are collective and without independent. The
collective APIs require all processes in the MPI communicator to participate
the call. All APIs for reading from and writing to the shared file contain
arguments describing the layouts for both file accesses and buffer in memory,
in the form of a list of offset-length pairs. For example,
```
MPI_Offset GIO_write_all(GIO_File          fh,
                         const void       *buf,
                         MPI_Offset        file_npairs,
                         const MPI_Offset *file_offs,
                         const MPI_Offset *file_lens,
                         MPI_Offset        buf_npairs,
                         const MPI_Offset *buf_offs,
                         const MPI_Offset *buf_lens);
```
* File access layout is described by arguments `file_npairs`, `file_offs`, and
  `file_lens`.
  + `file_npairs` is the number of offset-length pairs,
  + `file_offs` is an array containing `file_npairs` number of non-contiguous
    file offsets, and
  + `file_lens` is an array containing `file_npairs` number of non-contiguous
    request lengths, each corresponding to the element in `file_offs`.
* Buffer layout is described by arguments `buf_npairs`, `buf_offs`, and
  `buf_lens`.
  + `buf_npairs` is the number of offset-length pairs,
  + `buf_offs` is an array containing `buf_npairs` number of non-contiguous
    offsets relative to the memory address pointed by argument `buf`, and
  + `buf_lens` is an array containing `buf_npairs` number of non-contiguous
    request lengths, each corresponding to the element in `buf_offs`.

## User documents
* [C API references](docs/APIs.md)
* A complete list of [I/O hints](docs/hints.md) supported by GIO
* GIO [utility programs](./utils/README.md)

## Related Projects and Application Users
* [PnetCDF](https://parallel-netcdf.github.io), a high-level parallel I/O
  library for accessing Unidata's NetCDF, files in classic formats.


---
Copyright (C) 2026, Northwestern University.

See COPYRIGHT notice in top-level directory.

