# GIO C Application Programming Interfaces Guide

* [GIO_inq_libvers](#gio_inq_libvers) - inquires GIO version string
* [GIO_strerror](#gio_strerror) - message corresponding to a GIO error code
* [GIO_strerrno](#gio_strerrno) - name of a GIO error code
* [GIO_open](#gio_open) - opens or creates a file
* [GIO_close](#gio_close) - closes a file
* [GIO_sync](#gio_sync) - synchronizes a file to disk
* [GIO_delete](#gio_delete) - deletes a file
* [GIO_set_size](#gio_set_size) - sets the size of a file
* [GIO_get_size](#gio_get_size) - obtains the current size of a file
* [GIO_get_info](#gio_get_info) - obtains I/O hints used by GIO library
* [GIO_write](#gio_write) - writes to a file (independent call)
* [GIO_write_all](#gio_write_all) - writes to a file (collective call)
* [GIO_read](#gio_read) - reads from a file (independent call)
* [GIO_read_all](#gio_read_all) - reads from a file (collective call)
* [GIO_flatten_subarray](#gio_flatten_subarray) - flattens a subarray access to a list of offset-length pairs

---
### GIO_inq_libvers
This API returns a string identifying the version of the GIO library, and
when it was built.
* Syntax
  ```
  const char* GIO_inq_libvers(void);
  ```

* Example
  ```
  #include <gio.h>
  ...
  printf("%s\n", GIO_inq_libvers());
  ```
* Example output:
  ```
  1.0.0 of 19 May 2026
  ```

---
### GIO_strerror
This API returns a static reference to an error message string corresponding to
an integer GIO error code or to a system error number, presumably returned by a
previous call to a GIO API. The list of GIO error codes can be found in
[Error Codes](#error_codes).
* Syntax
  ```
  const char* GIO_strerror(int err);
  ```
  + **IN** `err` - an error code returned from a call to a GIO API previously.

* Errors

  If `err` is not one of the GIO error codes or any system error code (as
  understood by the system `strerror()` function), `gio_strerror()` returns a
  string indicating that there is no such error status.

* Example

  Below is an example that checks the error code returned from a call to
  `GIO_open()`. If the call to `GIO_open()` failed, it prints the error message
  describing the cause of failure.
  ```
  #include <gio.h>
  ...
  int err;

  err = GIO_open(MPI_COMM_WORLD, "testfile.dat", ...);
  if (err != GIO_NOERR)
      fprintf(stderr, "%s\n", gio_strerror(err));
  ```
* Example output:

  The error corresponds to GIO error code `GIO_EMULTIDEFINE_AMODE` is
  ```
  File open mode is inconsistent among processes.
  ```

---
### GIO_strerrno
This API returns a static reference to the name string of the supplied GIO
error code. The list of GIO error codes can be found in
[Error Codes](#error_codes).
* Syntax
  ```
  const char* GIO_strerrno(int err);
  ```
  + **IN**  `err` - a GIO error code defined in the header file `gio.h`.

* Errors

  If `err` is not one of the GIO error codes `gio_strerror()` returns a string
  indicating that it is an unknown code.

* Example

  Below is an example that print the name of error code returned from a call to
  `GIO_open()`.
  ```
  #include <gio.h>
  ...
  int err;

  err = GIO_open(MPI_COMM_WORLD, "testfile.dat", ...);
  if (err != GIO_NOERR)
      fprintf(stderr, "%s\n", gio_strerrno(err));
  ```
* Example output:

  If the supplied error code is equal to `GIO_EMULTIDEFINE_AMODE`, the above
  example will print the below string to stderr.
  ```
  GIO_EMULTIDEFINE_AMODE
  ```

---
### GIO_open
This API opens an existing file or creates a new file for access.

* Operational Mode

  This API is a collective routine, i.e. all processes included in the MPI
  communicator supplied in the first argument `comm` must participate the call.
  In addition, all processes must provide argument `filenames` that reference
  the same file. The values of `amode` and `info` must also be the same.

* Syntax
  ```
  int GIO_open(MPI_Comm    comm,
               const char *filename,
               int         amode,
               MPI_Info    info,
               GIO_File   *fh);
  ```
  + **IN**  `comm` - an MPI communicator and must be an MPI intra-communicator.
  + **IN**  `filename` - name of the file to be opened or created.
  + **IN**  `amode` - file access mode, a combination of the following constants
    * `O_CREAT`   - create file if it does not exist
    * `O_RDONLY`  - open for reading only
    * `O_WRONLY`  - open for writing only
    * `O_RDWR`    - open for reading and writing
  + **IN**  info, an MPI info object that contains some I/O hints. See the
    complete list of [I/O hints](#io_hints) supported by GIO.
  + **OUT** `fh` - a pointer to the new file handle

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_ENOENT`: The file to be opened does not exist.
  + `GIO_EBAD_FILE`: Invalid file name, file name too long, or the directory
    does not exist.
  + `GIO_EINVAL_AMODE`: invalid value used in argument `amode`.
  + `GIO_EPERM`: Attempting to create a file in a directory where you do not
     have permission to open files.
  + `GIO_ENOMEM`: Out of memory
  + `GIO_ENFILE`: Too many files open
  + `GIO_EFILE`: other unknown I/O error
  + `GIO_EMULTIDEFINE_AMODE`: inconsistent values used in argument `amode`
    among processes.
  + `GIO_EMULTIDEFINE_FNC_ARGS`: one or more arguments are inconsistent among
    processes.

* Example

  Below is an example of calling `gio_open()` to create a new file named
  `foo.dat` in the reading and writing mode.
  ```
  #include <gio.h>
  ...
  int err;
  MPI_Info info;
  GIO_File fh;
       ...
  MPI_Info_create(&info);
  MPI_Info_set(info, "overstriping_ratio", "4");
  ...
  err = gio_open(MPI_COMM_WORLD, "foo.nc", O_CREAT|O_RWRD, info, &fh);
  if (err != GIO_NOERR) printf("Error: %s\n",gio_strerror(err));
  MPI_Info_free(&info);
  ```

---
### GIO_close
This API closes an previously opened file.

* Operational Mode

  This API is a collective routine, i.e. all processes in the MPI communicator
  used in the call to `GIO_open()` previously must participate the call.

* Syntax
  ```
  int GIO_close(GIO_File *fh);
  ```
  + **IN** `fh` - a pointer to the file handle obtained from a call to
    `gio_open()` previously

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler

* Example

  Below is an example of calling `gio_open()` to create a new file named
  `foo.dat` followed by a call to `gio_close()` to close the file.
  ```
  #include <gio.h>
  ...
  int err;
  GIO_File fh;
       ...
  err = gio_open(MPI_COMM_WORLD, "foo.nc", O_CREAT|O_RWRD, MPI_INFO_NULL, &fh);
  if (err != GIO_NOERR) printf("Error: %s\n",gio_strerror(err));
       ...
  err = gio_close(&fh);
  if (err != GIO_NOERR) printf("Error: %s\n",gio_strerror(err));
  ```

---
### GIO_sync
This API synchronize a file's in-core state with that on disk.

* Operational Mode

  This API is an independent routine.

* Syntax
  ```
  int GIO_sync(GIO_File fh);
  ```
  + **IN** `fh` - a file handle obtained from a call to `gio_open()` previously

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_EFILE`: Other file system error


---
### GIO_delete
This API deletes a file.

* Operational Mode

  This API is an independent routine.

* Syntax
  ```
  int GIO_delete(const char *filename);
  ```
  + **IN** `filename` - name of the file to be deleted.

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EACCESS`: Access permission failure
  + `GIO_ENOENT`: The named file does not exist.
  + `GIO_EBAD_FILE`: Invalid file name (e.g., path name too long)
  + `GIO_EFILE`: Other file system error

---
### GIO_set_size
This API sets the size in bytes of an opened file.

* Operational Mode

  This API is a collective routine, i.e. all processes in the MPI communicator
  used in the call to `GIO_open()` previously must participate the call.

* Syntax
  ```
  int GIO_set_size(GIO_File   fh,
                   MPI_Offset size);
  ```
  + **IN** `fh` - a file handle obtained from a call to `gio_open()` previously
  + **IN** `size` - number of bytes

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_EACCESS`: Access permission failure
  + `GIO_EFILE`: Other file system error

---
### GIO_get_size
This API returns the current file size in bytes.

* Operational Mode

  This API is an independent routine.

* Syntax
  ```
  int GIO_get_size(GIO_File    fh,
                   MPI_Offset *size);
  ```
  + **IN**  `fh` - a file handle obtained from a call to `gio_open()` previously
  + **OUT** `size` - a pointer to a buffer storing the file size in bytes

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_EFILE`: Other file system error

---
### GIO_get_info
This API returns an MPI_Info object containing the I/O hints for a file that
are actually being used by GIO.

* Operational Mode

  This API is an independent routine.

* Syntax
  ```
  int GIO_get_info(GIO_File  fh,
                   MPI_Info *info);
  ```
  + **IN**  `fh` - a file handle obtained from a call to `gio_open()` previously
  + **OUT** `info` - a pointer to a new MPI_Info object. Users are responsible
    to call `MPI_Info_free()` to free the object.

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_EFILE`: Other file system error

---
### GIO_write
This API writes a buffer to an opened file. The file access layout is described
by arguments `file_npairs`, `file_offs`, and `file_lens`, which allows writing
to multiple, noncontiguous file regions in a single call to this API.  The
buffer layout is described by arguments `buf_npairs`, `buf_offs`, and
`buf_lens`, which allows write buffer to contain multiple, noncontiguous memory
regions.

* Operational Mode

  This API is an independent routine.

* Syntax
  ```
  MPI_Offset GIO_write(GIO_File          fh,
                       const void       *buf,
                       MPI_Offset        file_npairs,
                       const MPI_Offset *file_offs,
                       const MPI_Offset *file_lens,
                       MPI_Offset        buf_npairs,
                       const MPI_Offset *buf_offs,
                       const MPI_Offset *buf_lens);
  ```
  + **IN** `fh` - a file handle obtained from a call to `gio_open()` previously
  + **IN** `buf` - pointer to write buffer
  + **IN** `file_npairs` - number of elements in arrays of `file_offs` and
    `file_lens`
  + **IN** `file_offs` - array of file offsets
  + **IN** `file_lens` - array of file lengths
  + **IN** `buf_npairs` - number of elements in arrays of `buf_offs` and
    `buf_lens`
  + **IN** `buf_offs` - array of offsets relative to `buf`
  + **IN** `buf_lens` - array of buffer lengths

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_EPERM`: Write to read only
  + `GIO_EACCESS`: Access failure
  + `GIO_ENO_SPACE`: Physical disk space or inode exhaustion
  + `GIO_EQUOTA`: Disk quota exceeded
  + `GIO_EINTOVERFLOW`: Integer type casting overflow
  + `GIO_EFILE`: Other file system error

* Full example C program
  + [examples/ex_write_indep.c](../examples/ex_write_indep.c)

---
### GIO_write_all
This API writes a buffer to an opened file. The file access layout is described
by arguments `file_npairs`, `file_offs`, and `file_lens`, which allows writing
to multiple, noncontiguous file regions in a single call to this API.  The
buffer layout is described by arguments `buf_npairs`, `buf_offs`, and
`buf_lens`, which allows write buffer to contain multiple, noncontiguous memory
regions.

* Operational Mode

  This API is a collective routine, i.e. all processes in the MPI communicator
  used in the call to `GIO_open()` previously must participate the call.

* Syntax
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
  + **IN** `fh` - a file handle obtained from a call to `gio_open()` previously
  + **IN** `buf` - pointer to write buffer
  + **IN** `file_npairs` - number of elements in arrays of `file_offs` and
    `file_lens`
  + **IN** `file_offs` - array of file offsets
  + **IN** `file_lens` - array of file lengths
  + **IN** `buf_npairs` - number of elements in arrays of `buf_offs` and
    `buf_lens`
  + **IN** `buf_offs` - array of offsets relative to `buf`
  + **IN** `buf_lens` - array of buffer lengths

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_EPERM`: Write to read only
  + `GIO_EACCESS`: Access failure
  + `GIO_ENO_SPACE`: Physical Disk Space or Inode Exhaustion
  + `GIO_EQUOTA`: Disk quota exceeded
  + `GIO_EINTOVERFLOW`: Integer type casting overflow
  + `GIO_EFILE`: Other file system error

* Full example C program
  + [examples/ex_write_coll.c](../examples/ex_write_coll.c)


---
### GIO_read
This API reads from an opened file to a memory buffer. The file access layout
is described by arguments `file_npairs`, `file_offs`, and `file_lens`, which
allows writing to multiple, noncontiguous file regions in a single call to this
API.  The buffer layout is described by arguments `buf_npairs`, `buf_offs`, and
`buf_lens`, which allows write buffer to contain multiple, noncontiguous memory
regions.

* Operational Mode

  This API is an independent routine.

* Syntax
  ```
  MPI_Offset GIO_read(GIO_File          fh,
                      void             *buf,
                      MPI_Offset        file_npairs,
                      const MPI_Offset *file_offs,
                      const MPI_Offset *file_lens,
                      MPI_Offset        buf_npairs,
                      const MPI_Offset *buf_offs,
                      const MPI_Offset *buf_lens);
  ```
  + **IN**  `fh` - a file handle obtained from a call to `gio_open()` previously
  + **OUT** `buf` - pointer to write buffer
  + **IN**  `file_npairs` - number of elements in arrays of `file_offs` and
    `file_lens`
  + **IN**  `file_offs` - array of file offsets
  + **IN**  `file_lens` - array of file lengths
  + **IN**  `buf_npairs` - number of elements in arrays of `buf_offs` and
    `buf_lens`
  + **IN**  `buf_offs` - array of offsets relative to `buf`
  + **IN**  `buf_lens` - array of buffer lengths

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_EPERM`: Operation not permitted
  + `GIO_EACCESS`: Access failure
  + `GIO_EINTOVERFLOW`: Integer type casting overflow
  + `GIO_EFILE`: Other file system error

* Full example C program
  + [examples/ex_read_indep.c](../examples/ex_read_indep.c)


---
### GIO_read_all
This API reads from an opened file to a memory buffer. The file access layout
is described by arguments `file_npairs`, `file_offs`, and `file_lens`, which
allows writing to multiple, noncontiguous file regions in a single call to this
API.  The buffer layout is described by arguments `buf_npairs`, `buf_offs`, and
`buf_lens`, which allows write buffer to contain multiple, noncontiguous memory
regions.

* Operational Mode

  This API is a collective routine, i.e. all processes in the MPI communicator
  used in the call to `GIO_open()` previously must participate the call.

* Syntax
  ```
  MPI_Offset GIO_read_all(GIO_File          fh,
                          void             *buf,
                          MPI_Offset        file_npairs,
                          const MPI_Offset *file_offs,
                          const MPI_Offset *file_lens,
                          MPI_Offset        buf_npairs,
                          const MPI_Offset *buf_offs,
                          const MPI_Offset *buf_lens);
  ```
  + **IN**  `fh` - a file handle obtained from a call to `gio_open()` previously
  + **OUT** `buf` - pointer to write buffer
  + **IN**  `file_npairs` - number of elements in arrays of `file_offs` and
    `file_lens`
  + **IN**  `file_offs` - array of file offsets
  + **IN**  `file_lens` - array of file lengths
  + **IN**  `buf_npairs` - number of elements in arrays of `buf_offs` and
    `buf_lens`
  + **IN**  `buf_offs` - array of offsets relative to `buf`
  + **IN**  `buf_lens` - array of buffer lengths

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_EPERM`: Operation not permitted
  + `GIO_EACCESS`: Access failure
  + `GIO_EINTOVERFLOW`: Integer type casting overflow
  + `GIO_EFILE`: Other file system error

* Full example C program
  + [examples/ex_read_coll.c](../examples/ex_read_indep.c)

---
### GIO_flatten_subarray
This utility API is a convenient subroutine for flattening a subarray access
layout, described by arguments esize, sizes, subsizes, and starts, into a list
of offset-length pairs.

* Operational Mode

  This API is an independent routine.

* Syntax
  ```
  int GIO_flatten_subarray(int          ndims,
                           int          esize,
                           MPI_Offset   disp,
                           const int   *sizes,
                           const int   *subsizes,
                           const int   *starts,
                           MPI_Offset  *npairs,
                           MPI_Offset **offs,
                           MPI_Offset **lens)
  ```
  + **IN** `ndims` - number of array dimensions
  + **IN** `esize` - array element size in bytes
  + **IN** `disp` - displacement, i.e. starting offset
  + **IN** `sizes[ndims]` - global array sizes
  + **IN** `subsizes[ndims]` - subarray sizes
  + **IN** `starts[ndims]` - starting indices of the subarray relative to
           global array
  + **OUT** `npairs` - number of offset-length pairs
  + **OUT** `offs[npairs]` - array of flattened offsets
  + **OUT** `lens[npairs]` - array of flattened lengths

* Errors

  If the call to this API is successful, it returns `GIO_NOERR`. Otherwise, the
  returned error code is one of the followings. Users can make a call to
  [GIO_strerror()](#gio_strerror) to obtain further error messages.

  + `GIO_EINVAL`: Invalid file handler
  + `GIO_ENOMEM`: Out of memory

* Full example C program
  + [examples/ex_write_coll.c](../examples/ex_write_coll.c)

