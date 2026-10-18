#pragma once

#include "avp/runtime/ipc/runtime_provider_typed_recovered.h"

enum {
    RUNTIME_PROVIDER_TYPED_SLOT_COUNT = 5,
};

__attribute__((minsize)) _Noreturn void runtime_provider_typed_fail(void);
__attribute__((minsize)) RuntimeProviderTypedImplementation *
runtime_provider_typed_require_implementation(
    RuntimeProviderTypedDescriptor *descriptor);
__attribute__((minsize)) RuntimeProviderTypedStorage *
runtime_provider_typed_require_storage(RuntimeProviderTypedStorage *storage);
