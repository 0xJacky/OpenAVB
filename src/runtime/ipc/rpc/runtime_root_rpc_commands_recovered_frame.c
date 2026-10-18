#include "runtime_root_rpc_commands_recovered_private.h"

RootRPCRegisterFrame RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_prologue_save_x19_x28_allocate_c0(
    const uintptr_t registers[10]) {
  if (registers == NULL) {
    root_rpc_recovered_trap();
  }
  RootRPCRegisterFrame frame = {0};
  for (size_t index = 0; index < 10; ++index) {
    frame.saved_registers[index] = registers[index];
  }
  return frame;
}

void RUNTIME_ROOT_RPC_PUBLIC
root_rpc_outlined_epilogue_restore_x19_x28_release_c0(
    const RootRPCRegisterFrame *frame,
    uintptr_t registers[10]
) {
  if (frame == NULL || registers == NULL) {
    root_rpc_recovered_trap();
  }
  for (size_t index = 0; index < 10; ++index) {
    registers[index] = frame->saved_registers[index];
  }
}
