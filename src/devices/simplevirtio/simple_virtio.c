#include "avp/devices/simplevirtio/simple_virtio.h"
#include "avp/platform/mmio.h"

enum {
  QUEUE0_DESCRIPTOR_IPA = 0x70001000,
  QUEUE0_BUFFER_IPA = 0x70002000,
  QUEUE1_DESCRIPTOR_IPA = 0x70003000,
  QUEUE1_BUFFER_IPA = 0x70004000,
  BLOCK_DESCRIPTOR_IPA = 0x70006000,
  BLOCK_HEADER_IPA = 0x70006100,
  BLOCK_STATUS_IPA = 0x70006110,
};

static const SimpleVirtioBlockOps direct_block_ops;

bool simple_virtio_block_initialize(SimpleVirtioBlock *block, uintptr_t base) {
  if (mmio_read32(base) != 1) {
    return false;
  }
  mmio_write32(base + 0x400, 1);
  if (mmio_read32(base + 0x400) != 1 || mmio_read32(base + 4) != 2) {
    return false;
  }
  const uint64_t configuration_offset = mmio_read64(base + 8);
  if (configuration_offset >= 0x10000) {
    return false;
  }
  const uint64_t sector_count = mmio_read64(base + configuration_offset);
  uint32_t block_size = mmio_read32(base + configuration_offset + 0x14);
  if (block_size == 0) {
    block_size = 512;
  }
  if ((block_size & 0x1ff) != 0) {
    return false;
  }
  mmio_write32(base + 0x404, 3);
  *block = (SimpleVirtioBlock){
      .base = base,
      .sector_count = sector_count,
      .block_size = block_size,
      .ops = &direct_block_ops,
      .slot = base == SIMPLE_VIRTIO_UTIL_BLOCK_BASE
                  ? SIMPLE_VIRTIO_BLOCK_SLOT_UTIL
                  : SIMPLE_VIRTIO_BLOCK_SLOT_ROOT,
  };
  return true;
}

bool simple_virtio_block_deinitialize(const SimpleVirtioBlock *block) {
  mmio_write32(block->base + 0x404, 0);
  avp_memory_barrier();
  return mmio_read32(block->base + 0x404) == 0;
}

void simple_virtio_block_build_request(const SimpleVirtioBlock *block,
                                       SimpleVirtioSegment descriptors[3],
                                       SimpleVirtioBlockHeader *header,
                                       uint8_t *status,
                                       uint64_t data_ipa,
                                       uint64_t block_index,
                                       uint32_t block_count,
                                       bool write) {
  const uint64_t sectors_per_block = block->block_size / 512;
  *header = (SimpleVirtioBlockHeader){
      .type = write ? 1U : 0U,
      .reserved = 0,
      .sector = block_index * sectors_per_block,
  };
  *status = UINT8_MAX;
  descriptors[0] = (SimpleVirtioSegment){
      .ipa = BLOCK_HEADER_IPA,
      .size = sizeof(*header),
      .tag = SIMPLE_VIRTIO_TAG,
      .flags = SIMPLE_VIRTIO_READABLE | SIMPLE_VIRTIO_NEXT,
  };
  descriptors[1] = (SimpleVirtioSegment){
      .ipa = data_ipa,
      .size = block_count * block->block_size,
      .tag = SIMPLE_VIRTIO_TAG,
      .flags = (write ? SIMPLE_VIRTIO_READABLE : SIMPLE_VIRTIO_WRITABLE) |
               SIMPLE_VIRTIO_NEXT,
  };
  descriptors[2] = (SimpleVirtioSegment){
      .ipa = BLOCK_STATUS_IPA,
      .size = 1,
      .tag = SIMPLE_VIRTIO_TAG,
      .flags = SIMPLE_VIRTIO_WRITABLE,
  };
}

static bool simple_virtio_block_transfer_direct(const SimpleVirtioBlock *block,
                                                uint64_t block_index,
                                                uint32_t block_count,
                                                const void *buffer,
                                                bool write) {
  const uint64_t sectors_per_block = block->block_size / 512;
  const uint64_t capacity_blocks = block->sector_count / sectors_per_block;
  if (block_count == 0 || block_index > capacity_blocks ||
      block_count > capacity_blocks - block_index ||
      block_count > UINT32_MAX / block->block_size) {
    return false;
  }
  SimpleVirtioSegment *descriptors =
      (SimpleVirtioSegment *)BLOCK_DESCRIPTOR_IPA;
  SimpleVirtioBlockHeader *header = (SimpleVirtioBlockHeader *)BLOCK_HEADER_IPA;
  uint8_t *status = (uint8_t *)BLOCK_STATUS_IPA;
  simple_virtio_block_build_request(block,
                                    descriptors,
                                    header,
                                    status,
                                    (uintptr_t)buffer,
                                    block_index,
                                    block_count,
                                    write);
  avp_memory_barrier();
  mmio_write64(block->base + 0x408, BLOCK_DESCRIPTOR_IPA);
  while (mmio_read32(block->base + 0x10) == 0) {
  }
  avp_memory_barrier();
  return *status == 0;
}

static bool simple_virtio_block_read_direct(const SimpleVirtioBlock *block,
                                            uint64_t block_index,
                                            uint32_t block_count,
                                            void *buffer) {
  return simple_virtio_block_transfer_direct(block, block_index, block_count, buffer, false);
}

static bool simple_virtio_block_write_direct(const SimpleVirtioBlock *block,
                                             uint64_t block_index,
                                             uint32_t block_count,
                                             const void *buffer) {
  return simple_virtio_block_transfer_direct(block, block_index, block_count, buffer, true);
}

static const SimpleVirtioBlockOps direct_block_ops = {
    .read = simple_virtio_block_read_direct,
    .write = simple_virtio_block_write_direct,
};

bool __attribute__((used, retain))
simple_virtio_block_read(const SimpleVirtioBlock *block,
                         uint64_t block_index,
                         uint32_t block_count,
                         void *buffer) {
  return block->ops->read(block, block_index, block_count, buffer);
}

bool __attribute__((used, retain))
simple_virtio_block_write(const SimpleVirtioBlock *block,
                          uint64_t block_index,
                          uint32_t block_count,
                          const void *buffer) {
  return block->ops->write(block, block_index, block_count, buffer);
}

static void select_queue(uint32_t queue) {
  mmio_write32(SIMPLE_VIRTIO_USB_BASE + 0x410, queue);
}

static void commit_descriptor(uint64_t descriptor) {
  mmio_write64(SIMPLE_VIRTIO_USB_BASE + 0x408, descriptor);
}

static void post_queue0(SimpleVirtioUSB *usb) {
  usb->queue0->ipa = QUEUE0_BUFFER_IPA;
  usb->queue0->size = SIMPLE_VIRTIO_QUEUE_CAPACITY;
  usb->queue0->tag = SIMPLE_VIRTIO_TAG;
  usb->queue0->flags = SIMPLE_VIRTIO_WRITABLE;
  avp_memory_barrier();
  select_queue(0);
  commit_descriptor(QUEUE0_DESCRIPTOR_IPA);
}

void simple_virtio_usb_attach(SimpleVirtioUSB *usb) {
  usb->queue0 = (volatile SimpleVirtioSegment *)QUEUE0_DESCRIPTOR_IPA;
  usb->queue1 = (volatile SimpleVirtioSegment *)QUEUE1_DESCRIPTOR_IPA;
  usb->queue0_buffer = (uint8_t *)QUEUE0_BUFFER_IPA;
  usb->queue1_buffer = (uint8_t *)QUEUE1_BUFFER_IPA;
}

void simple_virtio_usb_post_event(SimpleVirtioUSB *usb) { post_queue0(usb); }

void simple_virtio_usb_initialize(SimpleVirtioUSB *usb) {
  simple_virtio_usb_attach(usb);

  const uint64_t capabilities = mmio_read64(SIMPLE_VIRTIO_USB_BASE + 0x008);
  if (capabilities != 0x420) {
    for (;;) {
      __asm__ volatile("wfe");
    }
  }
  mmio_write32(SIMPLE_VIRTIO_USB_BASE + 0x400, 1);
  mmio_write32(SIMPLE_VIRTIO_USB_BASE + 0x404, 1);
  post_queue0(usb);
}

uint32_t simple_virtio_usb_wait_event(SimpleVirtioUSB *usb) {
  while (usb->queue0->flags != SIMPLE_VIRTIO_COMPLETE) {
  }
  avp_memory_barrier();
  return usb->queue0->size;
}

void simple_virtio_usb_respond(SimpleVirtioUSB *usb, uint32_t response_size) {
  usb->queue1->ipa = QUEUE1_BUFFER_IPA;
  usb->queue1->size = response_size;
  usb->queue1->tag = SIMPLE_VIRTIO_TAG;
  usb->queue1->flags = SIMPLE_VIRTIO_READABLE;

  post_queue0(usb);
  avp_memory_barrier();
  select_queue(1);
  commit_descriptor(QUEUE1_DESCRIPTOR_IPA);
  while (usb->queue1->flags != SIMPLE_VIRTIO_COMPLETE) {
  }
  avp_memory_barrier();
}

/*
 * Hand the device back as it was before simple_virtio_usb_initialize: the
 * event buffer it last posted points into RAM the handoff clears, and the
 * next stage initializes the device itself.
 */
void simple_virtio_usb_shutdown(void) {
  mmio_write32(SIMPLE_VIRTIO_USB_BASE + 0x404, 0);
  mmio_write32(SIMPLE_VIRTIO_USB_BASE + 0x400, 0);
}
