# CS537 P4: Distributed File System
A distributed file server that can be reached using shared library on the client side by establishing UDP communications. The file system is a simple version of [Log-Structured File System](https://en.wikipedia.org/wiki/Log-structured_file_system). It stores the data in a persistent file image that can be restored upon reboot.

##### Table of Contents
* [Installation](#installation)
  * [Server & Client Library](#server-and-client-library)
  * [Client](#client-for-testing-only)
* [Implementation](#implementation)
  * [Server](#server)
  * [Client Library](#client-library)
* [Acknowledgement](#acknowledgement)

## Installation
### Server and Client Library
To compile <code>server</code> and shared library <code>libmfs</code>, run:
<pre><code>make</code></pre>
To start the server, run:
<pre><code>server [portnum] [file-system-image]</code></pre>
Where:
* <code>portnum</code>: the port number that the file server should listen on.
* <code>file-system-image</code>: a file that contains the file system image. If this file does not exist, the file server will create one and initialize it properly.
### Client (for testing only)
In order to run <code>client</code>, run:
<pre><code>make client</code></pre>
Then, export the 
<pre><code>source .LD_LIBRARY_PATH</code></pre>

## Implementation
### Server
The log-based file system is simple ever-growing log. There is no background job that flushes the checkpoint region every amount of time. Instead, any update to the state of the file system is flushed immediately. There is also no support for cleaning unused data. It utilizes in-memory cache system to make any operations faster, while reading/writing to disk if necessary.

Here are some constraints:
* Maximum inodes / total number of files and directories (including <code>/</code>): <code>4096</code>
* Maximum number of blocks per file: <code>14</code>
* Block size: <code>4KB</code>
* Maximum number of entries per directory (including <code>.</code> and <code>..</code>): <code>128</code>
* Maximum character names in a directory: <code>27</code>

The source code can be found in [server.c](server.c).

### Client Library
The client library simply makes a UDP call to the server depending on the request. It also implements a retry if the server is unresponsive or if there is any package loss with a default timeout of <code>5</code> seconds.

List of APIs:
* `int MFS_Init(char *hostname, int port)`: `MFS_Init()` takes a host name
and port number and uses those to find the server exporting the file system.
* `int MFS_Lookup(int pinum, char *name)`: `MFS_Lookup()` takes the parent
inode number (which should be the inode number of a directory) and looks up
the entry `name` in it. The inode number of `name` is returned. Success: 
return inode number of name; failure: return -1. Failure modes: invalid pinum,
name does not exist in pinum.
* `int MFS_Stat(int inum, MFS_Stat_t *m)`: `MFS_Stat()` returns some
information about the file specified by inum. Upon success, return 0,
otherwise -1. The exact info returned is defined by `MFS_Stat_t`. Failure modes:
inum does not exist. 
* `int MFS_Write(int inum, char *buffer, int block)`: `MFS_Write()` writes a
block of size 4096 bytes at the block offset specified by `block`. Returns 0
on success, -1 on failure. Failure modes: invalid inum, invalid block, not a
regular file (because you can't write to directories).
* `int MFS_Read(int inum, char *buffer, int block)`: `MFS_Read()` reads
a block specified by `block` into the buffer from file specified by
`inum`. The routine should work for either a file or directory;
directories should return data in the format specified by
`MFS_DirEnt_t`. Success: 0, failure: -1. Failure modes: invalid inum,
invalid block. 
* `int MFS_Creat(int pinum, int type, char *name)`: `MFS_Creat()` makes a
file (`type == MFS_REGULAR_FILE`) or directory (`type == MFS_DIRECTORY`)
in the parent directory specified by `pinum` of name `name`. Returns 0 on
success, -1 on failure. Failure modes: pinum does not exist, or name is too
long. If `name` already exists, return success (think about why).
* `int MFS_Unlink(int pinum, char *name)`: `MFS_Unlink()` removes the file or
directory `name` from the directory specified by `pinum`. 0 on success, -1
on failure. Failure modes: pinum does not exist, directory is NOT empty. Note
that the name not existing is NOT a failure by our definition (think about why
this might be). 
* `int MFS_Shutdown()`: `MFS_Shutdown()` just tells the server to force all
of its data structures to disk and shutdown by calling `exit(0)`. This interface
will mostly be used for testing purposes.

The source code can be found in [mfs.c](mfs.c).

## Acknowledgement
This is an assignment for a class [Comp Sci. 537: Introduction to Operating Systems](https://pages.cs.wisc.edu/~remzi/Classes/537/Fall2021/) by [Remzi Arpaci-Dusseau](https://pages.cs.wisc.edu/~remzi/). Please refer to [this repo](https://github.com/remzi-arpacidusseau/ostep-projects/blob/master/filesystems-distributed/README.md) for more details about the assignment.
