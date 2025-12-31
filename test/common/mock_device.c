/*
 * Mock Device - Implementation
 */

#include "mock_device.h"
#include "mock_memory.h"
#include <string.h>

/*------------------------------------------------------------------------
 * Internal: mock memory ops that read/write directly to riff_buf
 *------------------------------------------------------------------------*/

static uint8_t mock_dev_read_u8(uintptr_t addr, void *ctx) {
  mock_device_t *dev = (mock_device_t *)ctx;
  if (!dev || !dev->riff_buf)
    return 0;
  return dev->riff_buf[addr];
}

static void mock_dev_write_u8(uintptr_t addr, uint8_t val, void *ctx) {
  mock_device_t *dev = (mock_device_t *)ctx;
  if (!dev || !dev->riff_buf)
    return;
  dev->riff_buf[addr] = val;
}

static void mock_dev_read_block(void *dest, uintptr_t addr, size_t size,
                                void *ctx) {
  mock_device_t *dev = (mock_device_t *)ctx;
  if (!dev || !dev->riff_buf || !dest)
    return;
  memcpy(dest, dev->riff_buf + addr, size);
}

static void mock_dev_write_block(uintptr_t addr, const void *src, size_t size,
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

  /* Set up signature */
  mock_device_set_signature(dev);

  /* Initialize host state with mock memory ops and dummy backend */
  mem_ops.read_u8 = mock_dev_read_u8;
  mem_ops.write_u8 = mock_dev_write_u8;
  mem_ops.read_block = mock_dev_read_block;
  mem_ops.write_block = mock_dev_write_block;

  zbc_host_init(&dev->host_state, &mem_ops, dev,
                zbc_backend_dummy(), NULL,
                dev->work_buf, sizeof(dev->work_buf));
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

}

