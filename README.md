# Unmad — Mini File System (OS Lab Project)

**Course:** Operating Systems Lab (MU CSE 61)  
**Project Type:** Individual Mini File System Implementation in C  
**Core OS Concepts:** File Systems, On-Disk Layouts, Superblocks, Inodes, Free-Space Bitmaps, Indexed Allocation, Hierarchical Directories, Persistence

---

## Overview

**Unmad** is a persistent, user-space mini file system implemented in C99/C11, backed by a 1 MB virtual disk image (`disk.img`) divided into 2,048 blocks of 512 bytes each. It mimics Unix-like file system semantics, providing indexed block allocation, hierarchical directories, a bitwise free-block bitmap, and an interactive shell.

### Key Features
* **Persistent Virtual Disk:** All writes flush immediately to `disk.img`; files and directories survive across program restarts.
* **Bitwise Free-Block Bitmap:** Tracks all 2,048 blocks using bit operations (`|=`, `&= ~`, `>> & 1`) with disk-full error handling.
* **Indexed Allocation:** 10 direct block pointers + 1 single-indirect block pointer (128 pointers) supporting files up to 138 blocks (70,656 bytes).
* **Hierarchical Directories:** Full support for `mkdir`, `cd` (`.`, `..`, `/`, `dirname`), `pwd`, and `rmdir` (blocking removal of non-empty directories).
* **Storage Diagnostics:** `df`-style usage summary and a terminal **Visual Block Map (`map`)** displaying system metadata (`S`), used data (`#`), and free blocks (`.`).
* **Fixed-Size Data Structures:** Structs are verified at compile time with `_Static_assert` (`superblock_t` = 512 B, `inode_t` = 64 B, `dir_entry_t` = 32 B).

---

## On-Disk Architecture (`disk.img` — 1 MB / 2,048 Blocks)

```text
+---------------+---------------+-----------------------+-----------------------------------+
|    Block 0    |    Block 1    |     Blocks 2 .. 17    |         Blocks 18 .. 2047         |
|  Superblock   |    Bitmap     |      Inode Table      |            Data Blocks            |
|    (512 B)    |    (512 B)    |  (128 inodes x 64 B)  |      (2,030 blocks x 512 B)       |
+---------------+---------------+-----------------------+-----------------------------------+
  Magic: "UNMD"   2,048 bits      Inode 0 = "/" (Root)    File data, directory entries,
  (0x554E4D44)    (256 B used)    10 direct + 1 indirect  and single-indirect pointer blocks
```

### How a Filename Resolves to Data
1. **Directory Search:** The current directory (`cwd_inode`) stores `dir_entry_t` structs in its data blocks. The system scans entries matching `name` to retrieve `inode_num`.
2. **Inode Lookup:** The system indexes into `inodes[inode_num]` to read file metadata, file size, permissions, and block pointers.
3. **Block Traversal:**
   - Bytes 0 to 5,119: read directly from `node->direct[0..9]` (10 direct blocks $\times$ 512 B = 5,120 B).
   - Bytes 5,120 to 70,655: read through `node->indirect`, which points to a 512-byte block holding 128 `uint32_t` block pointers.
4. **Disk I/O:** The system seeks to `block_num * 512` in `disk.img` to read or write the actual data bytes.

---

## Build & Run Instructions

### Prerequisites
* `gcc` or `clang` (C11 support)
* `make`
* `xxd` (optional, for raw hex inspection of `disk.img`)

### Commands
```bash
# Compile the binary with strict warnings (-Wall -Wextra -std=c11 -O2)
make

# Launch the interactive file system shell
make run

# Run the automated 11-case test suite
make test

# Inspect the raw on-disk superblock header
make hex

# Remove compiled binary and disk.img
make clean
```

---

## Supported Shell Commands

| Command | Usage | Description |
| :--- | :--- | :--- |
| `format` | `format` | Initializes `disk.img`, writes superblock (`UNMD`), resets bitmap and root `/` |
| `create` | `create <name>` | Creates an empty file in the current directory |
| `write` | `write <name> <text>` | Writes text to a file, allocating direct/indirect blocks as needed |
| `writefill` | `writefill <name> <bytes>` | Helper to write `<bytes>` of test data (ideal for testing indirect blocks / full disk) |
| `read` | `read <name>` | Prints the contents of a file |
| `ls` | `ls` | Lists files and subdirectories in the current directory |
| `info` | `info <name>` | Displays inode number, type, permissions, size, `ctime`, and block pointers |
| `delete` | `delete <name>` | Deletes a file and frees its inode and data blocks in the bitmap |
| `mkdir` | `mkdir <name>` | Creates a new subdirectory |
| `cd` | `cd <dir \| .. \| />` | Changes the current working directory |
| `pwd` | `pwd` | Prints the full path of the current working directory |
| `rmdir` | `rmdir <name>` | Removes an empty directory |
| `df` | `df` | Shows total, used, and available blocks and inodes |
| `map` | `map` | Displays a visual grid of system metadata, used data, and free blocks |
| `exit` | `exit` | Flushes metadata to `disk.img` and exits the shell |

---

## Test Summary (`run_tests.sh`)

| # | Test Case | Expected Result |
| :-: | :--- | :--- |
| 1 | Format & Superblock | `disk.img` created; `xxd disk.img \| head` shows `UNMD` magic bytes |
| 2 | Create, Write & Read | File stores text and reads back identically |
| 3 | Metadata (`info`) | Displays inode index, permissions (`0644`), timestamp, and allocated block numbers |
| 4 | Persistence | Exiting and reopening `disk.img` retains all files and contents |
| 5 | Duplicate File | Rejects duplicate names with `Error: File or directory already exists.` |
| 6 | Filename Too Long | Rejects names > 27 chars with `Error: Name too long (max 27 chars).` |
| 7 | File Not Found | Rejects missing files with `Error: File not found.` |
| 8 | Delete & Block Reuse | Deleting a file clears bitmap bits; next write reuses the freed block |
| 9 | Indirect Block (10 KB) | Allocates 10 direct pointers + 1 indirect block (20 data blocks total) |
| 10 | Capacity / Disk Full | Rejects oversized writes or full-disk allocations without corruption |
| 11 | Hierarchical Dirs | `mkdir`, `cd`, `pwd`, and `rmdir` (blocking non-empty deletion) work cleanly |

---

## Quick Answers for Viva Questions

* **What the superblock stores:** File system metadata needed to mount the disk — the magic number (`0x554E4D44` identifying a valid "UNMD" file system), total/free block counts, total/free inode counts, and starting block offsets for the bitmap, inode table, and root directory.
* **How a filename leads to data:** The OS searches the directory entries for a matching name to obtain `inode_num` $\rightarrow$ indexes into the Inode Table to load `inode_t` $\rightarrow$ reads direct (and indirect) block pointers $\rightarrow$ seeks to `block_num * 512` on disk to read/write raw data bytes.
* **How the bitmap is updated with bit operations:**
  - Byte index: `block / 8`, bit offset: `block % 8`.
  - Allocation: `bitmap[b / 8] |= (1 << (b % 8))`
  - Freeing: `bitmap[b / 8] &= ~(1 << (b % 8))`
  - Status check: `(bitmap[b / 8] >> (b % 8)) & 1`
* **What happens on delete:** Data blocks are not physically zeroed out immediately; instead, their bits in the free-space bitmap are cleared to 0 (making them eligible for reallocation), the inode's `used` flag is set to 0, and the directory entry is marked free.
* **Why direct pointers alone limit file size:** Because an inode has a fixed size (64 bytes), it can only hold a small, fixed number of direct block pointers (10 pointers $\times$ 512 B = 5 KB max). An indirect block delegates pointer storage to a 512-byte data block holding 128 additional 4-byte pointers, scaling capacity to ~70 KB without inflating every inode.
