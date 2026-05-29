# GIO I/O Hints

* User Settable Hints
  + [file_striping](#file_striping) - file striping setting of a new file
    either to inherit from its parent folder or automatically determined by GIO
  + [striping_factor](#striping_factor) - file striping count of a new file
  + [striping_unit](#striping_unit) - file striping unit size of a new file
  + [start_iodevice](#start_iodevice) - starting file server ID of a new file
  + [cb_nodes](#cb_nodes) - number of I/O aggregators for collective APIs
  + [cb_buffer_size](#cb_buffer_size) - collective buffer size that can be
    allocated and used by GIO's implementation for collective APIs
  + [overstriping_ratio](#overstriping_ratio) - Lustre overstriping ratio
  + [cb_read](#cb_read) - whether to enable collective buffering for read APIs
  + [cb_write](#cb_write) - whether to enable collective buffering for write
    APIs
  + [ds_read](#ds_read) - whether to enable data sieving for read APIs
  + [ds_write](#ds_write) - whether to enable data sieving for write APIs
  + [ind_rd_buffer_size](#ind_rd_buffer_size) - temporary buffer size used in
    data sieving for independent read APIs
  + [ind_wr_buffer_size](#ind_wr_buffer_size) - temporary buffer size used in
    data sieving for independent write APIs

* Informative Hints
  + [lustre_num_osts](#lustre_num_osts) - the number of Lustre OSTs a file is
    striped across of
  + [cb_node_list](#cb_node_list) - MPI rank IDs of I/O aggregators

---
I/O hints specified via `MPI_Info` object allow a user to provide information,
such as memory management and file system striping setting. Providing hints
may enable GIO to improve I/O performance over the default settings. Hints are
divided into two subsets: user settable hints and informative hints
(un-settable by users). The former hints require their values to be consistent
among all processes.

## User Settable Hints
---
### file_striping
This hint instructs GIO to set the file striping of a new file to be created.

* Hint values
  + **automatic** - lets GIO to determine the striping configuration (default)
  + **inherit** - inherits the striping configuration of the new file's parent
    folder

---
### striping_factor
This hint instructs GIO to set the file striping count (number of file servers)
of a new file to be created. This hint currently takes effect only when Lustre
is used.

* Hint values
  + A positive integral value. When the value is larger than the total number
    of available file servers, GIO will try to use the total number of
    available file servers to set the new file's striping count.
  + When this hint is not set, GIO by default will use the number of computer
    nodes allocated to the MPI communicator passed to `GIO_open()` as the new
    file's striping count.

---
### striping_unit
This hint instructs GIO to set the file striping unit size of a new file to be
created. This hint currently takes effect only when Lustre is used.

* Hint values
  + A positive integral value of power of 2.
  + When this hint is not set, GIO by default will use 1048576.

---
### start_iodevice
This hint instructs GIO to set the starting file server ID of a new file to be
created. This hint currently takes effect only when Lustre is used.

* Hint values
  + A integral value of power of 2, ranging from 0 to the total number of file
    server minus one.
  + When this hint is not set, GIO by default will use the default value set by
    the underneath file system.

---
### overstriping_ratio
This hint instructs GIO to make use of Lustre's overstriping feature which
allows to create a new file to have more than one stripe per OST. For instance,
when hints `striping_factor` and `overstriping_ratio` are set to 8 and 4,
respectively, the number of OSTs storing the file is 8/4=2.

Overstriping allows users to stripe a file over a number larger than the total
number of available OSTs on a Lustre system. Our evaluation showed overstriping
performs equivalently well as not using it when the number of striping counts
(`striping_factor`) are the same, i.e. between the following two settings:
* `overstriping_ratio` set to 1 and `striping_factor` to `N` (number of OSTs =
  N)
* `overstriping_ratio` set to K and `striping_factor` to `N` (number of OSTs =
  N/K), where N is divisible by K.

This hint currently takes effect only when Lustre is used.

* Hint values
  + A positive integral value that divides the value set in hint
    `striping_factor`
  + When this hint is not set, GIO by default will use 1, i.e. disables the
    overstriping.

* Example
  + After creating a new file with hints `striping_factor`, `striping_unit`,
    and `overstriping_ratio` set to 8, 1048576, and 4, respectively, running
    command `lfs getstripe example_file` will produce a screen output similar
    to the one shown below. In this example, the file is striped across 2 OSTs,
    IDs 10 and 11.
    ```
    % lfs getstripe example_file
    lmm_stripe_count:  8
    lmm_stripe_size:   1048576
    lmm_pattern:       raid0,overstriped
    lmm_layout_gen:    0
    lmm_stripe_offset: 10
    lmm_pool:          original
        obdidx       objid          objid          group
            10        25322723      0x18264e3      0xf0000040b
            11        23943619      0x16d59c3      0xf40000411
            10        25322724      0x18264e4      0xf0000040b
            11        23943620      0x16d59c4      0xf40000411
            10        25322726      0x18264e6      0xf0000040b
            11        23943622      0x16d59c6      0xf40000411
            10        25322727      0x18264e7      0xf0000040b
            11        23943623      0x16d59c7      0xf40000411
    ```

---
### cb_nodes
This hint instructs GIO to set the number of I/O aggregators to be used for
collective buffering, an optimization strategy implemented in the collective
read and write APIs.

* Hint values
  + A positive integral value
  + When this hint is not set, GIO by default will use the followings
    * When the file system is Lustre, the value of `cb_nodes` will be set to be
      a multiple value of one set in hint `striping_factor`.
    * For other file systems, the value of `cb_nodes` will be set to be equal
      to the number of NUMA nodes, i.e. one aggregator per node.

---
### cb_buffer_size
This hint enables GIO to allocate an internal buffer to be used in the
implementation of collective buffering.

* Hint values
  + A positive integral value
  + When this hint is not set, GIO by default will use 16 MiB.

---
### cb_read
This hint instructs GIO to enable/disable collective buffering mechanism
implemented for collective read APIs. Collective buffering, also known as
"two-phase I/O", is an optimization strategy that reorganizes I/O requests
among all MPI processes into a file access layout that minimizes the costs when
making file system calls. This strategy is particularly effective when the
requests consist of a large number of small, non-contiguous regions in the file
spaces.

* Hint values
  + **automatic** - lets GIO to determine based on the file access pattern
    (default)
  + **enable** - enables this feature for all collective read calls
  + **disable** - disables this feature for all collective read calls

---
### cb_write
This hint instructs GIO to enable/disable collective buffering mechanism
implemented for collective write APIs. Collective buffering, also known as
"two-phase I/O", is an optimization strategy that reorganizes I/O requests
among all MPI processes into a file access layout that minimizes the costs when
making file system calls. This strategy is particularly effective when the
requests consist of a large number of small, non-contiguous regions in the file
spaces.

* Hint values
  + **automatic** - lets GIO to determine based on the file access pattern
    (default)
  + **enable** - enables this feature for all collective write calls
  + **disable** - disables this feature for all collective write calls

---
### ds_read
This hint instructs GIO to enable/disable data sieving mechanism implemented
for read APIs.  Data sieving is a strategy for non-contiguous file read
operations that reads a large contiguous block of data (spanning from the first
requested byte to the last requested byte) into a temporary memory buffer and
then copies the actual requested data to the user buffer. It avoids many, small
non-contiguous read operations which can potentially be expensive.

* Hint values
  + **automatic** - lets GIO to determine based on the file access pattern
    (default)
  + **enable** - enables this feature
  + **disable** - disables this feature

---
### ds_write
This hint instructs GIO to enable/disable data sieving mechanism implemented
for write APIs. Data sieving is a strategy for non-contiguous file write
operations that first reads a large contiguous block of data (spanning from the
first requested byte to the last requested byte) into a temporary memory buffer
and then modifies the buffer contents by copying the actual write data from the
user buffer. It avoids many, small non-contiguous write operations which can
potentially be expensive.

* Hint values
  + **automatic** - lets GIO to determine based on the file access pattern
    (default)
  + **enable** - enables this feature
  + **disable** - disables this feature

---
### ind_rd_buffer_size
This hint enables GIO to allocate an internal buffer to be used in the
implementation of data sieving for independent read APIs. Data sieving is a
strategy for non-contiguous file read operations that reads a large contiguous
block of data (spanning from the first requested byte to the last requested
byte) into a temporary memory buffer and then copies the actual requested data
to the user buffer. It avoids many, small non-contiguous read operations which
can potentially be expensive.

* Hint values
  + A positive integral value
  + When this hint is not set, GIO by default will use 4 MiB.

---
### ind_wr_buffer_size
This hint enables GIO to allocate an internal buffer to be used in the
implementation of data sieving for independent write APIs. Data sieving is a
strategy for non-contiguous file write operations that first reads a large
contiguous block of data (spanning from the first requested byte to the last
requested byte) into a temporary memory buffer and then modifies the buffer
contents by copying the actual write data from the user buffer. It avoids many,
small non-contiguous write operations which can potentially be expensive.

* Hint values
  + A positive integral value
  + When this hint is not set, GIO by default will use 4 MiB.


## Informative Hints
GIO also stores some internally generated hints into the `MPI_Info` object
returned to the user when calling API [GIO_get_info()](./APIs.md#gio_get_info).
They can be useful for understanding the GIO's performance at a lower software
and file system levels.

---
### lustre_num_osts
This hint shows the number of Lustre Object Storage Targets (OST) a file is
stripped across. When the Lustre overstriping is not enabled, the value of this
hint is essentially the same as the value shown in hint `striping_factor`. When
overstriping is enabled, the value of this hint reflects the actually number of
OSTs. For instance, when hint `overstriping_ratio` is set to K and
`striping_factor` to `N/K`, the value of this hint will be N/K.

---
### cb_node_list
The value of this hint consists of a list of MPI rank IDs of all the I/O
aggregators, i.e. cb_nodes, selected by the collective buffering mechanism
implemented in GIO.  The MPI rank IDs are relative to the MPI communicator
passed to the call to API [GIO_open()](./APIs.md#gio_open).

