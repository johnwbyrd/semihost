/*
 * ZBC virtio-9p Transport
 *
 * File semihosting opcodes over 9P2000.L on a virtio queue. Each
 * semihosting call maps to one or more synchronous 9p RPCs per the
 * normative table in docs/source/qemu-transports-proposal.rst:
 *
 *   SYS_OPEN   -> Twalk + Tlopen (or Twalk-to-parent + Tlcreate)
 *   SYS_CLOSE  -> Tclunk
 *   SYS_READ   -> Tread loop          SYS_WRITE  -> Twrite loop
 *   SYS_SEEK   -> fd-table state      SYS_FLEN   -> Tgetattr
 *   SYS_REMOVE -> Twalk + Tremove     SYS_RENAME -> Twalk x2 + Trenameat
 *   SYS_TMPNAM -> guest-side name generation
 *
 * Errors arrive as Rlerror carrying a Linux errno, which flows into
 * response->error_code unchanged.
 */

#include "zbc_9p.h"

/* fids: root from Tattach, one per fd slot, two scratch for path ops. */
#define P9_FID_ROOT 0
#define P9_FID_FILE(slot) ((uint32_t)(slot) + 1)
#define P9_FID_SCRATCH_A 100
#define P9_FID_SCRATCH_B 101

/* Single outstanding RPC; a constant tag suffices. */
#define P9_TAG 1

/* Per-round-trip headroom for message headers. */
#define P9_IO_OVERHEAD 32

/*========================================================================
 * Message writer (T-messages)
 *========================================================================*/

typedef struct {
  uint8_t *buf;
  size_t cap;
  size_t pos; /* > cap means overflow */
} p9_wr_t;

static void wr_raw_u8(p9_wr_t *w, uint8_t v) {
  if (w->pos + 1 > w->cap) {
    w->pos = w->cap + 1;
    return;
  }
  w->buf[w->pos++] = v;
}

static void wr_u16(p9_wr_t *w, uint16_t v) {
  if (w->pos + 2 > w->cap) {
    w->pos = w->cap + 1;
    return;
  }
  ZBC_WRITE_U16_LE(w->buf + w->pos, v);
  w->pos += 2;
}

static void wr_u32(p9_wr_t *w, uint32_t v) {
  if (w->pos + 4 > w->cap) {
    w->pos = w->cap + 1;
    return;
  }
  ZBC_WRITE_U32_LE(w->buf + w->pos, v);
  w->pos += 4;
}

/* 64-bit field from a 32-bit value (this implementation tracks 32-bit
 * file offsets; the high word is always zero). */
static void wr_u64_32(p9_wr_t *w, uint32_t lo) {
  wr_u32(w, lo);
  wr_u32(w, 0);
}

static void wr_bytes(p9_wr_t *w, const void *data, size_t len) {
  size_t i;
  const uint8_t *src = (const uint8_t *)data;

  if (w->pos + len > w->cap) {
    w->pos = w->cap + 1;
    return;
  }
  for (i = 0; i < len; i++) {
    w->buf[w->pos + i] = src[i];
  }
  w->pos += len;
}

/* 9p string: len[2] + bytes, no NUL. */
static void wr_str(p9_wr_t *w, const char *s, size_t len) {
  wr_u16(w, (uint16_t)len);
  wr_bytes(w, s, len);
}

static void wr_begin(p9_wr_t *w, zbc_9p_state_t *s, int type, uint16_t tag) {
  w->buf = s->tx;
  w->cap = s->msize;
  w->pos = 0;
  wr_u32(w, 0); /* size, patched by p9_rpc */
  wr_raw_u8(w, (uint8_t)type);
  wr_u16(w, tag);
}

/*========================================================================
 * Reply reader (R-messages)
 *========================================================================*/

typedef struct {
  const uint8_t *buf;
  size_t len;
  size_t pos;
  int truncated; /* set when a read ran past the end */
} p9_rd_t;

static uint16_t rd_u16(p9_rd_t *r) {
  uint16_t v;

  if (r->pos + 2 > r->len) {
    r->truncated = 1;
    return 0;
  }
  v = ZBC_READ_U16_LE(r->buf + r->pos);
  r->pos += 2;
  return v;
}

static uint32_t rd_u32(p9_rd_t *r) {
  uint32_t v;

  if (r->pos + 4 > r->len) {
    r->truncated = 1;
    return 0;
  }
  v = ZBC_READ_U32_LE(r->buf + r->pos);
  r->pos += 4;
  return v;
}

static const uint8_t *rd_bytes(p9_rd_t *r, size_t len) {
  const uint8_t *p;

  if (r->pos + len > r->len) {
    r->truncated = 1;
    return (const uint8_t *)0;
  }
  p = r->buf + r->pos;
  r->pos += len;
  return p;
}

static void rd_skip(p9_rd_t *r, size_t len) { (void)rd_bytes(r, len); }

/*========================================================================
 * RPC core
 *========================================================================*/

/*
 * Send the built T-message and receive the reply.
 *
 * Returns ZBC_OK with *p9err = 0 and *reply positioned at the R-message
 * payload on success; ZBC_OK with *p9err = Linux errno when the server
 * answered Rlerror; ZBC_ERR_* on transport-level failure.
 */
static int p9_rpc(zbc_9p_state_t *s, p9_wr_t *w, int rtype, p9_rd_t *reply,
                  int *p9err) {
  uint32_t used = 0;
  uint32_t rsize;
  uint8_t type;
  int rc;

  *p9err = 0;

  if (w->pos > w->cap) {
    ZBC_LOG_ERROR_S("9p: T-message exceeds msize");
    return ZBC_ERR_BUFFER_FULL;
  }
  ZBC_WRITE_U32_LE(w->buf, (uint32_t)w->pos);

  rc = zbc_virtq_xfer(&s->vq, s->tx, w->pos, s->rx, s->msize, &used);
  if (rc != ZBC_OK) {
    return rc;
  }

  if (used < ZBC_9P_HDR_SIZE) {
    return ZBC_ERR_PARSE_ERROR;
  }
  rsize = ZBC_READ_U32_LE(s->rx);
  if (rsize < ZBC_9P_HDR_SIZE || rsize > used) {
    return ZBC_ERR_PARSE_ERROR;
  }
  type = s->rx[4];
  /* tag (bytes 5-6) carries no information with one outstanding RPC */

  reply->buf = s->rx;
  reply->len = rsize;
  reply->pos = ZBC_9P_HDR_SIZE;
  reply->truncated = 0;

  if (type == ZBC_9P_RLERROR) {
    *p9err = (int)rd_u32(reply);
    if (reply->truncated || *p9err <= 0) {
      return ZBC_ERR_PARSE_ERROR;
    }
    return ZBC_OK;
  }
  if (type != rtype) {
    ZBC_LOG_ERROR("9p: expected R-type %d, got %d", rtype, (int)type);
    return ZBC_ERR_PARSE_ERROR;
  }
  return ZBC_OK;
}

/* Clunk a fid, ignoring errors (cleanup paths). */
static void p9_clunk(zbc_9p_state_t *s, uint32_t fid) {
  p9_wr_t w;
  p9_rd_t reply;
  int p9err;

  wr_begin(&w, s, ZBC_9P_TCLUNK, P9_TAG);
  wr_u32(&w, fid);
  (void)p9_rpc(s, &w, ZBC_9P_RCLUNK, &reply, &p9err);
}

/*========================================================================
 * Path walking
 *========================================================================*/

/*
 * Walk 'path' from base_fid, establishing newfid at the destination.
 * With to_parent set, stops one component early and returns the leaf
 * name. Empty and repeated separators are skipped; an absolute path is
 * relative to the 9p export root, like every path in this transport.
 */
static int p9_walk(zbc_9p_state_t *s, uint32_t base_fid, uint32_t newfid,
                   const char *path, size_t path_len, int to_parent,
                   const char **leaf, size_t *leaf_len, int *p9err) {
  const char *comp[ZBC_9P_MAXWELEM];
  size_t clen[ZBC_9P_MAXWELEM];
  int n = 0;
  size_t i = 0;
  p9_wr_t w;
  p9_rd_t reply;
  int rc;

  *p9err = 0;

  while (i < path_len) {
    while (i < path_len && path[i] == '/') {
      i++;
    }
    if (i >= path_len) {
      break;
    }
    {
      size_t start = i;

      while (i < path_len && path[i] != '/') {
        i++;
      }
      if (n >= ZBC_9P_MAXWELEM) {
        *p9err = 36; /* ENAMETOOLONG: deeper than one Twalk carries */
        return ZBC_OK;
      }
      comp[n] = path + start;
      clen[n] = i - start;
      n++;
    }
  }

  if (to_parent) {
    if (n == 0) {
      *p9err = ZBC_ERRNO_ENOENT; /* no leaf to create/rename */
      return ZBC_OK;
    }
    n--;
    *leaf = comp[n];
    *leaf_len = clen[n];
  }

  wr_begin(&w, s, ZBC_9P_TWALK, P9_TAG);
  wr_u32(&w, base_fid);
  wr_u32(&w, newfid);
  wr_u16(&w, (uint16_t)n);
  for (i = 0; i < (size_t)n; i++) {
    wr_str(&w, comp[i], clen[i]);
  }

  rc = p9_rpc(s, &w, ZBC_9P_RWALK, &reply, p9err);
  if (rc != ZBC_OK || *p9err != 0) {
    return rc;
  }

  /* Partial walk: newfid was not established. */
  if (rd_u16(&reply) != (uint16_t)n || reply.truncated) {
    *p9err = ZBC_ERRNO_ENOENT;
  }
  return ZBC_OK;
}

/* Fetch the file size via Tgetattr. */
static int p9_getattr_size(zbc_9p_state_t *s, uint32_t fid, uint32_t *size_out,
                           int *p9err) {
  p9_wr_t w;
  p9_rd_t reply;
  int rc;

  wr_begin(&w, s, ZBC_9P_TGETATTR, P9_TAG);
  wr_u32(&w, fid);
  wr_u64_32(&w, (uint32_t)ZBC_9P_GETATTR_SIZE);

  rc = p9_rpc(s, &w, ZBC_9P_RGETATTR, &reply, p9err);
  if (rc != ZBC_OK || *p9err != 0) {
    return rc;
  }

  rd_skip(&reply, ZBC_9P_RGETATTR_SIZE_OFFSET);
  *size_out = rd_u32(&reply); /* low word; 32-bit offsets by design */
  rd_skip(&reply, 4);
  if (reply.truncated) {
    return ZBC_ERR_PARSE_ERROR;
  }
  return ZBC_OK;
}

/*========================================================================
 * Session setup
 *========================================================================*/

int zbc_9p_setup(zbc_9p_state_t *s, volatile void *mmio, void *queue_mem,
                 size_t queue_mem_size, void *msg_buf, size_t msg_buf_size) {
  uint32_t device_id;
  int rc;
  int i;

  if (!s || !mmio || !queue_mem || !msg_buf) {
    return ZBC_ERR_NULL_ARG;
  }
  if (msg_buf_size < ZBC_9P_MSGBUF_MIN) {
    return ZBC_ERR_BUFFER_FULL;
  }

  rc = zbc_virtio_probe(mmio, &device_id);
  if (rc != ZBC_OK) {
    return rc;
  }
  if (device_id != ZBC_VIRTIO_ID_9P) {
    ZBC_LOG_ERROR("9p: device ID %u is not a 9p transport",
                  (unsigned)device_id);
    return ZBC_ERR_DEVICE_ERROR;
  }

  rc = zbc_virtio_init(mmio);
  if (rc != ZBC_OK) {
    return rc;
  }
  rc = zbc_virtq_init(&s->vq, mmio, 0, queue_mem, queue_mem_size);
  if (rc != ZBC_OK) {
    return rc;
  }

  /* Request and reply halves of the message buffer. */
  s->tx = (uint8_t *)msg_buf;
  s->rx = (uint8_t *)msg_buf + msg_buf_size / 2;
  s->msize = (uint32_t)(msg_buf_size / 2);

  for (i = 0; i < ZBC_9P_MAX_FILES; i++) {
    s->files[i].in_use = 0;
    s->files[i].offset = 0;
  }
  s->last_errno = 0;
  s->tmpnam_counter = 0;

  return zbc_virtio_start(mmio);
}

int zbc_9p_mount(zbc_9p_state_t *s) {
  p9_wr_t w;
  p9_rd_t reply;
  int p9err;
  int rc;

  if (!s || !s->tx) {
    return ZBC_ERR_NOT_INITIALIZED;
  }

  /* Tversion: negotiate msize and the protocol dialect. */
  wr_begin(&w, s, ZBC_9P_TVERSION, ZBC_9P_NOTAG);
  wr_u32(&w, s->msize);
  wr_str(&w, ZBC_9P_VERSION_STR, sizeof(ZBC_9P_VERSION_STR) - 1);

  rc = p9_rpc(s, &w, ZBC_9P_RVERSION, &reply, &p9err);
  if (rc != ZBC_OK) {
    return rc;
  }
  if (p9err != 0) {
    return ZBC_ERR_DEVICE_ERROR;
  }
  {
    uint32_t server_msize = rd_u32(&reply);
    uint16_t vlen = rd_u16(&reply);
    const uint8_t *vstr = rd_bytes(&reply, vlen);
    size_t want = sizeof(ZBC_9P_VERSION_STR) - 1;
    size_t i;

    if (reply.truncated || server_msize < ZBC_9P_MSGBUF_MIN / 2 ||
        vlen != want || !vstr) {
      ZBC_LOG_ERROR_S("9p: bad Rversion");
      return ZBC_ERR_DEVICE_ERROR;
    }
    for (i = 0; i < want; i++) {
      if (vstr[i] != (uint8_t)ZBC_9P_VERSION_STR[i]) {
        ZBC_LOG_ERROR_S("9p: server does not speak 9P2000.L");
        return ZBC_ERR_DEVICE_ERROR;
      }
    }
    if (server_msize < s->msize) {
      s->msize = server_msize;
    }
  }

  /* Tattach: obtain the root fid for the export. */
  wr_begin(&w, s, ZBC_9P_TATTACH, P9_TAG);
  wr_u32(&w, P9_FID_ROOT);
  wr_u32(&w, (uint32_t)ZBC_9P_NOFID);
  wr_str(&w, "zbc", 3); /* uname */
  wr_str(&w, "", 0);    /* aname: the export the device selects */
  wr_u32(&w, 0);        /* n_uname */

  rc = p9_rpc(s, &w, ZBC_9P_RATTACH, &reply, &p9err);
  if (rc != ZBC_OK) {
    return rc;
  }
  return (p9err == 0) ? ZBC_OK : ZBC_ERR_DEVICE_ERROR;
}

/*========================================================================
 * Opcode implementations
 *========================================================================*/

static void fill_response(zbc_response_t *response, int result,
                          int error_code) {
  response->result = result;
  response->error_code = error_code;
  response->data = (const uint8_t *)0;
  response->data_size = 0;
  response->is_error = 0;
  response->proto_error = 0;
}

/* Fail with a POSIX errno: result -1, sticky SYS_ERRNO state. */
static void fail_errno(zbc_9p_state_t *s, zbc_response_t *response, int err) {
  s->last_errno = err;
  fill_response(response, -1, err);
}

/* SH_OPEN_* -> Tlopen/Tlcreate flags. Binary modes map like text modes. */
static const uint32_t sh_open_flags[12] = {
    ZBC_9P_O_RDONLY,                                    /* r   */
    ZBC_9P_O_RDONLY,                                    /* rb  */
    ZBC_9P_O_RDWR,                                      /* r+  */
    ZBC_9P_O_RDWR,                                      /* r+b */
    ZBC_9P_O_WRONLY | ZBC_9P_O_CREAT | ZBC_9P_O_TRUNC,  /* w   */
    ZBC_9P_O_WRONLY | ZBC_9P_O_CREAT | ZBC_9P_O_TRUNC,  /* wb  */
    ZBC_9P_O_RDWR | ZBC_9P_O_CREAT | ZBC_9P_O_TRUNC,    /* w+  */
    ZBC_9P_O_RDWR | ZBC_9P_O_CREAT | ZBC_9P_O_TRUNC,    /* w+b */
    ZBC_9P_O_WRONLY | ZBC_9P_O_CREAT | ZBC_9P_O_APPEND, /* a   */
    ZBC_9P_O_WRONLY | ZBC_9P_O_CREAT | ZBC_9P_O_APPEND, /* ab  */
    ZBC_9P_O_RDWR | ZBC_9P_O_CREAT | ZBC_9P_O_APPEND,   /* a+  */
    ZBC_9P_O_RDWR | ZBC_9P_O_CREAT | ZBC_9P_O_APPEND    /* a+b */
};

static int do_open(zbc_9p_state_t *s, zbc_response_t *response,
                   const char *path, size_t path_len, int mode) {
  uint32_t flags;
  uint32_t fid;
  int slot;
  int p9err;
  int rc;
  p9_wr_t w;
  p9_rd_t reply;

  if (mode < 0 || mode > 11) {
    fail_errno(s, response, ZBC_ERRNO_EINVAL);
    return ZBC_OK;
  }
  flags = sh_open_flags[mode];

  for (slot = 0; slot < ZBC_9P_MAX_FILES; slot++) {
    if (!s->files[slot].in_use) {
      break;
    }
  }
  if (slot == ZBC_9P_MAX_FILES) {
    fail_errno(s, response, ZBC_ERRNO_EMFILE);
    return ZBC_OK;
  }
  fid = P9_FID_FILE(slot);

  rc = p9_walk(s, P9_FID_ROOT, fid, path, path_len, 0, (const char **)0,
               (size_t *)0, &p9err);
  if (rc != ZBC_OK) {
    return rc;
  }

  if (p9err == 0) {
    /* Existing file: open it (create flag stripped; trunc/append kept). */
    wr_begin(&w, s, ZBC_9P_TLOPEN, P9_TAG);
    wr_u32(&w, fid);
    wr_u32(&w, flags & ~ZBC_9P_O_CREAT);
    rc = p9_rpc(s, &w, ZBC_9P_RLOPEN, &reply, &p9err);
    if (rc != ZBC_OK) {
      p9_clunk(s, fid);
      return rc;
    }
    if (p9err != 0) {
      p9_clunk(s, fid);
      fail_errno(s, response, p9err);
      return ZBC_OK;
    }
  } else if (flags & ZBC_9P_O_CREAT) {
    /* Missing file in a create mode: walk to the parent, Tlcreate. */
    const char *leaf = (const char *)0;
    size_t leaf_len = 0;

    rc = p9_walk(s, P9_FID_ROOT, fid, path, path_len, 1, &leaf, &leaf_len,
                 &p9err);
    if (rc != ZBC_OK) {
      return rc;
    }
    if (p9err != 0) {
      fail_errno(s, response, p9err);
      return ZBC_OK;
    }

    wr_begin(&w, s, ZBC_9P_TLCREATE, P9_TAG);
    wr_u32(&w, fid);
    wr_str(&w, leaf, leaf_len);
    wr_u32(&w, flags & ~ZBC_9P_O_CREAT);
    wr_u32(&w, 0644); /* mode: rw-r--r-- */
    wr_u32(&w, 0);    /* gid */
    rc = p9_rpc(s, &w, ZBC_9P_RLCREATE, &reply, &p9err);
    if (rc != ZBC_OK) {
      p9_clunk(s, fid);
      return rc;
    }
    if (p9err != 0) {
      p9_clunk(s, fid);
      fail_errno(s, response, p9err);
      return ZBC_OK;
    }
  } else {
    fail_errno(s, response, p9err);
    return ZBC_OK;
  }

  /* Append modes start at end-of-file, like fopen("a"). */
  s->files[slot].offset = 0;
  if (flags & ZBC_9P_O_APPEND) {
    uint32_t size = 0;

    rc = p9_getattr_size(s, fid, &size, &p9err);
    if (rc != ZBC_OK || p9err != 0) {
      p9_clunk(s, fid);
      if (rc != ZBC_OK) {
        return rc;
      }
      fail_errno(s, response, p9err);
      return ZBC_OK;
    }
    s->files[slot].offset = size;
  }

  s->files[slot].in_use = 1;
  fill_response(response, slot + 3, 0);
  return ZBC_OK;
}

/* Map an fd to its slot, or -1 when it is not an open file. */
static int fd_slot(const zbc_9p_state_t *s, int fd) {
  int slot = fd - 3;

  if (slot < 0 || slot >= ZBC_9P_MAX_FILES || !s->files[slot].in_use) {
    return -1;
  }
  return slot;
}

static int do_close(zbc_9p_state_t *s, zbc_response_t *response, int fd) {
  int slot = fd_slot(s, fd);

  if (slot < 0) {
    fail_errno(s, response, ZBC_ERRNO_EBADF);
    return ZBC_OK;
  }

  /* Tclunk releases the fid even on error; the slot is freed either way. */
  p9_clunk(s, P9_FID_FILE(slot));
  s->files[slot].in_use = 0;
  fill_response(response, 0, 0);
  return ZBC_OK;
}

static int do_read(zbc_9p_state_t *s, zbc_response_t *response, int fd,
                   uint8_t *dest, size_t count) {
  int slot = fd_slot(s, fd);
  uint32_t chunk;
  size_t got = 0;
  p9_wr_t w;
  p9_rd_t reply;
  int p9err;
  int rc;

  if (slot < 0) {
    fail_errno(s, response, ZBC_ERRNO_EBADF);
    return ZBC_OK;
  }

  chunk = (s->msize > P9_IO_OVERHEAD) ? s->msize - P9_IO_OVERHEAD : 16;

  while (got < count) {
    uint32_t n = (uint32_t)(count - got);
    uint32_t cnt;
    const uint8_t *data;
    size_t i;

    if (n > chunk) {
      n = chunk;
    }

    wr_begin(&w, s, ZBC_9P_TREAD, P9_TAG);
    wr_u32(&w, P9_FID_FILE(slot));
    wr_u64_32(&w, s->files[slot].offset);
    wr_u32(&w, n);
    rc = p9_rpc(s, &w, ZBC_9P_RREAD, &reply, &p9err);
    if (rc != ZBC_OK) {
      return rc;
    }
    if (p9err != 0) {
      fail_errno(s, response, p9err);
      return ZBC_OK;
    }

    cnt = rd_u32(&reply);
    if (cnt > n) {
      return ZBC_ERR_PARSE_ERROR; /* server returned more than asked */
    }
    data = rd_bytes(&reply, cnt);
    if (reply.truncated) {
      return ZBC_ERR_PARSE_ERROR;
    }

    for (i = 0; i < cnt; i++) {
      dest[got + i] = data[i];
    }
    got += cnt;
    s->files[slot].offset += cnt;

    if (cnt < n) {
      break; /* end of file */
    }
  }

  fill_response(response, (int)(count - got), 0); /* bytes NOT read */
  response->data = dest;
  response->data_size = got;
  return ZBC_OK;
}

static int do_write(zbc_9p_state_t *s, zbc_response_t *response, int fd,
                    const uint8_t *src, size_t count) {
  int slot = fd_slot(s, fd);
  uint32_t chunk;
  size_t written = 0;
  p9_wr_t w;
  p9_rd_t reply;
  int p9err;
  int rc;

  if (slot < 0) {
    fail_errno(s, response, ZBC_ERRNO_EBADF);
    return ZBC_OK;
  }

  chunk = (s->msize > P9_IO_OVERHEAD) ? s->msize - P9_IO_OVERHEAD : 16;

  while (written < count) {
    uint32_t n = (uint32_t)(count - written);
    uint32_t cnt;

    if (n > chunk) {
      n = chunk;
    }

    wr_begin(&w, s, ZBC_9P_TWRITE, P9_TAG);
    wr_u32(&w, P9_FID_FILE(slot));
    wr_u64_32(&w, s->files[slot].offset);
    wr_u32(&w, n);
    wr_bytes(&w, src + written, n);
    rc = p9_rpc(s, &w, ZBC_9P_RWRITE, &reply, &p9err);
    if (rc != ZBC_OK) {
      return rc;
    }
    if (p9err != 0) {
      fail_errno(s, response, p9err);
      return ZBC_OK;
    }

    cnt = rd_u32(&reply);
    if (reply.truncated || cnt > n) {
      return ZBC_ERR_PARSE_ERROR;
    }
    written += cnt;
    s->files[slot].offset += cnt;

    if (cnt < n) {
      break; /* short write (e.g. filesystem full) */
    }
  }

  fill_response(response, (int)(count - written), 0); /* bytes NOT written */
  return ZBC_OK;
}

static int do_flen(zbc_9p_state_t *s, zbc_response_t *response, int fd) {
  int slot = fd_slot(s, fd);
  uint32_t size = 0;
  int p9err;
  int rc;

  if (slot < 0) {
    fail_errno(s, response, ZBC_ERRNO_EBADF);
    return ZBC_OK;
  }

  rc = p9_getattr_size(s, P9_FID_FILE(slot), &size, &p9err);
  if (rc != ZBC_OK) {
    return rc;
  }
  if (p9err != 0) {
    fail_errno(s, response, p9err);
    return ZBC_OK;
  }
  fill_response(response, (int)size, 0);
  return ZBC_OK;
}

static int do_remove(zbc_9p_state_t *s, zbc_response_t *response,
                     const char *path, size_t path_len) {
  p9_wr_t w;
  p9_rd_t reply;
  int p9err;
  int rc;

  rc = p9_walk(s, P9_FID_ROOT, P9_FID_SCRATCH_A, path, path_len, 0,
               (const char **)0, (size_t *)0, &p9err);
  if (rc != ZBC_OK) {
    return rc;
  }
  if (p9err != 0) {
    fail_errno(s, response, p9err);
    return ZBC_OK;
  }

  /* Tremove clunks the fid whether or not it succeeds. */
  wr_begin(&w, s, ZBC_9P_TREMOVE, P9_TAG);
  wr_u32(&w, P9_FID_SCRATCH_A);
  rc = p9_rpc(s, &w, ZBC_9P_RREMOVE, &reply, &p9err);
  if (rc != ZBC_OK) {
    return rc;
  }
  if (p9err != 0) {
    fail_errno(s, response, p9err);
    return ZBC_OK;
  }
  fill_response(response, 0, 0);
  return ZBC_OK;
}

static int do_rename(zbc_9p_state_t *s, zbc_response_t *response,
                     const char *old_path, size_t old_len, const char *new_path,
                     size_t new_len) {
  const char *old_leaf = (const char *)0;
  const char *new_leaf = (const char *)0;
  size_t old_leaf_len = 0;
  size_t new_leaf_len = 0;
  p9_wr_t w;
  p9_rd_t reply;
  int p9err;
  int rc;

  rc = p9_walk(s, P9_FID_ROOT, P9_FID_SCRATCH_A, old_path, old_len, 1,
               &old_leaf, &old_leaf_len, &p9err);
  if (rc != ZBC_OK) {
    return rc;
  }
  if (p9err != 0) {
    fail_errno(s, response, p9err);
    return ZBC_OK;
  }

  rc = p9_walk(s, P9_FID_ROOT, P9_FID_SCRATCH_B, new_path, new_len, 1,
               &new_leaf, &new_leaf_len, &p9err);
  if (rc != ZBC_OK) {
    p9_clunk(s, P9_FID_SCRATCH_A);
    return rc;
  }
  if (p9err != 0) {
    p9_clunk(s, P9_FID_SCRATCH_A);
    fail_errno(s, response, p9err);
    return ZBC_OK;
  }

  wr_begin(&w, s, ZBC_9P_TRENAMEAT, P9_TAG);
  wr_u32(&w, P9_FID_SCRATCH_A);
  wr_str(&w, old_leaf, old_leaf_len);
  wr_u32(&w, P9_FID_SCRATCH_B);
  wr_str(&w, new_leaf, new_leaf_len);
  rc = p9_rpc(s, &w, ZBC_9P_RRENAMEAT, &reply, &p9err);

  p9_clunk(s, P9_FID_SCRATCH_A);
  p9_clunk(s, P9_FID_SCRATCH_B);

  if (rc != ZBC_OK) {
    return rc;
  }
  if (p9err != 0) {
    fail_errno(s, response, p9err);
    return ZBC_OK;
  }
  fill_response(response, 0, 0);
  return ZBC_OK;
}

/* "tmpNNNNN_III": same shape the host backends generate. */
static int do_tmpnam(zbc_9p_state_t *s, zbc_response_t *response, char *dest,
                     int id, size_t maxlen) {
  char name[13];
  unsigned counter = (unsigned)s->tmpnam_counter++;
  unsigned ident = (unsigned)(id & 0xFF);
  int i;

  name[0] = 't';
  name[1] = 'm';
  name[2] = 'p';
  for (i = 7; i >= 3; i--) {
    name[i] = (char)('0' + counter % 10);
    counter /= 10;
  }
  name[8] = '_';
  for (i = 11; i >= 9; i--) {
    name[i] = (char)('0' + ident % 10);
    ident /= 10;
  }
  name[12] = '\0';

  if (maxlen < sizeof(name)) {
    fail_errno(s, response, ZBC_ERRNO_EINVAL);
    return ZBC_OK;
  }
  for (i = 0; i < (int)sizeof(name); i++) {
    dest[i] = name[i];
  }
  fill_response(response, 0, 0);
  response->data = (const uint8_t *)dest;
  response->data_size = sizeof(name);
  return ZBC_OK;
}

/*========================================================================
 * Transport entry point
 *========================================================================*/

static int p9_transport_call(zbc_response_t *response,
                             zbc_client_state_t *state, void *buf,
                             size_t buf_size, int opcode, uintptr_t *args) {
  zbc_9p_state_t *s = (zbc_9p_state_t *)state->transport_ctx;

  (void)buf;
  (void)buf_size;

  if (!s) {
    return ZBC_ERR_NOT_INITIALIZED;
  }

  switch (opcode) {
  case SH_SYS_OPEN:
    return do_open(s, response, (const char *)args[0], (size_t)args[2],
                   (int)args[1]);

  case SH_SYS_CLOSE:
    return do_close(s, response, (int)args[0]);

  case SH_SYS_READ:
    return do_read(s, response, (int)args[0], (uint8_t *)args[1],
                   (size_t)args[2]);

  case SH_SYS_WRITE:
    return do_write(s, response, (int)args[0], (const uint8_t *)args[1],
                    (size_t)args[2]);

  case SH_SYS_SEEK: {
    int slot = fd_slot(s, (int)args[0]);

    if (slot < 0) {
      fail_errno(s, response, ZBC_ERRNO_EBADF);
    } else {
      s->files[slot].offset = (uint32_t)args[1];
      fill_response(response, 0, 0);
    }
    return ZBC_OK;
  }

  case SH_SYS_FLEN:
    return do_flen(s, response, (int)args[0]);

  case SH_SYS_REMOVE:
    return do_remove(s, response, (const char *)args[0], (size_t)args[1]);

  case SH_SYS_RENAME:
    return do_rename(s, response, (const char *)args[0], (size_t)args[1],
                     (const char *)args[2], (size_t)args[3]);

  case SH_SYS_TMPNAM:
    return do_tmpnam(s, response, (char *)args[0], (int)args[1],
                     (size_t)args[2]);

  case SH_SYS_ISTTY: {
    int fd = (int)args[0];
    fill_response(response, (fd >= 0 && fd <= 2) ? 1 : 0, 0);
    return ZBC_OK;
  }

  case SH_SYS_ISERROR: {
    intptr_t status = (intptr_t)args[0];
    fill_response(response, (status < 0) ? 1 : 0, 0);
    return ZBC_OK;
  }

  case SH_SYS_ERRNO:
    fill_response(response, s->last_errno, 0);
    return ZBC_OK;

  default:
    /* Console opcodes route to the console transport in a composite;
     * time/exit go to platform hooks. */
    s->last_errno = ZBC_ERRNO_ENOSYS;
    fill_response(response, -1, ZBC_ERRNO_ENOSYS);
    return ZBC_OK;
  }
}

static const zbc_transport_t p9_transport_vtable = {p9_transport_call};

const zbc_transport_t *zbc_transport_9p(void) { return &p9_transport_vtable; }
