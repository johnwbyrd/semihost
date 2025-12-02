/*
 * Mock Device - Implementation
 */

#include "mock_device.h"
#include "mock_memory.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Internal: mock memory ops that read/write directly to riff_buf
 *------------------------------------------------------------------------*/

static uint8_t mock_dev_read_u8(uint64_t addr, void *ctx) {
  mock_device_t *dev = (mock_device_t *)ctx;
  if (!dev || !dev->riff_buf)
    return 0;
  return dev->riff_buf[addr];
}

static void mock_dev_write_u8(uint64_t addr, uint8_t val, void *ctx) {
  mock_device_t *dev = (mock_device_t *)ctx;
  if (!dev || !dev->riff_buf)
    return;
  dev->riff_buf[addr] = val;
}

static void mock_dev_read_block(void *dest, uint64_t addr, size_t size,
                                void *ctx) {
  mock_device_t *dev = (mock_device_t *)ctx;
  if (!dev || !dev->riff_buf || !dest)
    return;
  memcpy(dest, dev->riff_buf + addr, size);
}

static void mock_dev_write_block(uint64_t addr, const void *src, size_t size,
                                 void *ctx) {
  mock_device_t *dev = (mock_device_t *)ctx;
  if (!dev || !dev->riff_buf || !src)
    return;
  memcpy(dev->riff_buf + addr, src, size);
}

/*------------------------------------------------------------------------
 * Initialization
 *------------------------------------------------------------------------*/

void mock_device_init(mock_device_t *dev) {
  zbc_host_mem_ops_t mem_ops;

  if (!dev)
    return;

  /* Clear everything */
  memset(dev, 0, sizeof(*dev));

  /* Set up signature and status */
  mock_device_set_signature(dev);
  mock_device_set_present(dev);

  /* Initialize host state with mock memory ops */
  mem_ops.read_u8 = mock_dev_read_u8;
  mem_ops.write_u8 = mock_dev_write_u8;
  mem_ops.read_block = mock_dev_read_block;
  mem_ops.write_block = mock_dev_write_block;

  zbc_host_init(&dev->host_state, &mem_ops, dev, dev->work_buf,
                sizeof(dev->work_buf));
}

void mock_device_set_signature(mock_device_t *dev) {
  if (!dev)
    return;

  dev->regs[ZBC_REG_SIGNATURE + 0] = ZBC_SIGNATURE_BYTE0; /* 'S' */
  dev->regs[ZBC_REG_SIGNATURE + 1] = ZBC_SIGNATURE_BYTE1; /* 'E' */
  dev->regs[ZBC_REG_SIGNATURE + 2] = ZBC_SIGNATURE_BYTE2; /* 'M' */
  dev->regs[ZBC_REG_SIGNATURE + 3] = ZBC_SIGNATURE_BYTE3; /* 'I' */
  dev->regs[ZBC_REG_SIGNATURE + 4] = ZBC_SIGNATURE_BYTE4; /* 'H' */
  dev->regs[ZBC_REG_SIGNATURE + 5] = ZBC_SIGNATURE_BYTE5; /* 'O' */
  dev->regs[ZBC_REG_SIGNATURE + 6] = ZBC_SIGNATURE_BYTE6; /* 'S' */
  dev->regs[ZBC_REG_SIGNATURE + 7] = ZBC_SIGNATURE_BYTE7; /* 'T' */
}

void mock_device_set_present(mock_device_t *dev) {
  if (!dev)
    return;

  dev->regs[ZBC_REG_STATUS] = ZBC_STATUS_DEVICE_PRESENT;
}

/*------------------------------------------------------------------------
 * Doorbell handling
 *------------------------------------------------------------------------*/

void mock_device_doorbell(mock_device_t *dev) {
  uintptr_t ptr_val;
  size_t i;

  if (!dev)
    return;

  dev->doorbell_count++;

  /* Extract pointer from RIFF_PTR register (native endianness) */
  ptr_val = 0;
  for (i = 0; i < sizeof(uintptr_t) && i < 16; i++) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    ptr_val = (ptr_val << 8) |
              dev->regs[ZBC_REG_RIFF_PTR + sizeof(uintptr_t) - 1 - i];
#else
    ptr_val |= ((uintptr_t)dev->regs[ZBC_REG_RIFF_PTR + i]) << (i * 8);
#endif
  }

  dev->riff_buf = (uint8_t *)ptr_val;

  /* Process the request if we have a valid buffer */
  if (dev->riff_buf) {
    if (dev->custom_handler) {
      dev->custom_handler(dev);
    } else {
      /* Use host library to process - addr=0 since mem_ops offset from riff_buf
       */
      zbc_host_process(&dev->host_state, 0);
    }
    dev->process_count++;
  }

  /* Set RESPONSE_READY in STATUS */
  dev->regs[ZBC_REG_STATUS] |= ZBC_STATUS_RESPONSE_READY;
}

void mock_device_set_handler(mock_device_t *dev, uint8_t opcode,
                             zbc_syscall_handler_t handler) {
  if (!dev)
    return;
  zbc_host_set_handler(&dev->host_state, opcode, handler);
}

/*------------------------------------------------------------------------
 * Built-in test handlers
 *------------------------------------------------------------------------*/

int mock_handler_return_42(zbc_syscall_ctx_t *ctx,
                           zbc_syscall_result_t *result) {
  (void)ctx;
  result->result = 42;
  result->error = 0;
  result->data = NULL;
  result->data_size = 0;
  result->parm_count = 0;
  return 0;
}

int mock_handler_echo(zbc_syscall_ctx_t *ctx, zbc_syscall_result_t *result) {
  /* Echo back the first DATA chunk received */
  result->result = 0;
  result->error = 0;
  result->parm_count = 0;

  if (ctx->data_count > 0 && ctx->data[0].size > 0) {
    result->data = ctx->data[0].data;
    result->data_size = ctx->data[0].size;
    result->result = (int64_t)ctx->data[0].size;
  } else {
    result->data = NULL;
    result->data_size = 0;
  }

  return 0;
}

int mock_handler_error(zbc_syscall_ctx_t *ctx, zbc_syscall_result_t *result) {
  (void)ctx;
  result->result = -1;
  result->error = 5; /* EIO */
  result->data = NULL;
  result->data_size = 0;
  result->parm_count = 0;
  return 0;
}

int mock_handler_heapinfo(zbc_syscall_ctx_t *ctx,
                          zbc_syscall_result_t *result) {
  (void)ctx;

  result->result = 0;
  result->error = 0;
  result->data = NULL;
  result->data_size = 0;

  /* Return 4 test pointers as PARM chunks */
  result->parm_count = 4;
  result->parm_types[0] = ZBC_PARM_TYPE_PTR;
  result->parm_types[1] = ZBC_PARM_TYPE_PTR;
  result->parm_types[2] = ZBC_PARM_TYPE_PTR;
  result->parm_types[3] = ZBC_PARM_TYPE_PTR;

  result->parm_values[0] = 0x20001000; /* heap_base */
  result->parm_values[1] = 0x20010000; /* heap_limit */
  result->parm_values[2] = 0x20020000; /* stack_base */
  result->parm_values[3] = 0x2002F000; /* stack_limit */

  return 0;
}
