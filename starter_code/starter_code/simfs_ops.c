/*
 * simfs_ops.c
 * Implements all file system operations: create, delete, read, write.
 * Also contains helper functions for opening, loading, and searching metadata.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "simfs.h"


/* ── Open / Close ─────────────────────────────────────────────────────────── */

/* Opens the filesystem file. Exits if it cannot be opened. */
FILE *
openfs(char *filename, char *mode)
{
    FILE *fp;
    if ((fp = fopen(filename, mode)) == NULL) {
        perror("openfs");
        exit(1);
    }
    return fp;
}

/* Closes the filesystem file. Exits if closing fails. */
void
closefs(FILE *fp)
{
    if (fclose(fp) != 0) {
        perror("closefs");
        exit(1);
    }
}


/* ── Metadata Helpers ─────────────────────────────────────────────────────── */

/* Reads (Loads) fentry and fnode arrays from the start of the filesystem file. */
static void
load_metadata(FILE *fp, fentry *file_table, fnode *block_table)
{
    rewind(fp);
    if (fread(file_table, sizeof(fentry), MAXFILES, fp) < MAXFILES) {
        fprintf(stderr, "Error: could not read file entries\n");
        exit(1);
    }
    if (fread(block_table, sizeof(fnode), MAXBLOCKS, fp) < MAXBLOCKS) {
        fprintf(stderr, "Error: could not read block nodes\n");
        exit(1);
    }
}

/* Writes (Save) fentry and fnode arrays back to the start of the filesystem file. */
static void
save_metadata(FILE *fp, fentry *file_table, fnode *block_table)
{
    rewind(fp);
    if (fwrite(file_table, sizeof(fentry), MAXFILES, fp) < MAXFILES) {
        fprintf(stderr, "Error: could not write file entries\n");
        exit(1);
    }
    if (fwrite(block_table, sizeof(fnode), MAXBLOCKS, fp) < MAXBLOCKS) {
        fprintf(stderr, "Error: could not write block nodes\n");
        exit(1);
    }
    fflush(fp);
}


/*Search Helper function*/
/* Returns index of the file with the given name, or -1 if not found. */

static int
find_file_by_name(fentry *file_table, char *target_name)
{
    int i;
    for (i = 0; i < MAXFILES; i++) {
        /* Skip empty slots (name[0] == '\0' means unused) */
        if (file_table[i].name[0] != '\0' &&
            strcmp(file_table[i].name, target_name) == 0) {
            return i;
        }
    }
    return -1;
}

/* Returns index of the first empty (fentry slot) file slot, or -1 if the table is full. */
static int
find_empty_file_slot(fentry *file_table)
{
    int i;
    for (i = 0; i < MAXFILES; i++) {
        if (file_table[i].name[0] == '\0') {
            return i;
        }
    }
    return -1;
}

/* Returns index of the first free (fnode) block, or -1 if none available.
 * A negative blockindex means that block is not in use. */
 
static int
find_free_block_slot(fnode *block_table)
{
    int i;
    for (i = 0; i < MAXBLOCKS; i++) {
        if (block_table[i].blockindex < 0) {
            return i;
        }
    }
    return -1;
}

/* Returns how many blocks are currently free. */
static int
count_available_blocks(fnode *block_table)
{
    int i, free_count = 0;
    for (i = 0; i < MAXBLOCKS; i++) {
        if (block_table[i].blockindex < 0) {
            free_count++;
        }
    }
    return free_count;
}


/* Operations Perform On File System */
/*
 * createfile - Creates a new empty file entry in the filesystem.
 * Fails if the name is too long, already exists, or no slots are available.
 */
 
void
createfile(char *fs_filename, char *new_filename)
{
    fentry file_table[MAXFILES];
    fnode  block_table[MAXBLOCKS];
    int    empty_slot;
    FILE  *fp;

    /* Max filename length is 11 chars (struct holds 12 bytes with null terminator) */
    if (strlen(new_filename) > 11) {
        fprintf(stderr, "Error: createfile: filename too long\n");
        exit(1);
    }

    fp = openfs(fs_filename, "r+b");
    load_metadata(fp, file_table, block_table);

    /* Reject duplicate filenames */
    if (find_file_by_name(file_table, new_filename) != -1) {
        fprintf(stderr, "Error: createfile: file already exists\n");
        closefs(fp);
        exit(1);
    }

    /* Find a free directory slot */
    empty_slot = find_empty_file_slot(file_table);
    if (empty_slot == -1) {
        fprintf(stderr, "Error: createfile: no free file slots\n");
        closefs(fp);
        exit(1);
    }

    /* Initialize the new file entry — size 0, no blocks allocated yet */
    strncpy(file_table[empty_slot].name, new_filename, 11);
    file_table[empty_slot].name[11]   = '\0';
    file_table[empty_slot].size       = 0;
    file_table[empty_slot].firstblock = -1;

    save_metadata(fp, file_table, block_table);
    closefs(fp);
}

/*
 * deletefile - Removes a file and frees all its data blocks.
 * Each block is zeroed out on disk before being freed (security requirement).
 */
 
void
deletefile(char *fs_filename, char *target_filename)
{
    fentry file_table[MAXFILES];
    fnode  block_table[MAXBLOCKS];
    int    file_index;
    short  current_block;
    short  next_block;
    char   zero_buffer[BLOCKSIZE];
    FILE  *fp;

    fp = openfs(fs_filename, "r+b");
    load_metadata(fp, file_table, block_table);

    file_index = find_file_by_name(file_table, target_filename);
    if (file_index == -1) {
        fprintf(stderr, "Error: deletefile: file not found\n");
        closefs(fp);
        exit(1);
    }

    memset(zero_buffer, 0, BLOCKSIZE);

    /* Walk the block chain: zero each block on disk, then mark fnode as free */
    current_block = file_table[file_index].firstblock;
    while (current_block != -1) {

        /* Overwrite this block's data with zeros */
        fseek(fp, block_table[current_block].blockindex * BLOCKSIZE, SEEK_SET);
        fwrite(zero_buffer, BLOCKSIZE, 1, fp);

        next_block = block_table[current_block].nextblock;

        /* Free the fnode — negative blockindex means not in use */
        block_table[current_block].blockindex = -current_block;
        block_table[current_block].nextblock  = -1;

        current_block = next_block;
    }

    /* Clear the file entry so the slot can be reused */
    file_table[file_index].name[0]    = '\0';
    file_table[file_index].size       = 0;
    file_table[file_index].firstblock = -1;

    save_metadata(fp, file_table, block_table);
    closefs(fp);
}


/*
 * readfile - Reads num_bytes from a file starting at start_byte, prints to stdout.
 * Handles reads that span across multiple blocks automatically.
 */
 
void
readfile(char *fs_filename, char *target_filename, int start_byte, int num_bytes)
{
    fentry file_table[MAXFILES];
    fnode  block_table[MAXBLOCKS];
    int    file_index;
    short  current_block;
    int    block_start_pos;  /* byte offset at the start of the current block */
    int    bytes_remaining;
    int    offset_in_block;  /* how far into the current block we begin reading */
    int    bytes_this_block; /* how many bytes to read from this block */
    char   read_buffer[BLOCKSIZE];
    FILE  *fp;

    fp = openfs(fs_filename, "rb");
    load_metadata(fp, file_table, block_table);

    file_index = find_file_by_name(file_table, target_filename);
    if (file_index == -1) {
        fprintf(stderr, "Error: readfile: file not found\n");
        closefs(fp);
        exit(1);
    }

    /* start_byte must be inside the file */
    if (start_byte < 0 || start_byte >= file_table[file_index].size) {
        fprintf(stderr, "Error: readfile: invalid start position\n");
        closefs(fp);
        exit(1);
    }

    /* The read range must not go past the end of the file */
    if (num_bytes < 0 || start_byte + num_bytes > file_table[file_index].size) {
        fprintf(stderr, "Error: readfile: read exceeds file size\n");
        closefs(fp);
        exit(1);
    }

    /* Skip blocks until we reach the one containing start_byte */
    current_block   = file_table[file_index].firstblock;
    block_start_pos = 0;
    while (block_start_pos + BLOCKSIZE <= start_byte) {
        current_block    = block_table[current_block].nextblock;
        block_start_pos += BLOCKSIZE;
    }

    /* Read one block at a time, writing output to stdout */
    bytes_remaining = num_bytes;
    while (bytes_remaining > 0 && current_block != -1) {

        /* offset_in_block is non-zero only on the first iteration (partial block) */
        offset_in_block  = (start_byte + (num_bytes - bytes_remaining)) - block_start_pos;
        bytes_this_block = BLOCKSIZE - offset_in_block;
        if (bytes_this_block > bytes_remaining) {
            bytes_this_block = bytes_remaining;
        }

        fseek(fp,
              block_table[current_block].blockindex * BLOCKSIZE + offset_in_block,
              SEEK_SET);
        fread(read_buffer, 1, bytes_this_block, fp);
        fwrite(read_buffer, 1, bytes_this_block, stdout);

        bytes_remaining  -= bytes_this_block;
        current_block     = block_table[current_block].nextblock;
        block_start_pos  += BLOCKSIZE;
    }

    closefs(fp);
}


/*
 * writefile - Writes num_bytes from stdin into a file starting at start_byte.
 *
 * Space is checked BEFORE any changes are made (all-or-nothing guarantee).
 * Uses ceiling division to calculate how many blocks are needed.
 */
 
void
writefile(char *fs_filename, char *target_filename, int start_byte, int num_bytes)
{
    fentry file_table[MAXFILES];
    fnode  block_table[MAXBLOCKS];
    int    file_index;
    int    current_file_size, new_file_size;
    int    blocks_currently_used, blocks_needed, blocks_to_allocate;
    short  current_block;
    short *chain_tail;          /* points to the nextblock field at end of chain */
    int    new_block_index;
    int    block_start_pos;
    int    bytes_remaining;
    int    offset_in_block;
    int    bytes_this_block;
    char   write_buffer[BLOCKSIZE];
    char   zero_buffer[BLOCKSIZE];
    int    i;
    FILE  *fp;

    fp = openfs(fs_filename, "r+b");
    load_metadata(fp, file_table, block_table);

    file_index = find_file_by_name(file_table, target_filename);
    if (file_index == -1) {
        fprintf(stderr, "Error: writefile: file not found\n");
        closefs(fp);
        exit(1);
    }

    current_file_size = file_table[file_index].size;

    /* Writing past the end would leave a gap of unknown bytes — not allowed */
    if (start_byte < 0 || start_byte > current_file_size) {
        fprintf(stderr, "Error: writefile: invalid start position (gap)\n");
        closefs(fp);
        exit(1);
    }

    /* New size is the larger of: end of this write, or existing file size */
    new_file_size = start_byte + num_bytes;
    if (new_file_size < current_file_size) {
        new_file_size = current_file_size;
    }

    /* Ceiling division: (size + BLOCKSIZE - 1) / BLOCKSIZE rounds up to whole blocks */
    blocks_currently_used = (current_file_size == 0) ? 0
                            : (current_file_size + BLOCKSIZE - 1) / BLOCKSIZE;
    blocks_needed         = (new_file_size == 0) ? 0
                            : (new_file_size + BLOCKSIZE - 1) / BLOCKSIZE;
    blocks_to_allocate    = blocks_needed - blocks_currently_used;

    /* Check space BEFORE touching anything — all-or-nothing guarantee */
    if (blocks_to_allocate > count_available_blocks(block_table)) {
        fprintf(stderr, "Error: writefile: not enough free blocks\n");
        closefs(fp);
        exit(1);
    }

    /* Walk to the end of the block chain so we can append new blocks */
    current_block = file_table[file_index].firstblock;
    chain_tail    = &file_table[file_index].firstblock;
    while (current_block != -1) {
        chain_tail    = &block_table[current_block].nextblock;
        current_block = *chain_tail;
    }

    /* Allocate new blocks, zero-initialize each, and link onto chain */
    memset(zero_buffer, 0, BLOCKSIZE);
    for (i = 0; i < blocks_to_allocate; i++) {
        new_block_index = find_free_block_slot(block_table);

        block_table[new_block_index].blockindex = new_block_index; /* mark in use */
        block_table[new_block_index].nextblock  = -1;
        *chain_tail = new_block_index;
        chain_tail  = &block_table[new_block_index].nextblock;

        /* Zero out this block on disk */
        fseek(fp, new_block_index * BLOCKSIZE, SEEK_SET);
        fwrite(zero_buffer, BLOCKSIZE, 1, fp);
    }

    /* Walk to the block containing start_byte */
    current_block   = file_table[file_index].firstblock;
    block_start_pos = 0;
    while (block_start_pos + BLOCKSIZE <= start_byte) {
        current_block    = block_table[current_block].nextblock;
        block_start_pos += BLOCKSIZE;
    }

    /* Write stdin data into the filesystem one block at a time */
    bytes_remaining = num_bytes;
    while (bytes_remaining > 0 && current_block != -1) {

        offset_in_block  = (start_byte + (num_bytes - bytes_remaining)) - block_start_pos;
        bytes_this_block = BLOCKSIZE - offset_in_block;
        if (bytes_this_block > bytes_remaining) {
            bytes_this_block = bytes_remaining;
        }

        if (fread(write_buffer, 1, bytes_this_block, stdin) < (size_t)bytes_this_block) {
            fprintf(stderr, "Error: writefile: not enough input data\n");
            closefs(fp);
            exit(1);
        }

        fseek(fp,
              block_table[current_block].blockindex * BLOCKSIZE + offset_in_block,
              SEEK_SET);
        fwrite(write_buffer, 1, bytes_this_block, fp);

        bytes_remaining  -= bytes_this_block;
        current_block     = block_table[current_block].nextblock;
        block_start_pos  += BLOCKSIZE;
    }

    /* Update file size and save everything to disk */
    file_table[file_index].size = new_file_size;
    save_metadata(fp, file_table, block_table);
    closefs(fp);
}
