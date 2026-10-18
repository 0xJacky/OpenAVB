#ifndef AVP_RUNTIME_STRING_COMPAT_H
#define AVP_RUNTIME_STRING_COMPAT_H

#include "avp/runtime/firebloom/runtime_firebloom.h"

#include <stddef.h>

#define AVP_RUNTIME_STRING_PUBLIC __attribute__((used, retain))

AVP_RUNTIME_STRING_PUBLIC int runtime_memcmp(
    const void *left,
    const void *right,
    size_t count
);
AVP_RUNTIME_STRING_PUBLIC RuntimeFirebloomSpan runtime_bounded_strchr(
    RuntimeFirebloomSpan source,
    int character
);
AVP_RUNTIME_STRING_PUBLIC int runtime_bounded_strcmp(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right
);
AVP_RUNTIME_STRING_PUBLIC size_t runtime_bounded_strlcat(
    RuntimeFirebloomSpan destination,
    RuntimeFirebloomSpan source
);
AVP_RUNTIME_STRING_PUBLIC size_t runtime_bounded_strlcpy(
    RuntimeFirebloomSpan destination,
    RuntimeFirebloomSpan source
);
AVP_RUNTIME_STRING_PUBLIC int runtime_bounded_strncmp(
    RuntimeFirebloomSpan left,
    RuntimeFirebloomSpan right,
    size_t count
);
AVP_RUNTIME_STRING_PUBLIC size_t runtime_strlcpy(
    char *destination,
    const char *source,
    size_t capacity
);
AVP_RUNTIME_STRING_PUBLIC size_t runtime_strlen(const char *string);
AVP_RUNTIME_STRING_PUBLIC int runtime_strncmp(
    const char *left,
    const char *right,
    size_t count
);
AVP_RUNTIME_STRING_PUBLIC void *runtime_memmove_source_first(
    const void *source,
    void *destination,
    size_t count
);
AVP_RUNTIME_STRING_PUBLIC void *runtime_bounded_memmove(
    void *destination,
    size_t capacity,
    const void *source,
    size_t count
);
AVP_RUNTIME_STRING_PUBLIC void *runtime_bounded_memset(
    void *destination,
    size_t capacity,
    int value,
    size_t count
);

#endif
