#include "unmad_fs_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

_Static_assert(sizeof(superblock_t) == 512, "Superblock must be 512 bytes");
_Static_assert(sizeof(inode_t) == 64, "Inode must be 64 bytes");
_Static_assert(sizeof(dir_entry_t) == 32, "Dir entry must be 32 bytes");

static FILE *disk_fp = NULL;
static superblock_t sb;
static uint8_t bitmap[BLOCK_SIZE];
static inode_t inodes[MAX_INODES];
static uint16_t cwd_inode = 0;
static char disk_file_path[512] = DISK_FILE;

static void read_block(uint32_t blk, void *buf) {
    if (!disk_fp) return;
    fseek(disk_fp, (long)blk * BLOCK_SIZE, SEEK_SET);
    fread(buf, BLOCK_SIZE, 1, disk_fp);
}

static void write_block(uint32_t blk, const void *buf) {
    if (!disk_fp) return;
    fseek(disk_fp, (long)blk * BLOCK_SIZE, SEEK_SET);
    fwrite(buf, BLOCK_SIZE, 1, disk_fp);
    fflush(disk_fp);
}

static void sync_metadata(void) {
    if (!disk_fp) return;
    write_block(SUPERBLOCK_BLK, &sb);
    write_block(BITMAP_BLK, bitmap);
    for (int i = 0; i < 16; i++) {
        write_block(INODE_START_BLK + i, ((uint8_t *)inodes) + (i * BLOCK_SIZE));
    }
}

static void load_metadata(void) {
    if (!disk_fp) return;
    read_block(SUPERBLOCK_BLK, &sb);
    read_block(BITMAP_BLK, bitmap);
    for (int i = 0; i < 16; i++) {
        read_block(INODE_START_BLK + i, ((uint8_t *)inodes) + (i * BLOCK_SIZE));
    }
}

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
    return -1;
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
            if (new_blk == -1) return -1;
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
    return -2;
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

void fs_get_cwd_path(char *out, size_t max_len) {
    if (cwd_inode == 0) {
        snprintf(out, max_len, "/");
        return;
    }
    uint16_t chain[MAX_INODES];
    int depth = 0;
    uint16_t curr = cwd_inode;
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

void fs_format(char *out, size_t out_len) {
    uint8_t zero[BLOCK_SIZE] = {0};
    for (int i = 0; i < TOTAL_BLOCKS; i++) write_block(i, zero);
    memset(&sb, 0, sizeof(sb));
    sb.magic = FS_MAGIC;
    sb.total_blocks = TOTAL_BLOCKS;
    sb.total_inodes = MAX_INODES;
    sb.free_blocks = TOTAL_BLOCKS - DATA_START_BLK;
    sb.free_inodes = MAX_INODES - 1;
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
    inodes[0].direct[0] = alloc_block();

    cwd_inode = 0;
    sync_metadata();
    if (out) {
        snprintf(out, out_len, "Disk formatted: %d blocks (%d KB), %u free data blocks.\n",
                 TOTAL_BLOCKS, DISK_SIZE / 1024, sb.free_blocks);
    } else {
        printf("Disk formatted: %d blocks (%d KB), %u free data blocks.\n",
               TOTAL_BLOCKS, DISK_SIZE / 1024, sb.free_blocks);
    }
}

int fs_init(const char *disk_path) {
    if (disk_path && strlen(disk_path) > 0) {
        strncpy(disk_file_path, disk_path, sizeof(disk_file_path) - 1);
    }
    disk_fp = fopen(disk_file_path, "r+b");
    if (!disk_fp) {
        disk_fp = fopen(disk_file_path, "w+b");
        if (!disk_fp) return -1;
        printf("Creating new '%s' and formatting...\n", disk_file_path);
        fs_format(NULL, 0);
    } else {
        load_metadata();
        if (sb.magic != FS_MAGIC) {
            printf("Unformatted disk image detected. Formatting...\n");
            fs_format(NULL, 0);
        } else {
            printf("Mounted '%s' (Magic: UNMD, Free Blocks: %u, Free Inodes: %u).\n",
                   disk_file_path, sb.free_blocks, sb.free_inodes);
        }
    }
    return 0;
}

void fs_close(void) {
    if (disk_fp) {
        sync_metadata();
        fclose(disk_fp);
        disk_fp = NULL;
    }
}

void fs_get_block_hex(uint32_t blk, char *out_buf, size_t max_out) {
    if (blk >= TOTAL_BLOCKS) {
        snprintf(out_buf, max_out, "Error: Block %u out of bounds (max %d).\n", blk, TOTAL_BLOCKS - 1);
        return;
    }
    uint8_t buf[BLOCK_SIZE];
    read_block(blk, buf);
    size_t off = 0;
    off += snprintf(out_buf + off, max_out - off, "=== Block %u (Offset: 0x%08x) ===\n", blk, blk * BLOCK_SIZE);
    for (size_t row = 0; row < BLOCK_SIZE && off + 80 < max_out; row += 16) {
        off += snprintf(out_buf + off, max_out - off, "%08zx: ", blk * BLOCK_SIZE + row);
        for (int j = 0; j < 16; j++) {
            off += snprintf(out_buf + off, max_out - off, "%02x%s", buf[row + j], (j == 7) ? "  " : " ");
        }
        off += snprintf(out_buf + off, max_out - off, " |");
        for (int j = 0; j < 16; j++) {
            uint8_t c = buf[row + j];
            off += snprintf(out_buf + off, max_out - off, "%c", (c >= 32 && c <= 126) ? c : '.');
        }
        off += snprintf(out_buf + off, max_out - off, "|\n");
    }
}

void fs_get_state_json(char *out_buf, size_t max_out) {
    char cwd_str[256];
    fs_get_cwd_path(cwd_str, sizeof(cwd_str));
    size_t off = 0;

    off += snprintf(out_buf + off, max_out - off,
        "{\"superblock\":{\"magic\":\"0x%08X\",\"total_blocks\":%u,\"free_blocks\":%u,"
        "\"total_inodes\":%u,\"free_inodes\":%u,\"bitmap_blk\":%u,\"inode_start_blk\":%u,"
        "\"data_start_blk\":%u},\"cwd\":\"%s\",\"cwd_inode\":%u,",
        sb.magic, sb.total_blocks, sb.free_blocks,
        sb.total_inodes, sb.free_inodes, sb.bitmap_block, sb.inode_start_block,
        sb.data_start_block, cwd_str, cwd_inode);

    // Current directory entries
    off += snprintf(out_buf + off, max_out - off, "\"entries\":[");
    inode_t *dir = &inodes[cwd_inode];
    dir_entry_t entries[DIR_ENTRIES_PER_BLK];
    int first_entry = 1;
    for (int b = 0; b < DIRECT_PTRS; b++) {
        if (dir->direct[b] == 0) continue;
        read_block(dir->direct[b], entries);
        for (size_t s = 0; s < DIR_ENTRIES_PER_BLK; s++) {
            if (entries[s].used) {
                uint16_t ino = entries[s].inode_num;
                inode_t *n = &inodes[ino];
                if (!first_entry && off < max_out - 1) off += snprintf(out_buf + off, max_out - off, ",");
                off += snprintf(out_buf + off, max_out - off,
                    "{\"name\":\"%s\",\"inode\":%u,\"type\":\"%s\",\"size\":%u,\"perm\":\"0%o\",\"ctime\":%u}",
                    entries[s].name, ino, (n->used == TYPE_DIR) ? "DIR" : "FILE", n->size, n->permissions, n->ctime);
                first_entry = 0;
            }
        }
    }
    off += snprintf(out_buf + off, max_out - off, "],\"used_blocks\":[");

    // List of allocated block indices
    int first_blk = 1;
    for (int b = 0; b < TOTAL_BLOCKS; b++) {
        if (b < DATA_START_BLK || bitmap_test(b)) {
            if (!first_blk && off < max_out - 1) off += snprintf(out_buf + off, max_out - off, ",");
            off += snprintf(out_buf + off, max_out - off, "%d", b);
            first_blk = 0;
        }
    }
    off += snprintf(out_buf + off, max_out - off, "]}");
}

static void do_write(const char *name, const char *text, char *out_buf, size_t max_out) {
    int ino = dir_lookup(cwd_inode, name, NULL, NULL);
    if (ino == -1) { snprintf(out_buf, max_out, "Error: File not found.\n"); return; }
    inode_t *node = &inodes[ino];
    if (node->used == TYPE_DIR) { snprintf(out_buf, max_out, "Error: Cannot write text to a directory.\n"); return; }

    size_t len = strlen(text);
    size_t blocks_needed = (len + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (len == 0) blocks_needed = 0;
    if (blocks_needed > MAX_FILE_BLOCKS) {
        snprintf(out_buf, max_out, "Error: File exceeds max size (%zu bytes).\n", (size_t)MAX_FILE_BLOCKS * BLOCK_SIZE);
        return;
    }

    release_inode_blocks(node);
    size_t total_needed = blocks_needed + (blocks_needed > DIRECT_PTRS ? 1 : 0);
    if (total_needed > sb.free_blocks) {
        snprintf(out_buf, max_out, "Error: Disk full (need %zu blocks, %u available).\n", total_needed, sb.free_blocks);
        sync_metadata();
        return;
    }

    uint32_t ind_ptrs[INDIRECT_PTRS] = {0};
    if (blocks_needed > DIRECT_PTRS) node->indirect = alloc_block();
    for (size_t b = 0; b < blocks_needed; b++) {
        int new_blk = alloc_block();
        if (b < DIRECT_PTRS) node->direct[b] = new_blk;
        else ind_ptrs[b - DIRECT_PTRS] = new_blk;

        uint8_t blk_buf[BLOCK_SIZE] = {0};
        size_t chunk = len - (b * BLOCK_SIZE);
        if (chunk > BLOCK_SIZE) chunk = BLOCK_SIZE;
        memcpy(blk_buf, text + (b * BLOCK_SIZE), chunk);
        write_block(new_blk, blk_buf);
    }
    if (node->indirect != 0) write_block(node->indirect, ind_ptrs);
    node->size = (uint32_t)len;
    sync_metadata();
    snprintf(out_buf, max_out, "Wrote %zu bytes to '%s' across %zu block(s).\n", len, name, blocks_needed);
}

void fs_execute_command(const char *cmd_line, char *out_buf, size_t max_out) {
    if (!cmd_line || strlen(cmd_line) == 0) {
        out_buf[0] = '\0';
        return;
    }
    char buf[4096];
    strncpy(buf, cmd_line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *cmd = strtok(buf, " \t\r\n");
    if (!cmd) {
        out_buf[0] = '\0';
        return;
    }

    if (strcmp(cmd, "format") == 0) {
        fs_format(out_buf, max_out);
    } else if (strcmp(cmd, "pwd") == 0) {
        char path[256];
        fs_get_cwd_path(path, sizeof(path));
        snprintf(out_buf, max_out, "%s\n", path);
    } else if (strcmp(cmd, "ls") == 0) {
        inode_t *dir = &inodes[cwd_inode];
        dir_entry_t entries[DIR_ENTRIES_PER_BLK];
        size_t off = 0;
        off += snprintf(out_buf + off, max_out - off, "%-6s %-5s %-6s %-8s %-28s\n", "INODE", "TYPE", "PERM", "SIZE", "NAME");
        off += snprintf(out_buf + off, max_out - off, "----------------------------------------------------------\n");
        int count = 0;
        for (int b = 0; b < DIRECT_PTRS; b++) {
            if (dir->direct[b] == 0) continue;
            read_block(dir->direct[b], entries);
            for (size_t s = 0; s < DIR_ENTRIES_PER_BLK; s++) {
                if (entries[s].used) {
                    uint16_t ino = entries[s].inode_num;
                    inode_t *node = &inodes[ino];
                    off += snprintf(out_buf + off, max_out - off, "%-6u %-5s 0%-5o %-8u %-28s\n",
                                   ino, (node->used == TYPE_DIR) ? "DIR" : "FILE",
                                   node->permissions, node->size, entries[s].name);
                    count++;
                }
            }
        }
        if (count == 0) snprintf(out_buf + off, max_out - off, "(directory is empty)\n");
    } else if (strcmp(cmd, "create") == 0) {
        char *name = strtok(NULL, " \t\r\n");
        if (!name || strlen(name) == 0) { snprintf(out_buf, max_out, "Error: Missing filename.\n"); return; }
        if (strlen(name) > MAX_FILENAME) { snprintf(out_buf, max_out, "Error: Name too long (max %d chars).\n", MAX_FILENAME); return; }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { snprintf(out_buf, max_out, "Error: Invalid filename.\n"); return; }
        if (dir_lookup(cwd_inode, name, NULL, NULL) != -1) { snprintf(out_buf, max_out, "Error: File or directory already exists.\n"); return; }

        int ino = alloc_inode();
        if (ino == -1) { snprintf(out_buf, max_out, "Error: Disk full (no free inodes).\n"); return; }
        inodes[ino].used = TYPE_FILE;
        inodes[ino].permissions = 0644;
        inodes[ino].parent_inode = cwd_inode;
        inodes[ino].ctime = (uint32_t)time(NULL);

        if (dir_add(cwd_inode, name, (uint16_t)ino) != 0) {
            inodes[ino].used = TYPE_FREE;
            sb.free_inodes++;
            snprintf(out_buf, max_out, "Error: Could not add entry to directory.\n");
            return;
        }
        sync_metadata();
        snprintf(out_buf, max_out, "Created file '%s' (inode %d).\n", name, ino);
    } else if (strcmp(cmd, "mkdir") == 0) {
        char *name = strtok(NULL, " \t\r\n");
        if (!name || strlen(name) == 0) { snprintf(out_buf, max_out, "Error: Missing directory name.\n"); return; }
        if (strlen(name) > MAX_FILENAME) { snprintf(out_buf, max_out, "Error: Name too long (max %d chars).\n", MAX_FILENAME); return; }
        if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) { snprintf(out_buf, max_out, "Error: Invalid directory name.\n"); return; }
        if (dir_lookup(cwd_inode, name, NULL, NULL) != -1) { snprintf(out_buf, max_out, "Error: Name already exists.\n"); return; }
        if (sb.free_blocks < 1) { snprintf(out_buf, max_out, "Error: Disk full.\n"); return; }

        int ino = alloc_inode();
        if (ino == -1) { snprintf(out_buf, max_out, "Error: Disk full (no free inodes).\n"); return; }
        inodes[ino].used = TYPE_DIR;
        inodes[ino].permissions = 0755;
        inodes[ino].parent_inode = cwd_inode;
        inodes[ino].ctime = (uint32_t)time(NULL);
        inodes[ino].direct[0] = alloc_block();

        if (dir_add(cwd_inode, name, (uint16_t)ino) != 0) {
            release_inode_blocks(&inodes[ino]);
            inodes[ino].used = TYPE_FREE;
            sb.free_inodes++;
            snprintf(out_buf, max_out, "Error: Directory full or disk full.\n");
            return;
        }
        sync_metadata();
        snprintf(out_buf, max_out, "Created directory '%s' (inode %d).\n", name, ino);
    } else if (strcmp(cmd, "cd") == 0) {
        char *path = strtok(NULL, " \t\r\n");
        if (!path || strcmp(path, "/") == 0) {
            cwd_inode = 0;
            snprintf(out_buf, max_out, "Moved to /\n");
            return;
        }
        if (strcmp(path, ".") == 0) { snprintf(out_buf, max_out, "Current directory unchanged.\n"); return; }
        if (strcmp(path, "..") == 0) {
            cwd_inode = inodes[cwd_inode].parent_inode;
            char p[256];
            fs_get_cwd_path(p, sizeof(p));
            snprintf(out_buf, max_out, "Moved to %s\n", p);
            return;
        }
        int target_ino = dir_lookup(cwd_inode, path, NULL, NULL);
        if (target_ino == -1) { snprintf(out_buf, max_out, "Error: Directory not found.\n"); return; }
        if (inodes[target_ino].used != TYPE_DIR) { snprintf(out_buf, max_out, "Error: '%s' is not a directory.\n", path); return; }
        cwd_inode = (uint16_t)target_ino;
        char p[256];
        fs_get_cwd_path(p, sizeof(p));
        snprintf(out_buf, max_out, "Moved to %s\n", p);
    } else if (strcmp(cmd, "rmdir") == 0) {
        char *name = strtok(NULL, " \t\r\n");
        if (!name || strlen(name) == 0) { snprintf(out_buf, max_out, "Error: Missing directory name.\n"); return; }
        int target_ino = dir_lookup(cwd_inode, name, NULL, NULL);
        if (target_ino == -1) { snprintf(out_buf, max_out, "Error: Directory not found.\n"); return; }
        if (inodes[target_ino].used != TYPE_DIR) { snprintf(out_buf, max_out, "Error: '%s' is not a directory.\n", name); return; }
        if (!dir_is_empty((uint16_t)target_ino)) { snprintf(out_buf, max_out, "Error: Directory not empty.\n"); return; }

        release_inode_blocks(&inodes[target_ino]);
        memset(&inodes[target_ino], 0, sizeof(inode_t));
        sb.free_inodes++;
        dir_remove(cwd_inode, name);
        sync_metadata();
        snprintf(out_buf, max_out, "Removed directory '%s' (inode %d freed).\n", name, target_ino);
    } else if (strcmp(cmd, "write") == 0) {
        char *name = strtok(NULL, " \t\r\n");
        char *text = strtok(NULL, "");
        if (!name || !text) { snprintf(out_buf, max_out, "Usage: write <filename> <text>\n"); return; }
        while (*text == ' ') text++;
        do_write(name, text, out_buf, max_out);
    } else if (strcmp(cmd, "writefill") == 0) {
        char *name = strtok(NULL, " \t\r\n");
        char *cnt_s = strtok(NULL, " \t\r\n");
        if (!name || !cnt_s) { snprintf(out_buf, max_out, "Usage: writefill <filename> <bytes>\n"); return; }
        int n = atoi(cnt_s);
        if (n <= 0 || n > 75000) { snprintf(out_buf, max_out, "Error: Bytes must be between 1 and 75000.\n"); return; }
        char *fill = (char *)malloc(n + 1);
        if (!fill) { snprintf(out_buf, max_out, "Error: Memory allocation failed.\n"); return; }
        memset(fill, 'A', n);
        fill[n] = '\0';
        do_write(name, fill, out_buf, max_out);
        free(fill);
    } else if (strcmp(cmd, "read") == 0) {
        char *name = strtok(NULL, " \t\r\n");
        if (!name) { snprintf(out_buf, max_out, "Usage: read <filename>\n"); return; }
        int ino = dir_lookup(cwd_inode, name, NULL, NULL);
        if (ino == -1) { snprintf(out_buf, max_out, "Error: File not found.\n"); return; }
        inode_t *node = &inodes[ino];
        if (node->used == TYPE_DIR) { snprintf(out_buf, max_out, "Error: '%s' is a directory.\n", name); return; }
        if (node->size == 0) { snprintf(out_buf, max_out, "(empty file)\n"); return; }

        uint32_t ind_ptrs[INDIRECT_PTRS] = {0};
        if (node->indirect != 0) read_block(node->indirect, ind_ptrs);

        size_t remaining = node->size;
        size_t total_blks = (node->size + BLOCK_SIZE - 1) / BLOCK_SIZE;
        size_t off = 0;
        for (size_t b = 0; b < total_blks; b++) {
            uint32_t blk = (b < DIRECT_PTRS) ? node->direct[b] : ind_ptrs[b - DIRECT_PTRS];
            uint8_t blk_buf[BLOCK_SIZE];
            read_block(blk, blk_buf);
            size_t to_print = (remaining < BLOCK_SIZE) ? remaining : BLOCK_SIZE;
            for (size_t k = 0; k < to_print && off < max_out - 1; k++) {
                out_buf[off++] = blk_buf[k];
            }
            remaining -= to_print;
        }
        if (off < max_out - 1) out_buf[off++] = '\n';
        out_buf[off] = '\0';
    } else if (strcmp(cmd, "delete") == 0) {
        char *name = strtok(NULL, " \t\r\n");
        if (!name) { snprintf(out_buf, max_out, "Usage: delete <filename>\n"); return; }
        int ino = dir_lookup(cwd_inode, name, NULL, NULL);
        if (ino == -1) { snprintf(out_buf, max_out, "Error: File not found.\n"); return; }
        if (inodes[ino].used == TYPE_DIR) { snprintf(out_buf, max_out, "Error: '%s' is a directory (use rmdir instead).\n", name); return; }

        release_inode_blocks(&inodes[ino]);
        memset(&inodes[ino], 0, sizeof(inode_t));
        sb.free_inodes++;
        dir_remove(cwd_inode, name);
        sync_metadata();
        snprintf(out_buf, max_out, "Deleted '%s' (inode %d freed).\n", name, ino);
    } else if (strcmp(cmd, "info") == 0) {
        char *name = strtok(NULL, " \t\r\n");
        if (!name) { snprintf(out_buf, max_out, "Usage: info <filename>\n"); return; }
        int ino = dir_lookup(cwd_inode, name, NULL, NULL);
        if (ino == -1) { snprintf(out_buf, max_out, "Error: '%s' not found in current directory.\n", name); return; }
        inode_t *node = &inodes[ino];
        time_t raw_time = (time_t)node->ctime;
        char tbuf[32];
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&raw_time));

        size_t off = 0;
        off += snprintf(out_buf + off, max_out - off, "Name            : %s\n", name);
        off += snprintf(out_buf + off, max_out - off, "Inode Number    : %d\n", ino);
        off += snprintf(out_buf + off, max_out - off, "Parent Inode    : %u\n", node->parent_inode);
        off += snprintf(out_buf + off, max_out - off, "Type            : %s\n", (node->used == TYPE_DIR) ? "Directory" : "Regular File");
        off += snprintf(out_buf + off, max_out - off, "Size            : %u bytes\n", node->size);
        off += snprintf(out_buf + off, max_out - off, "Permissions     : 0%o\n", node->permissions);
        off += snprintf(out_buf + off, max_out - off, "Created         : %s\n", tbuf);
        off += snprintf(out_buf + off, max_out - off, "Direct Blocks   : ");
        for (int i = 0; i < DIRECT_PTRS; i++) {
            if (node->direct[i]) off += snprintf(out_buf + off, max_out - off, "%u ", node->direct[i]);
        }
        off += snprintf(out_buf + off, max_out - off, "\n");
        if (node->indirect) {
            off += snprintf(out_buf + off, max_out - off, "Indirect Block  : %u -> [ ", node->indirect);
            uint32_t ind_ptrs[INDIRECT_PTRS] = {0};
            read_block(node->indirect, ind_ptrs);
            for (size_t i = 0; i < INDIRECT_PTRS; i++) {
                if (ind_ptrs[i]) off += snprintf(out_buf + off, max_out - off, "%u ", ind_ptrs[i]);
            }
            off += snprintf(out_buf + off, max_out - off, "]\n");
        }
    } else if (strcmp(cmd, "df") == 0) {
        uint32_t used_blocks = sb.total_blocks - sb.free_blocks;
        snprintf(out_buf, max_out,
                 "Filesystem   512B-blocks  Used   Available  Use%%  Free-Inodes\n"
                 "disk.img     %-12u %-6u %-10u %3u%%  %u/%u\n",
                 sb.total_blocks, used_blocks, sb.free_blocks,
                 (used_blocks * 100) / sb.total_blocks,
                 sb.free_inodes, sb.total_inodes);
    } else if (strcmp(cmd, "map") == 0) {
        size_t off = 0;
        off += snprintf(out_buf + off, max_out - off, "Block Map (first 256 blocks: S=System Metadata, #=Used Data, .=Free):\n");
        for (int b = 0; b < 256; b++) {
            if (b % 64 == 0) off += snprintf(out_buf + off, max_out - off, "Blk %04d: ", b);
            if (b < DATA_START_BLK) out_buf[off++] = 'S';
            else if (bitmap_test(b)) out_buf[off++] = '#';
            else out_buf[off++] = '.';
            if (b % 64 == 63) out_buf[off++] = '\n';
        }
        out_buf[off] = '\0';
    } else if (strcmp(cmd, "hex") == 0) {
        char *blk_s = strtok(NULL, " \t\r\n");
        uint32_t blk = blk_s ? (uint32_t)atoi(blk_s) : 0;
        fs_get_block_hex(blk, out_buf, max_out);
    } else {
        snprintf(out_buf, max_out, "Commands: format, create, write, writefill, read, ls, delete, info, mkdir, cd, pwd, rmdir, df, map, hex, exit\n");
    }
}
