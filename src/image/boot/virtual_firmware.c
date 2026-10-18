#include "avp/image/boot/virtual_firmware.h"
#include "avp/devices/simplevirtio/simple_virtio.h"
#include "avp/image/boot/patch_store.h"
#include "avp/image/boot/source_image.h"
#include "avp/platform/platform.h"

enum {
  /*
   * Past the patch store, not on top of it.
   *
   * This used to be SOURCE_IMAGE_UPLOAD_BASE + SOURCE_IMAGE_CAPACITY, which is
   * 0x71100000 -- the address PATCH_STORE_BASE also names.  Nothing noticed
   * while the only chain that ran was the recovery one: there the image arrives
   * over the recovery interface and no block is ever bounced, so the store sat
   * undisturbed.  The installed chain reads its firmware out of NOR through
   * this buffer, and the first block read destroyed the header of stage 0
   * before the ROM could apply it -- stage 1, a quarter of a megabyte higher,
   * came through untouched, which is what pointed at the collision.
   */
  VIRTUAL_FIRMWARE_BOUNCE_BASE = PATCH_STORE_BASE + PATCH_STORE_CAPACITY,
  VIRTUAL_FIRMWARE_CONSOLE_IPA = 0x20010000,
};

/*
 * Which of the four steps refused.
 *
 * The diagnostic hook below writes the reason into the progress registers,
 * which is no use once the DFU path that follows a refusal overwrites them,
 * and a caller that only sees false cannot say whether the reader would not
 * start, the source was not found, the prefix was short or the image did not
 * measure.  Each step says so on the console instead.
 */
static void virtual_firmware_say(const char *message) {
  volatile uint32_t *const data =
      (volatile uint32_t *)VIRTUAL_FIRMWARE_CONSOLE_IPA;

  while (*message != '\0') {
    *data = (uint8_t)*message++;
  }
}

static void virtual_firmware_diagnostic(
  void *context,
  uint64_t code,
  uint32_t detail,
  size_t candidate_count
) {
  (void)context;
  platform_progress(0, (uint32_t)code);
  platform_progress(1, detail);
  platform_progress(2, (uint32_t)candidate_count);
}

bool virtual_firmware_storage_initialize(VirtualFirmwareStorage *storage) {
  if (storage == 0) {
    return false;
  }
  return simple_virtio_block_domain_initialize(&storage->utility,
                                               SIMPLE_VIRTIO_BLOCK_SLOT_UTIL) &&
         simple_virtio_block_domain_initialize(&storage->root,
                                               SIMPLE_VIRTIO_BLOCK_SLOT_ROOT);
}

bool virtual_firmware_load_from_storage(
  const VirtualFirmwareStorage *storage,
  uint32_t *image_size
) {
  if (storage == 0 || image_size == 0) {
    return false;
  }

  VirtualFirmwareReader reader;
  AVPHUFAScanResult scan;
  uint32_t source_offset = 0;

  if (!virtual_firmware_reader_initialize(
          &reader, &storage->utility, (void *)VIRTUAL_FIRMWARE_BOUNCE_BASE,
          VIRTUAL_FIRMWARE_MAX_BLOCK_SIZE, 0, virtual_firmware_diagnostic)) {
    virtual_firmware_say("OpenBootROM: virtual firmware reader refused\n");
    return false;
  }
  if (!virtual_firmware_reader_select_source(&reader, &scan, &source_offset)) {
    virtual_firmware_say("OpenBootROM: virtual firmware source not found\n");
    return false;
  }
  if (virtual_firmware_reader_read(
          &reader, source_offset, (void *)SOURCE_IMAGE_UPLOAD_BASE,
          VIRTUAL_FIRMWARE_PREFIX_SIZE) != VIRTUAL_FIRMWARE_PREFIX_SIZE) {
    virtual_firmware_say("OpenBootROM: virtual firmware prefix short\n");
    return false;
  }
  if (!source_image_measure((const uint8_t *)SOURCE_IMAGE_UPLOAD_BASE,
                            VIRTUAL_FIRMWARE_PREFIX_SIZE, image_size)) {
    virtual_firmware_say("OpenBootROM: virtual firmware does not measure\n");
    return false;
  }
  if (virtual_firmware_reader_read(&reader, source_offset,
                                   (void *)SOURCE_IMAGE_UPLOAD_BASE,
                                   *image_size) != *image_size) {
    virtual_firmware_say("OpenBootROM: virtual firmware body short\n");
    return false;
  }
  return true;
}

bool virtual_firmware_load(uint32_t *image_size) {
  VirtualFirmwareStorage storage;
  return virtual_firmware_storage_initialize(&storage) &&
         virtual_firmware_load_from_storage(&storage, image_size);
}
