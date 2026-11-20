#ifndef UNMAD_FS_CORE_H
#define UNMAD_FS_CORE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DISK_FILE "disk.img"
#define DISK_SIZE (1024 * 1024) // 1 MB
#define BLOCK_SIZE 512
#define TOTAL_BLOCKS (DISK_SIZE / BLOCK_SIZE) // 2048 blocks
#define MAX_INODES 128
#define MAX_FILENAME 27
#define DIRECT_PTRS 10
#define INDIRECT_PTRS (BLOCK_SIZE / (int)sizeof(uint32_t)) // 128 pointers
#define MAX_FILE_BLOCKS (DIRECT_PTRS + INDIRECT_PTRS) // 138 blocks (70,656 B)
#define DIR_ENTRIES_PER_BLK (BLOCK_SIZE / 32) // 16 entries per block
#define MAX_DIR_ENTRIES (DIRECT_PTRS * DIR_ENTRIES_PER_BLK) // 160 entries per dir
#define FS_MAGIC 0x554E4D44 // "UNMD" in ASCII

#define SUPERBLOCK_BLK 0
#define BITMAP_BLK 1
#define INODE_START_BLK 2 // 16 blocks (2..17)
#define DATA_START_BLK 18 // 2030 blocks (18..2047)

#define TYPE_FREE 0
#define TYPE_FILE 1
#define TYPE_DIR 2

// 1. Superblock (512 bytes)
typedef struct {
    uint32_t magic;
    uint32_t total_blocks;
    uint32_t total_inodes;
    uint32_t free_blocks;
    uint32_t free_inodes;
    uint32_t bitmap_block;
    uint32_t inode_start_block;
    uint32_t root_inode;
    uint32_t data_start_block;
    uint8_t reserved[476];
} __attribute__((packed)) superblock_t;

// 2. Inode (64 bytes)
typedef struct {
    uint8_t used; // 0 = free, 1 = file, 2 = directory
    uint8_t reserved1;
    uint16_t permissions; // e.g. 0644 or 0755
    uint16_t parent_inode; // Inode of parent directory (0 for root)
    uint16_t reserved2;
    uint32_t size; // Bytes for files, entry count * 32 for dirs
    uint32_t ctime; // Creation time (epoch)
    uint32_t direct[DIRECT_PTRS]; // 10 direct block pointers
    uint32_t indirect; // 1 single-indirect block pointer
    uint32_t reserved3; // Pad to 64 bytes
} __attribute__((packed)) inode_t;

// 3. Directory Entry (32 bytes)
typedef struct {
    uint8_t used; // 0 = free, 1 = active
    uint16_t inode_num; // 0..127
    char name[MAX_FILENAME + 1]; // Null-terminated name (28 bytes)
    uint8_t reserved; // Pad to 32 bytes
} __attribute__((packed)) dir_entry_t;

// Core Engine API
int fs_init(const char *disk_path);
void fs_close(void);
void fs_format(char *out, size_t out_len);
void fs_execute_command(const char *cmd_line, char *out_buf, size_t max_out);
void fs_get_state_json(char *out_buf, size_t max_out);
void fs_get_block_hex(uint32_t blk, char *out_buf, size_t max_out);
void fs_get_cwd_path(char *out_buf, size_t max_out);

#ifdef __cplusplus
}
#endif

#endif // UNMAD_FS_CORE_H
