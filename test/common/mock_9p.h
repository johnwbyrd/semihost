/*
 * Mock 9P2000.L Server for Transport Testing
 *
 * A tiny in-memory filesystem behind a mock_virtio service function.
 * Speaks exactly the message subset the ZBC 9p transport uses; replies
 * with Rlerror (Linux errno) on anything that fails. Paths are flat
 * strings; directories are an explicit list (the root "" always exists).
 */

#ifndef MOCK_9P_H
#define MOCK_9P_H

#include "zbc_9p_wire.h"
#include "zbc_virtio.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOCK9P_MAX_FILES 8
#define MOCK9P_FILE_CAP 512
#define MOCK9P_MAX_FIDS 16
#define MOCK9P_MAX_DIRS 4
#define MOCK9P_PATH_MAX 64

typedef struct {
  char path[MOCK9P_PATH_MAX];
  uint8_t data[MOCK9P_FILE_CAP];
  uint32_t size;
  int exists;
} mock9p_file_t;

typedef struct {
  uint32_t fid;
  char path[MOCK9P_PATH_MAX];
  int in_use;
} mock9p_fid_t;

typedef struct {
  mock9p_file_t files[MOCK9P_MAX_FILES];
  char dirs[MOCK9P_MAX_DIRS][MOCK9P_PATH_MAX];
  int dir_count;
  mock9p_fid_t fids[MOCK9P_MAX_FIDS];
  uint32_t reply_msize; /* 0 = echo the client's msize */
  int request_count;    /* T-messages processed */
} mock9p_t;

void mock9p_init(mock9p_t *fs);

/* Add a file with initial contents. Returns NULL if full. */
mock9p_file_t *mock9p_add_file(mock9p_t *fs, const char *path,
                               const void *data, size_t len);

/* Register a directory (the root "" is implicit). */
void mock9p_add_dir(mock9p_t *fs, const char *path);

/* Find an existing file by exact path, or NULL. */
mock9p_file_t *mock9p_find(mock9p_t *fs, const char *path);

/*
 * mock_virtio service function. Attach with:
 *   dev.service = mock9p_service;
 *   dev.service_ctx = &fs;
 */
int mock9p_service(void *ctx, int queue_index, const uint8_t *out,
                   size_t out_len, uint8_t *in, size_t in_len);

#ifdef __cplusplus
}
#endif

#endif /* MOCK_9P_H */
