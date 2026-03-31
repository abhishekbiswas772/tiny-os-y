#include "vfs.h"
#include "kfs.h"
#include "../../drivers/screen/screen.h"
#include "../libc/string.h"
#include "../libc/mem.h"

/* ── Path utilities ──────────────────────────────────────────────────────── */

/* Normalise path to absolute form, collapsing duplicate/trailing slashes. */
static int normalize_path(const char *in, char *out, int out_sz) {
    int i = 0, j = 0;

    if (!in || !in[0] || out_sz < 2) return -1;

    if (in[0] != '/') out[j++] = '/';

    while (in[i] && j < out_sz - 1) {
        char c = in[i++];
        if (c == '/' && j > 0 && out[j - 1] == '/') continue;
        out[j++] = c;
    }

    if (j > 1 && out[j - 1] == '/') j--;
    out[j] = '\0';
    return 0;
}

/* Walk an absolute path and return its inode, or -1 if not found. */
static int resolve(const char *path) {
    char norm[128];
    if (normalize_path(path, norm, sizeof(norm)) != 0) return -1;

    int cur = KFS_ROOT_INODE;
    char token[KFS_NAME_MAX + 1];
    int i = 1;  /* skip leading '/' */

    while (norm[i]) {
        int j = 0;
        while (norm[i] && norm[i] != '/' && j < KFS_NAME_MAX)
            token[j++] = norm[i++];
        token[j] = '\0';

        if (norm[i] == '/') i++;  /* skip separator */
        if (j == 0) continue;     /* skip double slashes */

        cur = kfs_dir_lookup((u32)cur, token);
        if (cur == -1) return -1;
    }
    return cur;
}

/* Split `path` into parent inode and the final component (basename).
 * Fills `out_name` (must have room for KFS_NAME_MAX+1 bytes).
 * Returns parent inode or -1. */
static int resolve_parent(const char *path, char *out_name) {
    char norm[128];
    if (normalize_path(path, norm, sizeof(norm)) != 0) return -1;
    if (strcmp(norm, "/") == 0) return -1;  /* root has no parent name */

    /* Find last slash */
    int last = 0, i = 0;
    while (norm[i]) { if (norm[i] == '/') last = i; i++; }

    /* basename = everything after last slash */
    int j = 0;
    for (i = last + 1; norm[i] && j < KFS_NAME_MAX; i++)
        out_name[j++] = norm[i];
    out_name[j] = '\0';
    if (out_name[0] == '\0') return -1;

    /* parent path */
    char parent[128];
    if (last == 0) {
        parent[0] = '/'; parent[1] = '\0';
    } else {
        for (i = 0; i < last; i++) parent[i] = norm[i];
        parent[last] = '\0';
    }

    return resolve(parent);
}

/* ── VFS API ─────────────────────────────────────────────────────────────── */

void vfs_ls(const char *path) {
    int ino = resolve(path);
    if (ino == -1) { kprint("ls: path not found\n"); return; }

    kfs_inode_t inode;
    kfs_read_inode((u32)ino, &inode);
    if (inode.type != KFS_TYPE_DIR) { kprint("ls: not a directory\n"); return; }
    kfs_dir_list((u32)ino);
}

void vfs_cat(const char *path) {
    int ino = resolve(path);
    if (ino == -1) { kprint("cat: file not found\n"); return; }

    kfs_inode_t inode;
    kfs_read_inode((u32)ino, &inode);
    if (inode.type != KFS_TYPE_FILE) { kprint("cat: not a file\n"); return; }
    if (inode.size == 0) return;

    /* Read up to KFS_DIRECT_BLOCKS blocks and print each chunk */
    u8 buf[KFS_BLOCK_SIZE + 1];  /* +1 for null terminator */
    u32 remaining = inode.size;
    u32 offset    = 0;

    while (remaining > 0) {
        u32 chunk = (remaining > KFS_BLOCK_SIZE) ? KFS_BLOCK_SIZE : remaining;
        int got   = kfs_read_file((u32)ino, offset, buf, chunk);
        if (got <= 0) break;
        buf[got] = '\0';
        kprint((char *)buf);
        offset    += (u32)got;
        remaining -= (u32)got;
    }
    kprint("\n");
}

int vfs_touch(const char *path) {
    char name[KFS_NAME_MAX + 1];
    int parent = resolve_parent(path, name);
    if (parent == -1) { kprint("touch: parent not found\n"); return -1; }
    if (name[0] == '\0') { kprint("touch: invalid name\n"); return -1; }

    int r = kfs_create_file((u32)parent, name);
    if (r == -1) { kprint("touch: failed (exists or disk full)\n"); return -1; }
    return 0;
}

int vfs_mkdir(const char *path) {
    char name[KFS_NAME_MAX + 1];
    int parent = resolve_parent(path, name);
    if (parent == -1) { kprint("mkdir: parent not found\n"); return -1; }
    if (name[0] == '\0') { kprint("mkdir: invalid name\n"); return -1; }

    int r = kfs_create_dir((u32)parent, name);
    if (r == -1) { kprint("mkdir: failed (exists or disk full)\n"); return -1; }
    return 0;
}

int vfs_write(const char *path, const char *data) {
    int ino = resolve(path);

    /* Create file if it doesn't exist */
    if (ino == -1) {
        char name[KFS_NAME_MAX + 1];
        int parent = resolve_parent(path, name);
        if (parent == -1) { kprint("write: parent not found\n"); return -1; }
        ino = kfs_create_file((u32)parent, name);
        if (ino == -1) { kprint("write: cannot create file\n"); return -1; }
    }

    kfs_inode_t inode;
    kfs_read_inode((u32)ino, &inode);
    if (inode.type != KFS_TYPE_FILE) { kprint("write: not a file\n"); return -1; }

    /* Overwrite from offset 0 */
    int len = strlen((char *)data);
    int written = kfs_write_file((u32)ino, 0, (const u8 *)data, (u32)len);
    if (written != len) { kprint("write: partial write (disk full?)\n"); return -1; }
    return 0;
}

int vfs_rm(const char *path) {
    char norm[128];
    if (normalize_path(path, norm, sizeof(norm)) != 0) {
        kprint("rm: invalid path\n");
        return -1;
    }
    if (strcmp(norm, "/") == 0) {
        kprint("rm: cannot remove root\n");
        return -1;
    }

    char name[KFS_NAME_MAX + 1];
    int parent = resolve_parent(norm, name);
    if (parent == -1) { kprint("rm: parent not found\n"); return -1; }

    int r = kfs_delete((u32)parent, name);
    if (r == -1) { kprint("rm: not found\n"); return -1; }
    return 0;
}

int vfs_list_dir(const char *path, char names[][28], u8 types[], int max_items) {
    int ino = resolve(path);
    if (ino == -1) return -1;
    return kfs_dir_get_entries((u32)ino, names, types, max_items);
}

int vfs_read_into(const char *path, char *buf, int max_len) {
    int ino = resolve(path);
    if (ino == -1) return -1;

    kfs_inode_t inode;
    kfs_read_inode((u32)ino, &inode);
    if (inode.type != KFS_TYPE_FILE) return -1;

    int got = kfs_read_file((u32)ino, 0, (u8 *)buf, max_len);
    if (got >= 0 && got < max_len) {
        buf[got] = '\0';  /* null terminate to be safe for text editors */
    } else if (got == max_len && max_len > 0) {
        buf[max_len - 1] = '\0';
    }
    return got;
}
