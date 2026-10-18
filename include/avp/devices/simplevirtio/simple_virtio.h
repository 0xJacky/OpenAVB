#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
  SIMPLE_VIRTIO_ROOT_BLOCK_BASE = 0x30000000,
  SIMPLE_VIRTIO_UTIL_BLOCK_BASE = 0x30010000,
  SIMPLE_VIRTIO_USB_BASE = 0x30100000,
  SIMPLE_VIRTIO_QUEUE_CAPACITY = 0x80e,
  SIMPLE_VIRTIO_TAG = 1,
  SIMPLE_VIRTIO_NEXT = 1,
  SIMPLE_VIRTIO_READABLE = 0,
  SIMPLE_VIRTIO_WRITABLE = 2,
  SIMPLE_VIRTIO_COMPLETE = 0x8000,
};

typedef enum {
  SIMPLE_VIRTIO_BLOCK_SLOT_ROOT = 0,
  SIMPLE_VIRTIO_BLOCK_SLOT_UTIL = 1,
} SimpleVirtioBlockSlot;

typedef struct {
  uint64_t ipa;
  uint32_t size;
  uint16_t tag;
  uint16_t flags;
} SimpleVirtioSegment;

_Static_assert(sizeof(SimpleVirtioSegment) == 16, "SimpleVirtioSegment size");

typedef struct {
  volatile SimpleVirtioSegment *queue0;
  volatile SimpleVirtioSegment *queue1;
  uint8_t *queue0_buffer;
  uint8_t *queue1_buffer;
} SimpleVirtioUSB;

typedef struct {
  uint32_t type;
  uint32_t reserved;
  uint64_t sector;
} SimpleVirtioBlockHeader;

_Static_assert(sizeof(SimpleVirtioBlockHeader) == 16,
               "SimpleVirtioBlockHeader size");

typedef struct SimpleVirtioBlock SimpleVirtioBlock;

typedef struct {
  bool (*read)(
    const SimpleVirtioBlock *block,
    uint64_t block_index,
    uint32_t block_count,
    void *buffer
  );
  bool (*write)(
    const SimpleVirtioBlock *block,
    uint64_t block_index,
    uint32_t block_count,
    const void *buffer
  );
} SimpleVirtioBlockOps;

struct SimpleVirtioBlock {
  uintptr_t base;
  uint64_t sector_count;
  uint32_t block_size;
  const SimpleVirtioBlockOps *ops;
  uint8_t slot;
};

bool simple_virtio_block_initialize(SimpleVirtioBlock *block, uintptr_t base);
bool simple_virtio_block_deinitialize(const SimpleVirtioBlock *block);
bool simple_virtio_block_domain_initialize(
  SimpleVirtioBlock *block,
  SimpleVirtioBlockSlot slot
);
void simple_virtio_block_build_request(
  const SimpleVirtioBlock *block,
  SimpleVirtioSegment descriptors[3],
  SimpleVirtioBlockHeader *header,
  uint8_t *status,
  uint64_t data_ipa,
  uint64_t block_index,
  uint32_t block_count,
  bool write
);
bool simple_virtio_block_read(
  const SimpleVirtioBlock *block,
  uint64_t block_index,
  uint32_t block_count,
  void *buffer
);
bool simple_virtio_block_write(
  const SimpleVirtioBlock *block,
  uint64_t block_index,
  uint32_t block_count,
  const void *buffer
);

void simple_virtio_usb_initialize(SimpleVirtioUSB *usb);
void simple_virtio_usb_shutdown(void);
void simple_virtio_usb_attach(SimpleVirtioUSB *usb);
void simple_virtio_usb_post_event(SimpleVirtioUSB *usb);
uint32_t simple_virtio_usb_wait_event(SimpleVirtioUSB *usb);
void simple_virtio_usb_respond(SimpleVirtioUSB *usb, uint32_t response_size);
