/*
 * Client Builder Tests
 *
 * Tests for RIFF structure building functions.
 */

#include "test_harness.h"
#include "zbc_semi_client.h"
#include "mock_device.h"

/*------------------------------------------------------------------------
 * Helper: Create a fake client state pointing to mock device
 *------------------------------------------------------------------------*/

static void setup_client_state(zbc_client_state_t *state, mock_device_t *dev)
{
    zbc_client_init(state, dev->regs);
}

/*------------------------------------------------------------------------
 * Basic Builder Tests
 *------------------------------------------------------------------------*/

static void test_builder_start_empty(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Check RIFF header written */
    TEST_ASSERT_EQ(buf[0], 'R');
    TEST_ASSERT_EQ(buf[1], 'I');
    TEST_ASSERT_EQ(buf[2], 'F');
    TEST_ASSERT_EQ(buf[3], 'F');

    /* Check SEMI type */
    TEST_ASSERT_EQ(buf[8], 'S');
    TEST_ASSERT_EQ(buf[9], 'E');
    TEST_ASSERT_EQ(buf[10], 'M');
    TEST_ASSERT_EQ(buf[11], 'I');

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_start_with_cnfg(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);

    /* First call should include CNFG */
    TEST_ASSERT_EQ(state.cnfg_sent, 0);

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* CNFG should be at offset 12 */
    TEST_ASSERT_EQ(buf[12], 'C');
    TEST_ASSERT_EQ(buf[13], 'N');
    TEST_ASSERT_EQ(buf[14], 'F');
    TEST_ASSERT_EQ(buf[15], 'G');

    /* CNFG size should be 4 */
    TEST_ASSERT_EQ(buf[16], 4);
    TEST_ASSERT_EQ(buf[17], 0);
    TEST_ASSERT_EQ(buf[18], 0);
    TEST_ASSERT_EQ(buf[19], 0);

    /* CNFG data: int_size, ptr_size, endianness, reserved */
    TEST_ASSERT_EQ(buf[20], sizeof(int));
    TEST_ASSERT_EQ(buf[21], sizeof(void *));

    /* cnfg_sent should now be set */
    TEST_ASSERT_EQ(state.cnfg_sent, 1);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_cnfg_not_duplicated(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);

    /* First call includes CNFG */
    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);
    TEST_ASSERT_EQ(state.cnfg_sent, 1);

    /* Reset buffer */
    memset(buf, 0, buf_size);

    /* Second call should NOT include CNFG */
    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Offset 12 should NOT be CNFG (since it's already sent) */
    /* Builder offset should be smaller */
    TEST_ASSERT_EQ(builder.offset, ZBC_HDR_SIZE);  /* Just RIFF header, no CNFG */

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_begin_call(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;
    size_t call_offset;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);
    state.cnfg_sent = 1;  /* Skip CNFG for this test */

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    call_offset = builder.offset;

    rc = zbc_builder_begin_call(&builder, SH_SYS_CLOSE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Check CALL chunk */
    TEST_ASSERT_EQ(buf[call_offset + 0], 'C');
    TEST_ASSERT_EQ(buf[call_offset + 1], 'A');
    TEST_ASSERT_EQ(buf[call_offset + 2], 'L');
    TEST_ASSERT_EQ(buf[call_offset + 3], 'L');

    /* Opcode at offset 8 within CALL */
    TEST_ASSERT_EQ(buf[call_offset + 8], SH_SYS_CLOSE);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_add_parm_int(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;
    size_t parm_offset;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);
    state.cnfg_sent = 1;

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_CLOSE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    parm_offset = builder.offset;

    rc = zbc_builder_add_parm_int(&builder, 42);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Check PARM chunk */
    TEST_ASSERT_EQ(buf[parm_offset + 0], 'P');
    TEST_ASSERT_EQ(buf[parm_offset + 1], 'A');
    TEST_ASSERT_EQ(buf[parm_offset + 2], 'R');
    TEST_ASSERT_EQ(buf[parm_offset + 3], 'M');

    /* Type should be INT */
    TEST_ASSERT_EQ(buf[parm_offset + 8], ZBC_PARM_TYPE_INT);

    /* Value should be 42 (little-endian on LE systems) */
    TEST_ASSERT_EQ(buf[parm_offset + 12], 42);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_add_data_binary(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;
    size_t data_offset;
    uint8_t test_data[] = {0x11, 0x22, 0x33, 0x44, 0x55};

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);
    state.cnfg_sent = 1;

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_WRITE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    data_offset = builder.offset;

    rc = zbc_builder_add_data_binary(&builder, test_data, sizeof(test_data));
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Check DATA chunk */
    TEST_ASSERT_EQ(buf[data_offset + 0], 'D');
    TEST_ASSERT_EQ(buf[data_offset + 1], 'A');
    TEST_ASSERT_EQ(buf[data_offset + 2], 'T');
    TEST_ASSERT_EQ(buf[data_offset + 3], 'A');

    /* Type should be BINARY */
    TEST_ASSERT_EQ(buf[data_offset + 8], ZBC_DATA_TYPE_BINARY);

    /* Payload should match */
    TEST_ASSERT_MEM_EQ(buf + data_offset + 12, test_data, sizeof(test_data));

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_add_data_string(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;
    size_t data_offset;
    const char *test_str = "hello";

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);
    state.cnfg_sent = 1;

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_OPEN);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    data_offset = builder.offset;

    rc = zbc_builder_add_data_string(&builder, test_str);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Check DATA chunk */
    TEST_ASSERT_EQ(buf[data_offset + 0], 'D');
    TEST_ASSERT_EQ(buf[data_offset + 1], 'A');
    TEST_ASSERT_EQ(buf[data_offset + 2], 'T');
    TEST_ASSERT_EQ(buf[data_offset + 3], 'A');

    /* Type should be STRING */
    TEST_ASSERT_EQ(buf[data_offset + 8], ZBC_DATA_TYPE_STRING);

    /* Payload should match including null terminator */
    TEST_ASSERT_MEM_EQ(buf + data_offset + 12, test_str, 6);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_finish_patches_sizes(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;
    size_t riff_size;
    uint32_t stored_riff_size;
    uint32_t stored_call_size;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);
    state.cnfg_sent = 1;

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_CLOSE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_add_parm_int(&builder, 5);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* RIFF size at offset 4 should be total - 8 */
    stored_riff_size = ZBC_READ_U32_LE(buf + 4);
    TEST_ASSERT_EQ(stored_riff_size, riff_size - 8);

    /* CALL size should be correct */
    stored_call_size = ZBC_READ_U32_LE(buf + ZBC_HDR_SIZE + 4);
    /* CALL header data (4 bytes) + PARM chunk */
    TEST_ASSERT(stored_call_size > 0);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Buffer Boundary Tests
 *------------------------------------------------------------------------*/

static void test_builder_buffer_too_small_for_header(void)
{
    GUARDED_BUF(buf, 8);  /* Too small for RIFF header */
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_ERR_BUFFER_TOO_SMALL);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_buffer_too_small_for_parm(void)
{
    /* RIFF header (12) + CALL header (12) = 24 bytes used after begin_call
     * PARM chunk needs: 8 (chunk hdr) + 4 (type+reserved) + int_size(4) = 16
     * Total needed = 40 bytes. Use 39 to be 1 byte short. */
    GUARDED_BUF(buf, 39);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);
    state.cnfg_sent = 1;

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_CLOSE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Try to add PARM - should fail (need 16 bytes but only have 15) */
    rc = zbc_builder_add_parm_int(&builder, 5);
    TEST_ASSERT_EQ(rc, ZBC_ERR_BUFFER_TOO_SMALL);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_sticky_error(void)
{
    /* Use 39 bytes - same as test_builder_buffer_too_small_for_parm */
    GUARDED_BUF(buf, 39);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;
    size_t riff_size;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);
    state.cnfg_sent = 1;

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_CLOSE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Force an error by trying to add PARM that won't fit */
    rc = zbc_builder_add_parm_int(&builder, 5);
    TEST_ASSERT_EQ(rc, ZBC_ERR_BUFFER_TOO_SMALL);

    /* Subsequent operations should also fail due to sticky error */
    rc = zbc_builder_finish(&builder, &riff_size);
    TEST_ASSERT_EQ(rc, ZBC_ERR_BUFFER_TOO_SMALL);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Edge Cases
 *------------------------------------------------------------------------*/

static void test_builder_null_buffer(void)
{
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;

    mock_device_init(&dev);
    setup_client_state(&state, &dev);

    rc = zbc_builder_start(&builder, NULL, 256, &state);
    TEST_ASSERT_EQ(rc, ZBC_ERR_INVALID_ARG);
}

static void test_builder_null_state(void)
{
    GUARDED_BUF(buf, 256);
    zbc_builder_t builder;
    int rc;

    GUARDED_INIT(buf);

    rc = zbc_builder_start(&builder, buf, buf_size, NULL);
    TEST_ASSERT_EQ(rc, ZBC_ERR_INVALID_ARG);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

static void test_builder_data_empty(void)
{
    GUARDED_BUF(buf, 256);
    zbc_client_state_t state;
    zbc_builder_t builder;
    mock_device_t dev;
    int rc;

    GUARDED_INIT(buf);
    mock_device_init(&dev);
    setup_client_state(&state, &dev);
    state.cnfg_sent = 1;

    rc = zbc_builder_start(&builder, buf, buf_size, &state);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    rc = zbc_builder_begin_call(&builder, SH_SYS_WRITE);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    /* Empty data should succeed */
    rc = zbc_builder_add_data_binary(&builder, NULL, 0);
    TEST_ASSERT_EQ(rc, ZBC_OK);

    TEST_ASSERT_EQ(GUARDED_CHECK(buf), 0);
}

/*------------------------------------------------------------------------
 * Run all builder tests
 *------------------------------------------------------------------------*/

void run_client_builder_tests(void)
{
    BEGIN_SUITE("Client Builder");

    RUN_TEST(builder_start_empty);
    RUN_TEST(builder_start_with_cnfg);
    RUN_TEST(builder_cnfg_not_duplicated);
    RUN_TEST(builder_begin_call);
    RUN_TEST(builder_add_parm_int);
    RUN_TEST(builder_add_data_binary);
    RUN_TEST(builder_add_data_string);
    RUN_TEST(builder_finish_patches_sizes);
    RUN_TEST(builder_buffer_too_small_for_header);
    RUN_TEST(builder_buffer_too_small_for_parm);
    RUN_TEST(builder_sticky_error);
    RUN_TEST(builder_null_buffer);
    RUN_TEST(builder_null_state);
    RUN_TEST(builder_data_empty);

    END_SUITE();
}
