#include "avp/runtime/firebloom/runtime_firebloom_tuples_recovered.h"

#include <stddef.h>
#include <stdint.h>


RuntimeFirebloomRangeType runtime_firebloom_tuple_character_type = {
    .relation = {.parent_and_kind = RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE,
                 .identity = UINT64_C(0x00144ed0)},
    .element_size = 1,
    .alignment = 1,
};

RuntimeFirebloomRangeType runtime_firebloom_tuple_byte_type = {
    .relation = {.parent_and_kind = RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE,
                 .identity = UINT64_C(0x00144f18)},
    .element_size = 1,
    .alignment = 1,
};

RuntimeFirebloomRangeType runtime_firebloom_tuple_qword_validation_type = {
    .relation = {.parent_and_kind = RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE,
                 .identity = UINT64_C(0x00144f00)},
    .element_size = sizeof(uint64_t),
    .alignment = _Alignof(uint64_t),
};

RuntimeFirebloomRangeType runtime_firebloom_tuple_qword_type = {
    .relation = {.parent_and_kind = RUNTIME_FIREBLOOM_RANGE_KIND_BYTEWISE,
                 .identity = UINT64_C(0x00144f30)},
    .element_size = sizeof(uint64_t),
    .alignment = _Alignof(uint64_t),
};

RuntimeFirebloomRangeType runtime_firebloom_tuple_dispatch_type = {
    .relation = {.parent_and_kind = RUNTIME_FIREBLOOM_RANGE_KIND_AGGREGATE,
                 .identity = UINT64_C(0x00145e20)},
    .element_size = 0xf8,
    .alignment = _Alignof(uint64_t),
};

static _Noreturn void runtime_firebloom_tuple_trap(void) {
  __builtin_trap();
}

static RuntimeFirebloomTuple runtime_firebloom_make_span(
    void *current,
    void *lower_bound,
    void *upper_bound,
    RuntimeFirebloomRangeType *type
) {
  if (current == NULL || lower_bound == NULL || upper_bound == NULL ||
      type == NULL || (uintptr_t)lower_bound > (uintptr_t)current ||
      (uintptr_t)current > (uintptr_t)upper_bound) {
    runtime_firebloom_tuple_trap();
  }
  return (RuntimeFirebloomTuple){current, lower_bound, upper_bound, type};
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_empty_span(void) {
  return (RuntimeFirebloomTuple){0};
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_return_empty_tuple(void) {
  return runtime_firebloom_empty_span();
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_make_cstring_span_bounded(
    const char *string,
    size_t maximum_length
) {
  if (string == NULL) {
    return runtime_firebloom_empty_span();
  }
  size_t length = 0;
  while (length < maximum_length && string[length] != '\0') {
    ++length;
  }
  if (length < maximum_length) {
    ++length;
  }
  if (length > UINTPTR_MAX - (uintptr_t)string) {
    runtime_firebloom_tuple_trap();
  }
  return runtime_firebloom_make_span(
      (void *)(uintptr_t)string,
      (void *)(uintptr_t)string,
      (void *)((uintptr_t)string + length),
      &runtime_firebloom_tuple_character_type
  );
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_make_cstring_span(const char *string) {
  return runtime_firebloom_make_cstring_span_bounded(string, SIZE_MAX);
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_make_five_byte_cstring_span(const char *current) {
  if (current == NULL || UINTPTR_MAX - (uintptr_t)current < 5) {
    runtime_firebloom_tuple_trap();
  }
  return runtime_firebloom_make_span(
      (void *)(uintptr_t)current,
      (void *)(uintptr_t)current,
      (void *)((uintptr_t)current + 5),
      &runtime_firebloom_tuple_byte_type
  );
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_make_qword_slot_span(uint64_t *slot) {
  if (slot == NULL || UINTPTR_MAX - (uintptr_t)slot < sizeof(*slot)) {
    runtime_firebloom_tuple_trap();
  }
  return runtime_firebloom_make_span(
      slot,
      slot,
      (void *)((uintptr_t)slot + sizeof(*slot)),
      &runtime_firebloom_tuple_qword_type
  );
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_make_byte_slot_span(uint8_t *slot) {
  if (slot == NULL || (uintptr_t)slot == UINTPTR_MAX) {
    runtime_firebloom_tuple_trap();
  }
  return runtime_firebloom_make_span(
      slot,
      slot,
      (void *)((uintptr_t)slot + 1),
      &runtime_firebloom_tuple_byte_type
  );
}

RuntimeFirebloomTuple RUNTIME_FIREBLOOM_TUPLES_PUBLIC
runtime_firebloom_make_qword_slot_span_clone(uint64_t *slot) {
  return runtime_firebloom_make_qword_slot_span(slot);
}
