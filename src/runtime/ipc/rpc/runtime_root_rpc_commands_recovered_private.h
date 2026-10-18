#pragma once

#include "avp/runtime/ipc/runtime_root_rpc_commands_recovered.h"

static inline _Noreturn void root_rpc_recovered_trap(void) { __builtin_trap(); }

static inline RuntimeRPCPayloadSpan root_rpc_scalar_payload(const void *value,
                                                            size_t size) {
  const uint8_t *bytes = value;
  return (RuntimeRPCPayloadSpan){
      .current = value,
      .lower_bound = value,
      .upper_bound = bytes + size,
      .type_token = AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
  };
}

static inline RootRPCFieldArguments
root_rpc_scalar_arguments(const void *value, size_t size, size_t alignment) {
  return (RootRPCFieldArguments){
      .spec =
          {
              .alignment = alignment,
              .size = size,
              .pointer_offset = 0,
              .type_token = AVP_RUNTIME_RPC_NO_POINTER_TOKEN,
          },
      .payload = root_rpc_scalar_payload(value, size),
  };
}
