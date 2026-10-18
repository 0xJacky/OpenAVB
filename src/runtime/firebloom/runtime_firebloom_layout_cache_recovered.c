#include "runtime_firebloom_layout_recovered_private.h"

static void runtime_firebloom_layout_zero(void *pointer, size_t size) {
  volatile uint8_t *bytes = pointer;
  for (size_t index = 0U; index < size; ++index) {
    bytes[index] = 0U;
  }
}

RuntimeFirebloomLayoutCache *
runtime_firebloom_type_cache_create(RuntimeFirebloomLayoutType *type) {
  if (type == NULL || type->cache != NULL ||
      !runtime_firebloom_layout_mutation_active ||
      runtime_firebloom_layout_runtime.allocate == NULL) {
    runtime_firebloom_layout_contract_fail();
  }
  RuntimeFirebloomLayoutCache *cache =
      runtime_firebloom_layout_runtime.allocate(
          sizeof(*cache),
          runtime_firebloom_layout_runtime.context
      );
  if (cache == NULL) {
    runtime_firebloom_layout_contract_fail();
  }
  runtime_firebloom_layout_zero(cache, sizeof(*cache));
  cache->owner = type;
  type->cache = cache;
  return cache;
}

RuntimeFirebloomLayoutType *runtime_firebloom_type_suffix_create_cached(
    RuntimeFirebloomLayoutType *type,
    size_t offset
) {
  if (type == NULL || offset >= type->byte_size ||
      !runtime_firebloom_layout_mutation_active) {
    runtime_firebloom_layout_contract_fail();
  }
  runtime_firebloom_type_layout_validate_ranges(type);
  RuntimeFirebloomLayoutCache *cache = type->cache;
  if (cache == NULL) {
    cache = runtime_firebloom_type_cache_create(type);
  }
  for (RuntimeFirebloomLayoutSuffix *suffix = cache->suffixes; suffix != NULL;
       suffix = suffix->next) {
    if (suffix->offset == offset) {
      return &suffix->type;
    }
  }

  size_t first_entry = type->entry_count;
  for (size_t index = 0U; index < type->entry_count; ++index) {
    const RuntimeFirebloomLayoutEntry *entry = &type->entries[index];
    const size_t begin = runtime_firebloom_layout_entry_offset(entry);
    const size_t end = runtime_firebloom_layout_entry_end_offset(entry);
    if (offset == begin) {
      first_entry = index;
      break;
    }
    if (offset > begin && offset < end) {
      const size_t stride = runtime_firebloom_layout_kind_stride(
          runtime_firebloom_layout_entry_kind(entry));
      if ((offset - begin) % stride != 0U) {
        runtime_firebloom_layout_contract_fail();
      }
      first_entry = index;
      break;
    }
  }
  if (first_entry == type->entry_count) {
    runtime_firebloom_layout_contract_fail();
  }

  const size_t remaining = type->entry_count - first_entry;
  const size_t entries_size = remaining * sizeof(RuntimeFirebloomLayoutEntry);
  if (remaining != 0U &&
      entries_size / sizeof(RuntimeFirebloomLayoutEntry) != remaining) {
    runtime_firebloom_layout_contract_fail();
  }
  RuntimeFirebloomLayoutSuffix *suffix =
      runtime_firebloom_layout_runtime.allocate(
          sizeof(*suffix) + entries_size,
          runtime_firebloom_layout_runtime.context
      );
  if (suffix == NULL) {
    runtime_firebloom_layout_contract_fail();
  }
  runtime_firebloom_layout_zero(suffix, sizeof(*suffix) + entries_size);
  suffix->offset = offset;
  suffix->type.byte_size = type->byte_size - (uint32_t)offset;
  suffix->type.flags = type->flags;
  suffix->type.entry_count = remaining;
  suffix->type.entries = (RuntimeFirebloomLayoutEntry *)(suffix + 1);
  for (size_t index = 0U; index < remaining; ++index) {
    RuntimeFirebloomLayoutEntry entry = type->entries[first_entry + index];
    uint64_t encoded = entry.encoded & UINT64_C(0xffffffff00000000);
    encoded |= (uint32_t)(runtime_firebloom_layout_entry_offset(&entry) -
                          offset);
    suffix->type.entries[index].encoded = encoded;
    suffix->type.entries[index].type = entry.type;
  }
  suffix->next = cache->suffixes;
  cache->suffixes = suffix;
  return &suffix->type;
}

