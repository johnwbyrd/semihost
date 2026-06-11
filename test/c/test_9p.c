/*
 * virtio-9p Transport Tests
 *
 * Drives the 9p file transport through zbc_call()/zbc_api_* against the
 * mock 9P2000.L server (in-memory filesystem behind a mock virtio
 * device): session setup, the full open/create/read/write/seek/flen
 * lifecycle, remove/rename, subdirectory paths, chunked I/O when msize
 * is small, and fd exhaustion.
 */

#include "mock_9p.h"
#include "mock_virtio.h"
#include "test_harness.h"
#include "zbc_9p.h"
#include "zbc_api.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Fixture
 *------------------------------------------------------------------------*/

typedef struct {
  mock_virtio_t dev;
  mock_virtio_hook_t hook;
  mock9p_t fs;
  zbc_9p_state_t p9;
  zbc_client_state_t client;
  zbc_api_t api;
  uint8_t arena[ZBC_9P_ARENA_SIZE];
  uint8_t msgs[2048];
  uint8_t riff[64]; /* unused by the transport; zbc_call needs a buffer */
} p9_fixture_t;

static p9_fixture_t g_fx;

static int setup_9p_sized(p9_fixture_t *fx, size_t msg_buf_size) {
  int rc;

  mock9p_init(&fx->fs);
  mock_virtio_init(&fx->dev, ZBC_VIRTIO_ID_9P, 8);
  fx->dev.service = mock9p_service;
  fx->dev.service_ctx = &fx->fs;

  rc = zbc_9p_setup(&fx->p9, fx->dev.window.regs, fx->arena, sizeof(fx->arena),
                    fx->msgs, msg_buf_size);
  if (rc != ZBC_OK) {
    return rc;
  }
  mock_virtio_attach(&fx->dev, &fx->hook, 0, &fx->p9.vq);

  rc = zbc_9p_mount(&fx->p9);
  if (rc != ZBC_OK) {
    return rc;
  }

  zbc_client_init(&fx->client, (volatile void *)0);
  fx->client.transport = zbc_transport_9p();
  fx->client.transport_ctx = &fx->p9;
  zbc_api_init(&fx->api, &fx->client, fx->riff, sizeof(fx->riff));

  return ZBC_OK;
}

static int setup_9p(p9_fixture_t *fx) {
  return setup_9p_sized(fx, sizeof(fx->msgs));
}

/*------------------------------------------------------------------------
 * Test: mount succeeds; wrong device is refused
 *------------------------------------------------------------------------*/

static void test_9p_mount(void) {
  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);
  /* Tversion + Tattach */
  TEST_ASSERT_EQ(g_fx.fs.request_count, 2);

  {
    mock_virtio_t not_9p;
    zbc_9p_state_t p9;

    mock_virtio_init(&not_9p, ZBC_VIRTIO_ID_CONSOLE, 8);
    TEST_ASSERT_EQ(zbc_9p_setup(&p9, not_9p.window.regs, g_fx.arena,
                                sizeof(g_fx.arena), g_fx.msgs,
                                sizeof(g_fx.msgs)),
                   ZBC_ERR_DEVICE_ERROR);
  }
}

/*------------------------------------------------------------------------
 * Test: open an existing file, read it, check length, close it
 *------------------------------------------------------------------------*/

static void test_9p_open_read_close(void) {
  char dest[64];
  int fd;

  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);
  TEST_ASSERT(mock9p_add_file(&g_fx.fs, "hello.txt", "Hello, 9p!", 10) !=
              (mock9p_file_t *)0);

  fd = zbc_api_open(&g_fx.api, "hello.txt", SH_OPEN_R);
  TEST_ASSERT_EQ(fd, 3); /* user fds start at 3 */

  TEST_ASSERT_EQ((int)zbc_api_flen(&g_fx.api, fd), 10);

  memset(dest, 0, sizeof(dest));
  /* bytes NOT read: asked 64, file has 10 */
  TEST_ASSERT_EQ(zbc_api_read(&g_fx.api, fd, dest, sizeof(dest)), 54);
  TEST_ASSERT_MEM_EQ(dest, "Hello, 9p!", 10);

  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fd), 0);

  /* fd is dead after close */
  TEST_ASSERT_EQ(zbc_api_read(&g_fx.api, fd, dest, 4), -1);
  TEST_ASSERT_EQ(zbc_api_errno(&g_fx.api), ZBC_ERRNO_EBADF);
}

/*------------------------------------------------------------------------
 * Test: opening a missing file reports the server's ENOENT
 *------------------------------------------------------------------------*/

static void test_9p_open_missing(void) {
  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);

  TEST_ASSERT_EQ(zbc_api_open(&g_fx.api, "no-such-file", SH_OPEN_R), -1);
  TEST_ASSERT_EQ(zbc_api_errno(&g_fx.api), ZBC_ERRNO_ENOENT);
}

/*------------------------------------------------------------------------
 * Test: write mode creates the file; contents land in the server
 *------------------------------------------------------------------------*/

static void test_9p_create_write(void) {
  mock9p_file_t *file;
  int fd;

  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);

  fd = zbc_api_open(&g_fx.api, "new.txt", SH_OPEN_W);
  TEST_ASSERT(fd >= 3);

  /* bytes NOT written */
  TEST_ASSERT_EQ(zbc_api_write(&g_fx.api, fd, "created!", 8), 0);
  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fd), 0);

  file = mock9p_find(&g_fx.fs, "new.txt");
  TEST_ASSERT(file != (mock9p_file_t *)0);
  TEST_ASSERT_EQ((int)file->size, 8);
  TEST_ASSERT_MEM_EQ(file->data, "created!", 8);

  /* Reopening with SH_OPEN_W truncates */
  fd = zbc_api_open(&g_fx.api, "new.txt", SH_OPEN_W);
  TEST_ASSERT(fd >= 3);
  TEST_ASSERT_EQ((int)file->size, 0);
  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fd), 0);
}

/*------------------------------------------------------------------------
 * Test: append mode starts at end-of-file
 *------------------------------------------------------------------------*/

static void test_9p_append(void) {
  mock9p_file_t *file;
  int fd;

  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);
  file = mock9p_add_file(&g_fx.fs, "log.txt", "abc", 3);
  TEST_ASSERT(file != (mock9p_file_t *)0);

  fd = zbc_api_open(&g_fx.api, "log.txt", SH_OPEN_A);
  TEST_ASSERT(fd >= 3);
  TEST_ASSERT_EQ(zbc_api_write(&g_fx.api, fd, "def", 3), 0);
  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fd), 0);

  TEST_ASSERT_EQ((int)file->size, 6);
  TEST_ASSERT_MEM_EQ(file->data, "abcdef", 6);
}

/*------------------------------------------------------------------------
 * Test: seek is pure guest-side state; reads honor it
 *------------------------------------------------------------------------*/

static void test_9p_seek(void) {
  char dest[16];
  int fd;
  int before;

  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);
  TEST_ASSERT(mock9p_add_file(&g_fx.fs, "f.txt", "0123456789", 10) !=
              (mock9p_file_t *)0);

  fd = zbc_api_open(&g_fx.api, "f.txt", SH_OPEN_R);
  TEST_ASSERT(fd >= 3);

  before = g_fx.fs.request_count;
  TEST_ASSERT_EQ(zbc_api_seek(&g_fx.api, fd, 7), 0);
  /* SEEK generated no 9p traffic */
  TEST_ASSERT_EQ(g_fx.fs.request_count, before);

  memset(dest, 0, sizeof(dest));
  TEST_ASSERT_EQ(zbc_api_read(&g_fx.api, fd, dest, 3), 0);
  TEST_ASSERT_MEM_EQ(dest, "789", 3);

  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fd), 0);
}

/*------------------------------------------------------------------------
 * Test: remove deletes; removing a missing file reports ENOENT
 *------------------------------------------------------------------------*/

static void test_9p_remove(void) {
  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);
  TEST_ASSERT(mock9p_add_file(&g_fx.fs, "doomed", "x", 1) !=
              (mock9p_file_t *)0);

  TEST_ASSERT_EQ(zbc_api_remove(&g_fx.api, "doomed"), 0);
  TEST_ASSERT(mock9p_find(&g_fx.fs, "doomed") == (mock9p_file_t *)0);

  TEST_ASSERT_EQ(zbc_api_remove(&g_fx.api, "doomed"), -1);
  TEST_ASSERT_EQ(zbc_api_errno(&g_fx.api), ZBC_ERRNO_ENOENT);
}

/*------------------------------------------------------------------------
 * Test: rename, including into a subdirectory
 *------------------------------------------------------------------------*/

static void test_9p_rename(void) {
  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);
  mock9p_add_dir(&g_fx.fs, "sub");
  TEST_ASSERT(mock9p_add_file(&g_fx.fs, "a.txt", "data", 4) !=
              (mock9p_file_t *)0);

  TEST_ASSERT_EQ(zbc_api_rename(&g_fx.api, "a.txt", "b.txt"), 0);
  TEST_ASSERT(mock9p_find(&g_fx.fs, "a.txt") == (mock9p_file_t *)0);
  TEST_ASSERT(mock9p_find(&g_fx.fs, "b.txt") != (mock9p_file_t *)0);

  TEST_ASSERT_EQ(zbc_api_rename(&g_fx.api, "b.txt", "sub/c.txt"), 0);
  TEST_ASSERT(mock9p_find(&g_fx.fs, "sub/c.txt") != (mock9p_file_t *)0);

  /* Renaming a missing file fails */
  TEST_ASSERT_EQ(zbc_api_rename(&g_fx.api, "ghost", "d.txt"), -1);
  TEST_ASSERT_EQ(zbc_api_errno(&g_fx.api), ZBC_ERRNO_ENOENT);
}

/*------------------------------------------------------------------------
 * Test: paths with subdirectories walk component by component
 *------------------------------------------------------------------------*/

static void test_9p_subdir(void) {
  char dest[16];
  int fd;

  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);
  mock9p_add_dir(&g_fx.fs, "sub");
  TEST_ASSERT(mock9p_add_file(&g_fx.fs, "sub/inner.txt", "deep", 4) !=
              (mock9p_file_t *)0);

  fd = zbc_api_open(&g_fx.api, "sub/inner.txt", SH_OPEN_R);
  TEST_ASSERT(fd >= 3);
  memset(dest, 0, sizeof(dest));
  TEST_ASSERT_EQ(zbc_api_read(&g_fx.api, fd, dest, 4), 0);
  TEST_ASSERT_MEM_EQ(dest, "deep", 4);
  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fd), 0);

  /* Creating inside a subdirectory (leading slash tolerated) */
  fd = zbc_api_open(&g_fx.api, "/sub/created.txt", SH_OPEN_W);
  TEST_ASSERT(fd >= 3);
  TEST_ASSERT_EQ(zbc_api_write(&g_fx.api, fd, "ok", 2), 0);
  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fd), 0);
  TEST_ASSERT(mock9p_find(&g_fx.fs, "sub/created.txt") != (mock9p_file_t *)0);

  /* Missing intermediate directory */
  TEST_ASSERT_EQ(zbc_api_open(&g_fx.api, "nodir/x", SH_OPEN_R), -1);
  TEST_ASSERT_EQ(zbc_api_errno(&g_fx.api), ZBC_ERRNO_ENOENT);
}

/*------------------------------------------------------------------------
 * Test: I/O larger than msize loops in chunks
 *------------------------------------------------------------------------*/

static void test_9p_chunked_io(void) {
  uint8_t big[300];
  uint8_t dest[300];
  mock9p_file_t *file;
  int fd;
  int before;
  size_t i;

  /* Small message buffer: msize = 128, so chunk = 96 bytes */
  TEST_ASSERT_EQ(setup_9p_sized(&g_fx, 256), ZBC_OK);
  TEST_ASSERT_EQ((int)g_fx.p9.msize, 128);

  for (i = 0; i < sizeof(big); i++) {
    big[i] = (uint8_t)(i & 0xFF);
  }

  fd = zbc_api_open(&g_fx.api, "big.bin", SH_OPEN_WB);
  TEST_ASSERT(fd >= 3);
  before = g_fx.fs.request_count;
  TEST_ASSERT_EQ(zbc_api_write(&g_fx.api, fd, big, sizeof(big)), 0);
  /* 300 bytes through 96-byte chunks = 4 Twrite RPCs */
  TEST_ASSERT_EQ(g_fx.fs.request_count - before, 4);

  file = mock9p_find(&g_fx.fs, "big.bin");
  TEST_ASSERT(file != (mock9p_file_t *)0);
  TEST_ASSERT_EQ((int)file->size, (int)sizeof(big));
  TEST_ASSERT_MEM_EQ(file->data, big, sizeof(big));

  TEST_ASSERT_EQ(zbc_api_seek(&g_fx.api, fd, 0), 0);
  memset(dest, 0, sizeof(dest));
  TEST_ASSERT_EQ(zbc_api_read(&g_fx.api, fd, dest, sizeof(dest)), 0);
  TEST_ASSERT_MEM_EQ(dest, big, sizeof(big));

  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fd), 0);
}

/*------------------------------------------------------------------------
 * Test: fd table exhaustion reports EMFILE; slots are reusable
 *------------------------------------------------------------------------*/

static void test_9p_fd_exhaustion(void) {
  int fds[ZBC_9P_MAX_FILES];
  int i;

  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);
  TEST_ASSERT(mock9p_add_file(&g_fx.fs, "f", "x", 1) != (mock9p_file_t *)0);

  for (i = 0; i < ZBC_9P_MAX_FILES; i++) {
    fds[i] = zbc_api_open(&g_fx.api, "f", SH_OPEN_R);
    TEST_ASSERT_EQ(fds[i], i + 3);
  }
  TEST_ASSERT_EQ(zbc_api_open(&g_fx.api, "f", SH_OPEN_R), -1);
  TEST_ASSERT_EQ(zbc_api_errno(&g_fx.api), ZBC_ERRNO_EMFILE);

  TEST_ASSERT_EQ(zbc_api_close(&g_fx.api, fds[0]), 0);
  TEST_ASSERT_EQ(zbc_api_open(&g_fx.api, "f", SH_OPEN_R), 3);
}

/*------------------------------------------------------------------------
 * Test: tmpnam, istty, and ENOSYS for non-file opcodes
 *------------------------------------------------------------------------*/

static void test_9p_misc(void) {
  char name[32];
  uintptr_t args[3];
  zbc_response_t response;

  TEST_ASSERT_EQ(setup_9p(&g_fx), ZBC_OK);

  TEST_ASSERT_EQ(zbc_api_tmpnam(&g_fx.api, name, sizeof(name), 0), 0);
  TEST_ASSERT_MEM_EQ(name, "tmp00000_000", 13);
  TEST_ASSERT_EQ(zbc_api_tmpnam(&g_fx.api, name, sizeof(name), 7), 0);
  TEST_ASSERT_MEM_EQ(name, "tmp00001_007", 13);

  args[0] = 1; /* stdout is a tty even to the file transport */
  TEST_ASSERT_EQ(zbc_call(&response, &g_fx.client, g_fx.riff, sizeof(g_fx.riff),
                          SH_SYS_ISTTY, args),
                 ZBC_OK);
  TEST_ASSERT_EQ(response.result, 1);

  /* Console output belongs to the console transport */
  args[0] = (uintptr_t) "text";
  TEST_ASSERT_EQ(zbc_call(&response, &g_fx.client, g_fx.riff, sizeof(g_fx.riff),
                          SH_SYS_WRITE0, args),
                 ZBC_OK);
  TEST_ASSERT_EQ(response.result, -1);
  TEST_ASSERT_EQ(response.error_code, ZBC_ERRNO_ENOSYS);
}

/*------------------------------------------------------------------------
 * Suite runner
 *------------------------------------------------------------------------*/

void run_9p_tests(void) {
  BEGIN_SUITE("virtio-9p Transport");

  RUN_TEST(9p_mount);
  RUN_TEST(9p_open_read_close);
  RUN_TEST(9p_open_missing);
  RUN_TEST(9p_create_write);
  RUN_TEST(9p_append);
  RUN_TEST(9p_seek);
  RUN_TEST(9p_remove);
  RUN_TEST(9p_rename);
  RUN_TEST(9p_subdir);
  RUN_TEST(9p_chunked_io);
  RUN_TEST(9p_fd_exhaustion);
  RUN_TEST(9p_misc);

  END_SUITE();
}
