#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define DISK_FILE "disk.img"
#define DISK_SIZE (1024 * 1024) // 1 MB
#define BLOCK_SIZE 512
#define TOTAL_BLOCKS (DISK_SIZE / BLOCK_SIZE) // 2048 blocks
#define MAX_INODES 128
#define MAX_FILENAME 27
#define DIRECT_PTRS 10
#define INDIRECT_PTRS (BLOCK_SIZE / (int)sizeof(uint32_t)) // 128 pointers
#define MAX_FILE_BLOCKS (DIRECT_PTRS + INDIRECT_PTRS) // 138 blocks (70,656 B)
#define DIR_ENTRIES_PER_BLK (BLOCK_SIZE / (int)sizeof(dir_entry_t)) // 16 entries per block
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

// Compile-time struct verification
_Static_assert(sizeof(superblock_t) == 512, "Superblock must be 512 bytes");
_Static_assert(sizeof(inode_t) == 64, "Inode must be 64 bytes");
_Static_assert(sizeof(dir_entry_t) == 32, "Dir entry must be 32 bytes");

static FILE *disk_fp = NULL;
static superblock_t sb;
static uint8_t bitmap[BLOCK_SIZE];
static inode_t inodes[MAX_INODES];
static uint16_t cwd_inode = 0; // Current working directory (0 = root)

// --- Low-Level Block I/O ---
static void read_block(uint32_t blk, void *buf) {
    fseek(disk_fp, (long)blk * BLOCK_SIZE, SEEK_SET);
    fread(buf, BLOCK_SIZE, 1, disk_fp);
}

static void write_block(uint32_t blk, const void *buf) {
    fseek(disk_fp, (long)blk * BLOCK_SIZE, SEEK_SET);
    fwrite(buf, BLOCK_SIZE, 1, disk_fp);
    fflush(disk_fp);
}

static void sync_metadata(void) {
    write_block(SUPERBLOCK_BLK, &sb);
    write_block(BITMAP_BLK, bitmap);
    for (int i = 0; i < 16; i++) {
        write_block(INODE_START_BLK + i, ((uint8_t *)inodes) + (i * BLOCK_SIZE));
    }
}

static void load_metadata(void) {
    read_block(SUPERBLOCK_BLK, &sb);
    read_block(BITMAP_BLK, bitmap);
    for (int i = 0; i < 16; i++) {
        read_block(INODE_START_BLK + i, ((uint8_t *)inodes) + (i * BLOCK_SIZE));
    }
}

// --- Bitmap Bitwise Operations ---
static void bitmap_set(int blk) { bitmap[blk / 8] |= (1 << (blk % 8)); }
static void bitmap_clear(int blk) { bitmap[blk / 8] &= ~(1 << (blk % 8)); }
static int bitmap_test(int blk) { return (bitmap[blk / 8] >> (blk % 8)) & 1; }

static int alloc_block(void) {
    for (int b = DATA_START_BLK; b < TOTAL_BLOCKS; b++) {
        if (!bitmap_test(b)) {
            bitmap_set(b);
            sb.free_blocks--;
            uint8_t zero[BLOCK_SIZE] = {0};
            write_block(b, zero);
            return b;
        }
    }
    return -1; // Disk full
}

static void free_block(uint32_t b) {
    if (b >= DATA_START_BLK && b < TOTAL_BLOCKS && bitmap_test(b)) {
        bitmap_clear(b);
        sb.free_blocks++;
    }
}

static int alloc_inode(void) {
    if (sb.free_inodes == 0) return -1;
    for (int i = 1; i < MAX_INODES; i++) {
        if (inodes[i].used == TYPE_FREE) {
            memset(&inodes[i], 0, sizeof(inode_t));
            sb.free_inodes--;
            return i;
        }
    }
    return -1;
}

static void release_inode_blocks(inode_t *node) {
    for (int i = 0; i < DIRECT_PTRS; i++) {
        if (node->direct[i] != 0) {
            free_block(node->direct[i]);
            node->direct[i] = 0;
        }
    }
    if (node->indirect != 0) {
        uint32_t ind_ptrs[INDIRECT_PTRS] = {0};
        read_block(node->indirect, ind_ptrs);
        for (size_t i = 0; i < INDIRECT_PTRS; i++) {
            if (ind_ptrs[i] != 0) free_block(ind_ptrs[i]);
        }
        free_block(node->indirect);
        node->indirect = 0;
    }
    node->size = 0;
}

// --- Hierarchical Directory Helpers ---
static int dir_lookup(uint16_t dir_ino, const char *name, int *out_blk_idx, int *out_slot) {
    inode_t *dir = &inodes[dir_ino];
    dir_entry_t entries[DIR_ENTRIES_PER_BLK];
    for (int b = 0; b < DIRECT_PTRS; b++) {
        if (dir->direct[b] == 0) continue;
        read_block(dir->direct[b], entries);
        for (size_t s = 0; s < DIR_ENTRIES_PER_BLK; s++) {
            if (entries[s].used && strcmp(entries[s].name, name) == 0) {
                if (out_blk_idx) *out_blk_idx = b;
                if (out_slot) *out_slot = (int)s;
                return entries[s].inode_num;
            }
        }
    }
    return -1;
}

static int dir_add(uint16_t dir_ino, const char *name, uint16_t child_ino) {
    inode_t *dir = &inodes[dir_ino];
    dir_entry_t entries[DIR_ENTRIES_PER_BLK];
    for (int b = 0; b < DIRECT_PTRS; b++) {
        if (dir->direct[b] == 0) {
            int new_blk = alloc_block();
            if (new_blk == -1) return -1; // Disk full
            dir->direct[b] = new_blk;
        }
        read_block(dir->direct[b], entries);
        for (size_t s = 0; s < DIR_ENTRIES_PER_BLK; s++) {
            if (!entries[s].used) {
                entries[s].used = 1;
                entries[s].inode_num = child_ino;
                strncpy(entries[s].name, name, MAX_FILENAME);
                entries[s].name[MAX_FILENAME] = '\0';
                write_block(dir->direct[b], entries);
                dir->size += sizeof(dir_entry_t);
                return 0;
            }
        }
    }
    return -2; // Directory full
}

static int dir_remove(uint16_t dir_ino, const char *name) {
    int b = -1, s = -1;
    if (dir_lookup(dir_ino, name, &b, &s) == -1) return -1;
    inode_t *dir = &inodes[dir_ino];
    dir_entry_t entries[DIR_ENTRIES_PER_BLK];
    read_block(dir->direct[b], entries);
    memset(&entries[s], 0, sizeof(dir_entry_t));
    write_block(dir->direct[b], entries);
    if (dir->size >= sizeof(dir_entry_t)) dir->size -= sizeof(dir_entry_t);
    return 0;
}

static int dir_is_empty(uint16_t dir_ino) {
    inode_t *dir = &inodes[dir_ino];
    dir_entry_t entries[DIR_ENTRIES_PER_BLK];
    for (int b = 0; b < DIRECT_PTRS; b++) {
        if (dir->direct[b] == 0) continue;
        read_block(dir->direct[b], entries);
        for (size_t s = 0; s < DIR_ENTRIES_PER_BLK; s++) {
            if (entries[s].used) return 0;
        }
    }
    return 1;
}

static void get_pwd_path(uint16_t ino, char *out, size_t max_len) {
    if (ino == 0) {
        snprintf(out, max_len, "/");
        return;
    }
    uint16_t chain[MAX_INODES];
    int depth = 0;
    uint16_t curr = ino;
    while (curr != 0 && depth < MAX_INODES) {
        chain[depth++] = curr;
        curr = inodes[curr].parent_inode;
    }
    out[0] = '\0';
    for (int i = depth - 1; i >= 0; i--) {
        uint16_t parent = inodes[chain[i]].parent_inode;
        inode_t *pdir = &inodes[parent];
        dir_entry_t entries[DIR_ENTRIES_PER_BLK];
        char seg[MAX_FILENAME + 1] = "?";
        for (int b = 0; b < DIRECT_PTRS; b++) {
            if (pdir->direct[b] == 0) continue;
            read_block(pdir->direct[b], entries);
            for (size_t s = 0; s < DIR_ENTRIES_PER_BLK; s++) {
                if (entries[s].used && entries[s].inode_num == chain[i]) {
                    strncpy(seg, entries[s].name, MAX_FILENAME);
                    seg[MAX_FILENAME] = '\0';
                    break;
                }
            }
        }
        strncat(out, "/", max_len - strlen(out) - 1);
        strncat(out, seg, max_len - strlen(out) - 1);
    }
}

// --- Commands ---
static void cmd_format(void) {
    uint8_t zero[BLOCK_SIZE] = {0};
    for (int i = 0; i < TOTAL_BLOCKS; i++) write_block(i, zero);
    memset(&sb, 0, sizeof(sb));
    sb.magic = FS_MAGIC;
    sb.total_blocks = TOTAL_BLOCKS;
    sb.total_inodes = MAX_INODES;
    sb.free_blocks = TOTAL_BLOCKS - DATA_START_BLK;
    sb.free_inodes = MAX_INODES - 1; // Inode 0 reserved for "/"
    sb.bitmap_block = BITMAP_BLK;
    sb.inode_start_block = INODE_START_BLK;
    sb.root_inode = 0;
    sb.data_start_block = DATA_START_BLK;

    memset(bitmap, 0, sizeof(bitmap));
    for (int b = 0; b < DATA_START_BLK; b++) bitmap_set(b);

    memset(inodes, 0, sizeof(inodes));
    inodes[0].used = TYPE_DIR;
    inodes[0].permissions = 0755;
    inodes[0].parent_inode = 0;
    inodes[0].ctime = (uint32_t)time(NULL);
    inodes[0].direct[0] = alloc_block(); // Pre-allocate 1st block for root dir

    cwd_inode = 0;
    sync_metadata();
    printf("Disk formatted: %d blocks (%d KB), %u free data blocks.\n",
           TOTAL_BLOCKS, DISK_SIZE / 1024, sb.free_blocks);
}

static void cmd_create(const char *name) {
    if (!name || strlen(name) == 0) { printf("Error: Missing filename.\n"); return; }
    if (strlen(name) > MAX_FILENAME) { printf("Error: Name too long (max %d chars).\n", MAX_FILENAME); return; }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { printf("Error: Invalid filename.\n"); return; }
    if (dir_lookup(cwd_inode, name, NULL, NULL) != -1) { printf("Error: File or directory already exists.\n"); return; }

    int ino = alloc_inode();
    if (ino == -1) { printf("Error: Disk full (no free inodes).\n"); return; }

    inodes[ino].used = TYPE_FILE;
    inodes[ino].permissions = 0644;
    inodes[ino].parent_inode = cwd_inode;
    inodes[ino].ctime = (uint32_t)time(NULL);

    if (dir_add(cwd_inode, name, (uint16_t)ino) != 0) {
        inodes[ino].used = TYPE_FREE;
        sb.free_inodes++;
        printf("Error: Could not add entry to directory (disk or directory full).\n");
        return;
    }
    sync_metadata();
    printf("Created file '%s' (inode %d).\n", name, ino);
}

static void cmd_mkdir(const char *name) {
    if (!name || strlen(name) == 0) { printf("Error: Missing directory name.\n"); return; }
    if (strlen(name) > MAX_FILENAME) { printf("Error: Name too long (max %d chars).\n", MAX_FILENAME); return; }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { printf("Error: Invalid directory name.\n"); return; }
    if (dir_lookup(cwd_inode, name, NULL, NULL) != -1) { printf("Error: Name already exists.\n"); return; }
    if (sb.free_blocks < 1) { printf("Error: Disk full.\n"); return; }

    int ino = alloc_inode();
    if (ino == -1) { printf("Error: Disk full (no free inodes).\n"); return; }

    inodes[ino].used = TYPE_DIR;
    inodes[ino].permissions = 0755;
    inodes[ino].parent_inode = cwd_inode;
    inodes[ino].ctime = (uint32_t)time(NULL);
    inodes[ino].direct[0] = alloc_block();

    if (dir_add(cwd_inode, name, (uint16_t)ino) != 0) {
        release_inode_blocks(&inodes[ino]);
        inodes[ino].used = TYPE_FREE;
        sb.free_inodes++;
        printf("Error: Directory full or disk full.\n");
        return;
    }
    sync_metadata();
    printf("Created directory '%s' (inode %d).\n", name, ino);
}

static void cmd_cd(const char *path) {
    if (!path || strcmp(path, "/") == 0) {
        cwd_inode = 0;
        return;
    }
    if (strcmp(path, ".") == 0) return;
    if (strcmp(path, "..") == 0) {
        cwd_inode = inodes[cwd_inode].parent_inode;
        return;
    }
    int target_ino = dir_lookup(cwd_inode, path, NULL, NULL);
    if (target_ino == -1) {
        printf("Error: Directory not found.\n");
        return;
    }
    if (inodes[target_ino].used != TYPE_DIR) {
        printf("Error: '%s' is not a directory.\n", path);
        return;
    }
    cwd_inode = (uint16_t)target_ino;
}

static void cmd_pwd(void) {
    char path[512];
    get_pwd_path(cwd_inode, path, sizeof(path));
    printf("%s\n", path);
}

static void cmd_rmdir(const char *name) {
    if (!name || strlen(name) == 0) { printf("Error: Missing directory name.\n"); return; }
    int target_ino = dir_lookup(cwd_inode, name, NULL, NULL);
    if (target_ino == -1) { printf("Error: Directory not found.\n"); return; }
    if (inodes[target_ino].used != TYPE_DIR) { printf("Error: '%s' is not a directory.\n", name); return; }
    if (!dir_is_empty((uint16_t)target_ino)) { printf("Error: Directory not empty.\n"); return; }

    release_inode_blocks(&inodes[target_ino]);
    memset(&inodes[target_ino], 0, sizeof(inode_t));
    sb.free_inodes++;
    dir_remove(cwd_inode, name);
    sync_metadata();
    printf("Removed directory '%s' (inode %d freed).\n", name, target_ino);
}

static void cmd_write(const char *name, const char *data) {
    int ino = dir_lookup(cwd_inode, name, NULL, NULL);
    if (ino == -1) { printf("Error: File not found.\n"); return; }
    inode_t *node = &inodes[ino];
    if (node->used == TYPE_DIR) { printf("Error: Cannot write text to a directory.\n"); return; }

    size_t len = strlen(data);
    size_t blocks_needed = (len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (len == 0) blocks_needed = 0;
    if (blocks_needed > MAX_FILE_BLOCKS) {
        printf("Error: File exceeds max size (%zu bytes).\n", (size_t)MAX_FILE_BLOCKS * BLOCK_SIZE);
        return;
    }

    release_inode_blocks(node);
    size_t total_needed = blocks_needed + (blocks_needed > DIRECT_PTRS ? 1 : 0);
    if (total_needed > sb.free_blocks) {
        printf("Error: Disk full (need %zu blocks, %u available).\n", total_needed, sb.free_blocks);
        sync_metadata();
        return;
    }

    uint32_t ind_ptrs[INDIRECT_PTRS] = {0};
    if (blocks_needed > DIRECT_PTRS) node->indirect = alloc_block();

    for (size_t b = 0; b < blocks_needed; b++) {
        int new_blk = alloc_block();
        if (b < DIRECT_PTRS) node->direct[b] = new_blk;
        else ind_ptrs[b - DIRECT_PTRS] = new_blk;

        uint8_t buf[BLOCK_SIZE] = {0};
        size_t chunk = len - (b * BLOCK_SIZE);
        if (chunk > BLOCK_SIZE) chunk = BLOCK_SIZE;
        memcpy(buf, data + (b * BLOCK_SIZE), chunk);
        write_block(new_blk, buf);
    }

    if (node->indirect != 0) write_block(node->indirect, ind_ptrs);
    node->size = (uint32_t)len;
    sync_metadata();
    printf("Wrote %zu bytes to '%s' across %zu block(s).\n", len, name, blocks_needed);
}

static void cmd_read(const char *name) {
    int ino = dir_lookup(cwd_inode, name, NULL, NULL);
    if (ino == -1) { printf("Error: File not found.\n"); return; }
    inode_t *node = &inodes[ino];
    if (node->used == TYPE_DIR) { printf("Error: '%s' is a directory.\n", name); return; }
    if (node->size == 0) { printf("(empty file)\n"); return; }

    uint32_t ind_ptrs[INDIRECT_PTRS] = {0};
    if (node->indirect != 0) read_block(node->indirect, ind_ptrs);

    size_t remaining = node->size;
    size_t total_blks = (node->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (size_t b = 0; b < total_blks; b++) {
        uint32_t blk = (b < DIRECT_PTRS) ? node->direct[b] : ind_ptrs[b - DIRECT_PTRS];
        uint8_t buf[BLOCK_SIZE];
        read_block(blk, buf);
        size_t to_print = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;
        fwrite(buf, 1, to_print, stdout);
        remaining -= to_print;
    }
    printf("\n");
}

static void cmd_delete(const char *name) {
    if (!name || strlen(name) == 0) { printf("Error: Missing filename.\n"); return; }
    int ino = dir_lookup(cwd_inode, name, NULL, NULL);
    if (ino == -1) { printf("Error: File not found.\n"); return; }
    if (inodes[ino].used == TYPE_DIR) {
        printf("Error: '%s' is a directory (use rmdir instead).\n", name);
        return;
    }
    release_inode_blocks(&inodes[ino]);
    memset(&inodes[ino], 0, sizeof(inode_t));
    sb.free_inodes++;
    dir_remove(cwd_inode, name);
    sync_metadata();
    printf("Deleted '%s' (inode %d freed).\n", name, ino);
}

static void cmd_ls(void) {
    inode_t *dir = &inodes[cwd_inode];
    dir_entry_t entries[DIR_ENTRIES_PER_BLK];
    printf("%-6s %-5s %-6s %-8s %-28s\n", "INODE", "TYPE", "PERM", "SIZE", "NAME");
    printf("----------------------------------------------------------\n");
    int count = 0;
    for (int b = 0; b < DIRECT_PTRS; b++) {
        if (dir->direct[b] == 0) continue;
        read_block(dir->direct[b], entries);
        for (size_t s = 0; s < DIR_ENTRIES_PER_BLK; s++) {
            if (entries[s].used) {
                uint16_t ino = entries[s].inode_num;
                inode_t *node = &inodes[ino];
                printf("%-6u %-5s 0%-5o %-8u %-28s\n",
                       ino,
                       (node->used == TYPE_DIR) ? "DIR" : "FILE",
                       node->permissions,
                       node->size,
                       entries[s].name);
                count++;
            }
        }
    }
    if (count == 0) printf("(directory is empty)\n");
}

static void cmd_info(const char *name) {
    int ino = dir_lookup(cwd_inode, name, NULL, NULL);
    if (ino == -1) { printf("Error: '%s' not found in current directory.\n", name ? name : ""); return; }
    inode_t *node = &inodes[ino];
    time_t raw_time = (time_t)node->ctime;
    char tbuf[32];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&raw_time));

    printf("Name            : %s\n", name);
    printf("Inode Number    : %d\n", ino);
    printf("Parent Inode    : %u\n", node->parent_inode);
    printf("Type            : %s\n", (node->used == TYPE_DIR) ? "Directory" : "Regular File");
    printf("Size            : %u bytes\n", node->size);
    printf("Permissions     : 0%o\n", node->permissions);
    printf("Created         : %s\n", tbuf);
    printf("Direct Blocks   : ");
    for (int i = 0; i < DIRECT_PTRS; i++) {
        if (node->direct[i]) printf("%u ", node->direct[i]);
    }
    printf("\n");
    if (node->indirect) {
        printf("Indirect Block  : %u -> [ ", node->indirect);
        uint32_t ind_ptrs[INDIRECT_PTRS] = {0};
        read_block(node->indirect, ind_ptrs);
        for (size_t i = 0; i < INDIRECT_PTRS; i++) {
            if (ind_ptrs[i]) printf("%u ", ind_ptrs[i]);
        }
        printf("]\n");
    }
}

static void cmd_df(void) {
    uint32_t used_blocks = sb.total_blocks - sb.free_blocks;
    printf("Filesystem   512B-blocks  Used   Available  Use%%  Free-Inodes\n");
    printf("disk.img     %-12u %-6u %-10u %3u%%  %u/%u\n",
           sb.total_blocks, used_blocks, sb.free_blocks,
           (used_blocks * 100) / sb.total_blocks,
           sb.free_inodes, sb.total_inodes);
}

static void cmd_map(void) {
    printf("Block Map (first 256 blocks: S=System Metadata, #=Used Data, .=Free):\n");
    for (int b = 0; b < 256; b++) {
        if (b % 64 == 0) printf("Blk %04d: ", b);
        if (b < DATA_START_BLK) putchar('S');
        else if (bitmap_test(b)) putchar('#');
        else putchar('.');
        if (b % 64 == 63) putchar('\n');
    }
}

int main(void) {
    disk_fp = fopen(DISK_FILE, "r+b");
    if (!disk_fp) {
        disk_fp = fopen(DISK_FILE, "w+b");
        if (!disk_fp) { perror("Failed to open disk.img"); return 1; }
        printf("Creating new '%s' and formatting...\n", DISK_FILE);
        cmd_format();
    } else {
        load_metadata();
        if (sb.magic != FS_MAGIC) {
            printf("Unformatted disk image detected. Formatting...\n");
            cmd_format();
        } else {
            printf("Mounted '%s' (Magic: UNMD, Free Blocks: %u, Free Inodes: %u).\n",
                   DISK_FILE, sb.free_blocks, sb.free_inodes);
        }
    }

    char line[8192];
    char prompt_path[256];
    while (1) {
        get_pwd_path(cwd_inode, prompt_path, sizeof(prompt_path));
        printf("unmad-fs:%s> ", prompt_path);
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\r\n")] = '\0';

        char *cmd = strtok(line, " ");
        if (!cmd) continue;

        if (strcmp(cmd, "exit") == 0 || strcmp(cmd, "quit") == 0) {
            sync_metadata();
            break;
        } else if (strcmp(cmd, "format") == 0) {
            cmd_format();
        } else if (strcmp(cmd, "ls") == 0) {
            cmd_ls();
        } else if (strcmp(cmd, "pwd") == 0) {
            cmd_pwd();
        } else if (strcmp(cmd, "df") == 0) {
            cmd_df();
        } else if (strcmp(cmd, "map") == 0) {
            cmd_map();
        } else if (strcmp(cmd, "create") == 0) {
            cmd_create(strtok(NULL, " "));
        } else if (strcmp(cmd, "mkdir") == 0) {
            cmd_mkdir(strtok(NULL, " "));
        } else if (strcmp(cmd, "cd") == 0) {
            cmd_cd(strtok(NULL, " "));
        } else if (strcmp(cmd, "rmdir") == 0) {
            cmd_rmdir(strtok(NULL, " "));
        } else if (strcmp(cmd, "read") == 0) {
            cmd_read(strtok(NULL, " "));
        } else if (strcmp(cmd, "delete") == 0) {
            cmd_delete(strtok(NULL, " "));
        } else if (strcmp(cmd, "info") == 0) {
            cmd_info(strtok(NULL, " "));
        } else if (strcmp(cmd, "write") == 0) {
            char *name = strtok(NULL, " ");
            char *text = strtok(NULL, "");
            if (!name || !text) printf("Usage: write <filename> <text>\n");
            else {
                while (*text == ' ') text++;
                cmd_write(name, text);
            }
        } else if (strcmp(cmd, "writefill") == 0) {
            char *name = strtok(NULL, " ");
            char *cnt_s = strtok(NULL, " ");
            if (!name || !cnt_s) {
                printf("Usage: writefill <filename> <bytes>\n");
            } else {
                int n = atoi(cnt_s);
                if (n > 0 && n <= 75000) {
                    char *buf = malloc(n + 1);
                    if (buf) {
                        memset(buf, 'A', n);
                        buf[n] = '\0';
                        cmd_write(name, buf);
                        free(buf);
                    }
                }
            }
        } else {
            printf("Commands: format, create, write, writefill, read, ls, delete, info, mkdir, cd, pwd, rmdir, df, map, exit\n");
        }
    }

    fclose(disk_fp);
    return 0;
}
