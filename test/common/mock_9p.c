/*
 * Mock 9P2000.L Server Implementation
 */

#include "mock_9p.h"

#include <string.h>

/* Linux errno values the server reports */
#define M9_ENOENT 2
#define M9_EIO 5
#define M9_ENFILE 23
#define M9_ENOTDIR 20
#define M9_ENOSPC 28

/*------------------------------------------------------------------------
 * Filesystem state
 *------------------------------------------------------------------------*/

void mock9p_init(mock9p_t *fs) { memset(fs, 0, sizeof(*fs)); }

mock9p_file_t *mock9p_add_file(mock9p_t *fs, const char *path, const void *data,
                               size_t len) {
  int i;

  for (i = 0; i < MOCK9P_MAX_FILES; i++) {
    if (!fs->files[i].exists) {
      mock9p_file_t *f = &fs->files[i];

      memset(f, 0, sizeof(*f));
      strncpy(f->path, path, MOCK9P_PATH_MAX - 1);
      if (len > MOCK9P_FILE_CAP) {
        len = MOCK9P_FILE_CAP;
      }
      if (data && len > 0) {
        memcpy(f->data, data, len);
      }
      f->size = (uint32_t)len;
      f->exists = 1;
      return f;
    }
  }
  return (mock9p_file_t *)0;
}

void mock9p_add_dir(mock9p_t *fs, const char *path) {
  if (fs->dir_count < MOCK9P_MAX_DIRS) {
    strncpy(fs->dirs[fs->dir_count], path, MOCK9P_PATH_MAX - 1);
    fs->dirs[fs->dir_count][MOCK9P_PATH_MAX - 1] = '\0';
    fs->dir_count++;
  }
}

mock9p_file_t *mock9p_find(mock9p_t *fs, const char *path) {
  int i;

  for (i = 0; i < MOCK9P_MAX_FILES; i++) {
    if (fs->files[i].exists && strcmp(fs->files[i].path, path) == 0) {
      return &fs->files[i];
    }
  }
  return (mock9p_file_t *)0;
}

static int is_dir(mock9p_t *fs, const char *path) {
  int i;

  if (path[0] == '\0') {
    return 1; /* root */
  }
  for (i = 0; i < fs->dir_count; i++) {
    if (strcmp(fs->dirs[i], path) == 0) {
      return 1;
    }
  }
  return 0;
}

/* dest = dir + "/" + name (no leading slash when dir is the root). */
static void path_join(char *dest, const char *dir, const char *name,
                      size_t name_len) {
  size_t pos = 0;

  dest[0] = '\0';
  if (dir[0] != '\0') {
    size_t dir_len = strlen(dir);

    if (dir_len > MOCK9P_PATH_MAX - 1) {
      dir_len = MOCK9P_PATH_MAX - 1;
    }
    memcpy(dest, dir, dir_len);
    pos = dir_len;
    if (pos < MOCK9P_PATH_MAX - 1) {
      dest[pos++] = '/';
    }
    dest[pos] = '\0';
  }
  while (name_len > 0 && pos < MOCK9P_PATH_MAX - 1) {
    dest[pos++] = *name++;
    name_len--;
  }
  dest[pos] = '\0';
}

/*------------------------------------------------------------------------
 * fid table
 *------------------------------------------------------------------------*/

static mock9p_fid_t *fid_find(mock9p_t *fs, uint32_t fid) {
  int i;

  for (i = 0; i < MOCK9P_MAX_FIDS; i++) {
    if (fs->fids[i].in_use && fs->fids[i].fid == fid) {
      return &fs->fids[i];
    }
  }
  return (mock9p_fid_t *)0;
}

static mock9p_fid_t *fid_set(mock9p_t *fs, uint32_t fid, const char *path) {
  mock9p_fid_t *f = fid_find(fs, fid);
  int i;

  if (!f) {
    for (i = 0; i < MOCK9P_MAX_FIDS; i++) {
      if (!fs->fids[i].in_use) {
        f = &fs->fids[i];
        break;
      }
    }
  }
  if (!f) {
    return (mock9p_fid_t *)0;
  }
  f->fid = fid;
  f->in_use = 1;
  strncpy(f->path, path, MOCK9P_PATH_MAX - 1);
  f->path[MOCK9P_PATH_MAX - 1] = '\0';
  return f;
}

static void fid_clunk(mock9p_t *fs, uint32_t fid) {
  mock9p_fid_t *f = fid_find(fs, fid);

  if (f) {
    f->in_use = 0;
  }
}

/*------------------------------------------------------------------------
 * Message cursor and reply builder
 *------------------------------------------------------------------------*/

typedef struct {
  const uint8_t *buf;
  size_t len;
  size_t pos;
  int bad;
} cur_t;

static uint8_t c_u8(cur_t *c) {
  if (c->pos + 1 > c->len) {
    c->bad = 1;
    return 0;
  }
  return c->buf[c->pos++];
}

static uint16_t c_u16(cur_t *c) {
  uint16_t v;

  if (c->pos + 2 > c->len) {
    c->bad = 1;
    return 0;
  }
  v = ZBC_READ_U16_LE(c->buf + c->pos);
  c->pos += 2;
  return v;
}

static uint32_t c_u32(cur_t *c) {
  uint32_t v;

  if (c->pos + 4 > c->len) {
    c->bad = 1;
    return 0;
  }
  v = ZBC_READ_U32_LE(c->buf + c->pos);
  c->pos += 4;
  return v;
}

/* 64-bit field, low word only (the transport never exceeds 32 bits). */
static uint32_t c_u64_32(cur_t *c) {
  uint32_t lo = c_u32(c);

  (void)c_u32(c);
  return lo;
}

/* 9p string into a NUL-terminated stack buffer (truncating). */
static void c_str(cur_t *c, char *dest, size_t cap) {
  uint16_t len = c_u16(c);
  size_t n = len;
  size_t i;

  if (c->pos + len > c->len) {
    c->bad = 1;
    dest[0] = '\0';
    return;
  }
  if (n > cap - 1) {
    n = cap - 1;
  }
  for (i = 0; i < n; i++) {
    dest[i] = (char)c->buf[c->pos + i];
  }
  dest[n] = '\0';
  c->pos += len;
}

typedef struct {
  uint8_t *buf;
  size_t cap;
  size_t pos;
} rep_t;

static void r_u8(rep_t *r, uint8_t v) {
  if (r->pos < r->cap) {
    r->buf[r->pos] = v;
  }
  r->pos += 1;
}

static void r_u16(rep_t *r, uint16_t v) {
  if (r->pos + 2 <= r->cap) {
    ZBC_WRITE_U16_LE(r->buf + r->pos, v);
  }
  r->pos += 2;
}

static void r_u32(rep_t *r, uint32_t v) {
  if (r->pos + 4 <= r->cap) {
    ZBC_WRITE_U32_LE(r->buf + r->pos, v);
  }
  r->pos += 4;
}

static void r_u64_32(rep_t *r, uint32_t lo) {
  r_u32(r, lo);
  r_u32(r, 0);
}

static void r_bytes(rep_t *r, const void *data, size_t len) {
  if (r->pos + len <= r->cap) {
    memcpy(r->buf + r->pos, data, len);
  }
  r->pos += len;
}

static void r_qid(rep_t *r, int dir) {
  r_u8(r, dir ? ZBC_9P_QTDIR : 0);
  r_u32(r, 0);    /* version */
  r_u64_32(r, 0); /* path */
}

static void r_begin(rep_t *r, uint8_t *buf, size_t cap, int type,
                    uint16_t tag) {
  r->buf = buf;
  r->cap = cap;
  r->pos = 0;
  r_u32(r, 0); /* size, patched in r_end */
  r_u8(r, (uint8_t)type);
  r_u16(r, tag);
}

static int r_end(rep_t *r) {
  if (r->pos > r->cap) {
    return -1; /* reply buffer too small: leave pending */
  }
  ZBC_WRITE_U32_LE(r->buf, (uint32_t)r->pos);
  return (int)r->pos;
}

static int r_error(uint8_t *buf, size_t cap, uint16_t tag, uint32_t ecode) {
  rep_t r;

  r_begin(&r, buf, cap, ZBC_9P_RLERROR, tag);
  r_u32(&r, ecode);
  return r_end(&r);
}

/*------------------------------------------------------------------------
 * Request dispatch
 *------------------------------------------------------------------------*/

int mock9p_service(void *ctx, int queue_index, const uint8_t *out,
                   size_t out_len, uint8_t *in, size_t in_len) {
  mock9p_t *fs = (mock9p_t *)ctx;
  cur_t c;
  rep_t r;
  uint32_t size;
  uint8_t type;
  uint16_t tag;

  (void)queue_index;

  if (!out || out_len < ZBC_9P_HDR_SIZE || !in) {
    return -1;
  }

  c.buf = out;
  c.len = out_len;
  c.pos = 0;
  c.bad = 0;

  size = c_u32(&c);
  type = c_u8(&c);
  tag = c_u16(&c);
  if (c.bad || size > out_len) {
    return r_error(in, in_len, tag, M9_EIO);
  }
  c.len = size; /* trust the message's own framing */

  fs->request_count++;

  switch (type) {
  case ZBC_9P_TVERSION: {
    uint32_t client_msize = c_u32(&c);
    uint32_t msize = fs->reply_msize ? fs->reply_msize : client_msize;

    if (msize > client_msize) {
      msize = client_msize;
    }
    r_begin(&r, in, in_len, ZBC_9P_RVERSION, tag);
    r_u32(&r, msize);
    r_u16(&r, (uint16_t)(sizeof(ZBC_9P_VERSION_STR) - 1));
    r_bytes(&r, ZBC_9P_VERSION_STR, sizeof(ZBC_9P_VERSION_STR) - 1);
    return r_end(&r);
  }

  case ZBC_9P_TATTACH: {
    uint32_t fid = c_u32(&c);

    if (!fid_set(fs, fid, "")) {
      return r_error(in, in_len, tag, M9_ENFILE);
    }
    r_begin(&r, in, in_len, ZBC_9P_RATTACH, tag);
    r_qid(&r, 1);
    return r_end(&r);
  }

  case ZBC_9P_TWALK: {
    uint32_t fid = c_u32(&c);
    uint32_t newfid = c_u32(&c);
    uint16_t nwname = c_u16(&c);
    mock9p_fid_t *base = fid_find(fs, fid);
    char path[MOCK9P_PATH_MAX];
    int i;

    if (!base || c.bad) {
      return r_error(in, in_len, tag, M9_EIO);
    }
    strcpy(path, base->path);

    for (i = 0; i < (int)nwname; i++) {
      char name[MOCK9P_PATH_MAX];
      char next[MOCK9P_PATH_MAX];

      c_str(&c, name, sizeof(name));
      if (c.bad) {
        return r_error(in, in_len, tag, M9_EIO);
      }
      path_join(next, path, name, strlen(name));

      if (i < (int)nwname - 1) {
        if (!is_dir(fs, next)) {
          return r_error(in, in_len, tag, M9_ENOENT);
        }
      } else {
        if (!is_dir(fs, next) && !mock9p_find(fs, next)) {
          return r_error(in, in_len, tag, M9_ENOENT);
        }
      }
      strcpy(path, next);
    }

    if (!fid_set(fs, newfid, path)) {
      return r_error(in, in_len, tag, M9_ENFILE);
    }
    r_begin(&r, in, in_len, ZBC_9P_RWALK, tag);
    r_u16(&r, nwname);
    for (i = 0; i < (int)nwname; i++) {
      r_qid(&r, 0);
    }
    return r_end(&r);
  }

  case ZBC_9P_TLOPEN: {
    uint32_t fid = c_u32(&c);
    uint32_t flags = c_u32(&c);
    mock9p_fid_t *f = fid_find(fs, fid);
    mock9p_file_t *file;

    if (!f || c.bad) {
      return r_error(in, in_len, tag, M9_EIO);
    }
    if (is_dir(fs, f->path)) {
      r_begin(&r, in, in_len, ZBC_9P_RLOPEN, tag);
      r_qid(&r, 1);
      r_u32(&r, 0);
      return r_end(&r);
    }
    file = mock9p_find(fs, f->path);
    if (!file) {
      return r_error(in, in_len, tag, M9_ENOENT);
    }
    if (flags & ZBC_9P_O_TRUNC) {
      file->size = 0;
    }
    r_begin(&r, in, in_len, ZBC_9P_RLOPEN, tag);
    r_qid(&r, 0);
    r_u32(&r, 0); /* iounit */
    return r_end(&r);
  }

  case ZBC_9P_TLCREATE: {
    uint32_t fid = c_u32(&c);
    char name[MOCK9P_PATH_MAX];
    mock9p_fid_t *f = fid_find(fs, fid);
    mock9p_file_t *file;
    char path[MOCK9P_PATH_MAX];

    c_str(&c, name, sizeof(name));
    if (!f || c.bad) {
      return r_error(in, in_len, tag, M9_EIO);
    }
    if (!is_dir(fs, f->path)) {
      return r_error(in, in_len, tag, M9_ENOTDIR);
    }
    path_join(path, f->path, name, strlen(name));

    file = mock9p_find(fs, path);
    if (file) {
      file->size = 0;
    } else {
      file = mock9p_add_file(fs, path, (const void *)0, 0);
      if (!file) {
        return r_error(in, in_len, tag, M9_ENOSPC);
      }
    }
    /* The fid now refers to the created (open) file. */
    strcpy(f->path, path);

    r_begin(&r, in, in_len, ZBC_9P_RLCREATE, tag);
    r_qid(&r, 0);
    r_u32(&r, 0); /* iounit */
    return r_end(&r);
  }

  case ZBC_9P_TREAD: {
    uint32_t fid = c_u32(&c);
    uint32_t offset = c_u64_32(&c);
    uint32_t count = c_u32(&c);
    mock9p_fid_t *f = fid_find(fs, fid);
    mock9p_file_t *file;
    uint32_t n = 0;

    if (!f || c.bad) {
      return r_error(in, in_len, tag, M9_EIO);
    }
    file = mock9p_find(fs, f->path);
    if (file && offset < file->size) {
      n = file->size - offset;
      if (n > count) {
        n = count;
      }
    }
    r_begin(&r, in, in_len, ZBC_9P_RREAD, tag);
    r_u32(&r, n);
    if (file && n > 0) {
      r_bytes(&r, file->data + offset, n);
    }
    return r_end(&r);
  }

  case ZBC_9P_TWRITE: {
    uint32_t fid = c_u32(&c);
    uint32_t offset = c_u64_32(&c);
    uint32_t count = c_u32(&c);
    mock9p_fid_t *f = fid_find(fs, fid);
    mock9p_file_t *file;
    uint32_t n = 0;

    if (!f || c.bad || c.pos + count > c.len) {
      return r_error(in, in_len, tag, M9_EIO);
    }
    file = mock9p_find(fs, f->path);
    if (!file) {
      return r_error(in, in_len, tag, M9_ENOENT);
    }
    if (offset < MOCK9P_FILE_CAP) {
      n = MOCK9P_FILE_CAP - offset; /* short write at capacity */
      if (n > count) {
        n = count;
      }
      memcpy(file->data + offset, c.buf + c.pos, n);
      if (offset + n > file->size) {
        file->size = offset + n;
      }
    }
    r_begin(&r, in, in_len, ZBC_9P_RWRITE, tag);
    r_u32(&r, n);
    return r_end(&r);
  }

  case ZBC_9P_TCLUNK: {
    uint32_t fid = c_u32(&c);

    fid_clunk(fs, fid);
    r_begin(&r, in, in_len, ZBC_9P_RCLUNK, tag);
    return r_end(&r);
  }

  case ZBC_9P_TREMOVE: {
    uint32_t fid = c_u32(&c);
    mock9p_fid_t *f = fid_find(fs, fid);
    mock9p_file_t *file = f ? mock9p_find(fs, f->path) : (mock9p_file_t *)0;

    fid_clunk(fs, fid); /* remove always clunks */
    if (!file) {
      return r_error(in, in_len, tag, M9_ENOENT);
    }
    file->exists = 0;
    r_begin(&r, in, in_len, ZBC_9P_RREMOVE, tag);
    return r_end(&r);
  }

  case ZBC_9P_TGETATTR: {
    uint32_t fid = c_u32(&c);
    mock9p_fid_t *f = fid_find(fs, fid);
    mock9p_file_t *file;
    uint32_t fsize = 0;
    int dir;
    size_t i;

    if (!f || c.bad) {
      return r_error(in, in_len, tag, M9_EIO);
    }
    dir = is_dir(fs, f->path);
    file = mock9p_find(fs, f->path);
    if (!dir && !file) {
      return r_error(in, in_len, tag, M9_ENOENT);
    }
    if (file) {
      fsize = file->size;
    }

    r_begin(&r, in, in_len, ZBC_9P_RGETATTR, tag);
    r_u64_32(&r, (uint32_t)ZBC_9P_GETATTR_SIZE); /* valid */
    r_qid(&r, dir);
    r_u32(&r, dir ? 040755UL : 0644UL); /* mode */
    r_u32(&r, 0);                       /* uid */
    r_u32(&r, 0);                       /* gid */
    r_u64_32(&r, 1);                    /* nlink */
    r_u64_32(&r, 0);                    /* rdev */
    r_u64_32(&r, fsize);                /* size */
    /* blksize..data_version: zero-fill the remainder of the payload */
    for (i = r.pos; i < ZBC_9P_HDR_SIZE + ZBC_9P_RGETATTR_PAYLOAD_LEN; i += 4) {
      r_u32(&r, 0);
    }
    return r_end(&r);
  }

  case ZBC_9P_TRENAMEAT: {
    uint32_t olddirfid = c_u32(&c);
    char oldname[MOCK9P_PATH_MAX];
    uint32_t newdirfid;
    char newname[MOCK9P_PATH_MAX];
    mock9p_fid_t *od, *nd;
    mock9p_file_t *file, *target;
    char oldpath[MOCK9P_PATH_MAX];
    char newpath[MOCK9P_PATH_MAX];

    c_str(&c, oldname, sizeof(oldname));
    newdirfid = c_u32(&c);
    c_str(&c, newname, sizeof(newname));

    od = fid_find(fs, olddirfid);
    nd = fid_find(fs, newdirfid);
    if (!od || !nd || c.bad) {
      return r_error(in, in_len, tag, M9_EIO);
    }
    path_join(oldpath, od->path, oldname, strlen(oldname));
    path_join(newpath, nd->path, newname, strlen(newname));

    file = mock9p_find(fs, oldpath);
    if (!file) {
      return r_error(in, in_len, tag, M9_ENOENT);
    }
    target = mock9p_find(fs, newpath);
    if (target) {
      target->exists = 0; /* rename overwrites */
    }
    strcpy(file->path, newpath);

    r_begin(&r, in, in_len, ZBC_9P_RRENAMEAT, tag);
    return r_end(&r);
  }

  default:
    return r_error(in, in_len, tag, M9_EIO);
  }
}
