# simfs — Simulated File System
CSC 4420 – Computer Operating Systems | Wayne State University

---

## Overview

simfs is a simulated file system stored inside a single Unix file.
It supports one flat directory with a fixed number of files and data blocks.
All file operations (create, read, write, delete) are implemented from scratch.

---

## Files

| File | Description |
|------|-------------|
| `simfs.c` | Main program — parses arguments and calls the right operation |
| `simfs.h` | Function prototypes shared across all files |
| `simfs_ops.c` | All file system operations and helper functions |
| `simfstypes.h` | Struct definitions for fentry and fnode, and constants |
| `initfs.c` | Initializes a new empty filesystem file |
| `printfs.c` | Prints the filesystem metadata in human-readable form |
| `Makefile` | Builds the project |

---

## How to Compile

```bash
make
```

To recompile from scratch:
```bash
make clean
make
```

---

## How to Run

All commands follow this format:
```bash
./simfs -f <filesystem_file> <command> [arguments]
```

---

## Commands

### 1. Initialize the filesystem
Must be run first before any other command.
```bash
./simfs -f myfs.img initfs
```

### 2. Show filesystem state
```bash
./simfs -f myfs.img printfs
```

### 3. Create a file
```bash
./simfs -f myfs.img createfile myfile
```

### 4. Write data to a file
```bash
printf 'Hello, World!' | ./simfs -f myfs.img writefile myfile 0 13
```
Format: `writefile <filename> <start_offset> <length>`

### 5. Read data from a file
```bash
./simfs -f myfs.img readfile myfile 0 13
```
Format: `readfile <filename> <start_offset> <length>`

### 6. Delete a file
```bash
./simfs -f myfs.img deletefile myfile
```

---

## Full Test Sequence

Copy and run these commands in order to test everything:

```bash
# Step 1 - Compile
make

# Step 2 - Initialize filesystem
./simfs -f myfs.img initfs

# Step 3 - Show empty filesystem
./simfs -f myfs.img printfs

# Step 4 - Create a file
./simfs -f myfs.img createfile myfile

# Step 5 - Write data to the file
printf 'Hello, World!' | ./simfs -f myfs.img writefile myfile 0 13

# Step 6 - Read data back from the file
./simfs -f myfs.img readfile myfile 0 13 && echo ""

# Step 7 - Delete the file
./simfs -f myfs.img deletefile myfile

# Step 8 - Show filesystem again (should be empty)
./simfs -f myfs.img printfs
```

---

## How to Test Run

```bash
make
chmod +x test_simfs.sh
./test_simfs.sh
```

---

## Running the Test Script

A test script is included that runs 15 automated test cases:

```bash
chmod +x test_simfs.sh
./test_simfs.sh
```

Expected output — all 15 tests should pass:
```
Initializing fresh file system...
Running: [1] Create valid file ... PASSED
Running: [2] Create exactly 11 char file ... PASSED
Running: [3] Create too long file (>11) ... PASSED
...
Running: [15] Read non-existent file ... PASSED
Testing complete.
```

---

## Constants (defined in simfstypes.h)

| Constant | Value | Meaning |
|----------|-------|---------|
| `MAXFILES` | 8 | Maximum number of files in the filesystem |
| `MAXBLOCKS` | 32 | Maximum number of data blocks |
| `BLOCKSIZE` | 128 | Size of each block in bytes |

---

## Error Handling

All operations print an error to stderr and exit with a non-zero code if:
- A filename is longer than 11 characters
- A file already exists or is not found
- The start position is out of range or would create a gap
- There are not enough free blocks to complete a write
- Any system call (fread, fwrite, fseek) fails

