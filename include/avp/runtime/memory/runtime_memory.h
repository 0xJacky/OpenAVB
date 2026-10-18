#pragma once

#include <stddef.h>


_Noreturn void runtime_firebloom_panic(void);
size_t runtime_bounded_strnlen(
    const char *current,
    const char *lower,
    const char *upper,
    size_t limit
);
size_t runtime_bounded_strlen(
    const char *current,
    const char *lower,
    const char *upper
);
void *runtime_memmove(void *destination, const void *source, size_t size);
void runtime_bzero(void *destination, size_t size);
void *runtime_memset(void *destination, int value, size_t size);
