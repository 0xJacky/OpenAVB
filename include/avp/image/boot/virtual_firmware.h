#pragma once

#include "avp/devices/simplevirtio/simple_virtio.h"
#include "avp/runtime/boot/boot_virtual_firmware_compat.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
  VIRTUAL_FIRMWARE_PROVIDER_CAPACITY = 0x00a00000,
  VIRTUAL_FIRMWARE_PREFIX_SIZE = 512,
  VIRTUAL_FIRMWARE_MAX_BLOCK_SIZE = 0x4000,
};

typedef struct {
  const SimpleVirtioBlock *block;
  uint64_t capacity;
  uint8_t *bounce;
  size_t bounce_size;
  void *diagnostic_context;
  BootVirtualFirmwareDiagnostic diagnostic;
} VirtualFirmwareReader;

typedef struct {
  SimpleVirtioBlock utility;
  SimpleVirtioBlock root;
} VirtualFirmwareStorage;

bool virtual_firmware_reader_initialize(
  VirtualFirmwareReader *reader,
  const SimpleVirtioBlock *block,
  void *bounce,
  size_t bounce_size,
  void *diagnostic_context,
  BootVirtualFirmwareDiagnostic diagnostic
);
size_t virtual_firmware_reader_read(
  VirtualFirmwareReader *reader,
  uint32_t offset,
  void *output,
  size_t size
);
bool virtual_firmware_reader_select_source(
  VirtualFirmwareReader *reader,
  AVPHUFAScanResult *scan,
  uint32_t *source_offset
);

bool virtual_firmware_storage_initialize(VirtualFirmwareStorage *storage);
bool virtual_firmware_load_from_storage(
  const VirtualFirmwareStorage *storage,
  uint32_t *image_size
);
bool virtual_firmware_load(uint32_t *image_size);
