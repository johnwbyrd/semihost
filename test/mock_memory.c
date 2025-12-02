/*
 * Mock Memory - Implementation
 */

#include "mock_memory.h"

void mock_memory_init(mock_memory_t *mem, uint8_t *buf, size_t size) {
  if (!mem)
    return;

  mem->data = buf;
  mem->size = size;
  mem->read_count = 0;
  mem->write_count = 0;
}

void mock_memory_reset_counters(mock_memory_t *mem) {
  if (!mem)
    return;

  mem->read_count = 0;
  mem->write_count = 0;
}

void mock_memory_fill_pattern(mock_memory_t *mem) {
  size_t i;

  if (!mem || !mem->data)
    return;

  for (i = 0; i < mem->size; i++) {
    mem->data[i] = (uint8_t)(i & 0xFF);
  }
}

uint8_t mock_mem_read_u8(uint64_t addr, void *ctx) {
  mock_memory_t *mem = (mock_memory_t *)ctx;

  if (!mem || !mem->data)
    return 0;

  mem->read_count++;

  if (addr >= mem->size) {
    return 0; /* Out of bounds */
  }

  return mem->data[addr];
}

void mock_mem_write_u8(uint64_t addr, uint8_t val, void *ctx) {
  mock_memory_t *mem = (mock_memory_t *)ctx;

  if (!mem || !mem->data)
    return;

  mem->write_count++;

  if (addr >= mem->size) {
    return; /* Out of bounds */
  }

  mem->data[addr] = val;
}

void mock_mem_read_block(void *dest, uint64_t addr, size_t size, void *ctx) {
  mock_memory_t *mem = (mock_memory_t *)ctx;
  size_t i;
  uint8_t *d;

  if (!mem || !mem->data || !dest)
    return;

  mem->read_count++;

  d = (uint8_t *)dest;
  for (i = 0; i < size; i++) {
    if (addr + i < mem->size) {
      d[i] = mem->data[addr + i];
    } else {
      d[i] = 0;
    }
  }
}

void mock_mem_write_block(uint64_t addr, const void *src, size_t size,
                          void *ctx) {
  mock_memory_t *mem = (mock_memory_t *)ctx;
  size_t i;
  const uint8_t *s;

  if (!mem || !mem->data || !src)
    return;

  mem->write_count++;

  s = (const uint8_t *)src;
  for (i = 0; i < size; i++) {
    if (addr + i < mem->size) {
      mem->data[addr + i] = s[i];
    }
  }
}
