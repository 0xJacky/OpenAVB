#include "avp/runtime/support/runtime_cpu_recovered.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static RuntimeCpuRecoveredHostState state;

static void expect_trap(void (*operation)(void)) {
  const pid_t child = fork();
  int status;

  assert(child >= 0);
  if (child == 0) {
    operation();
    _exit(0);
  }
  assert(waitpid(child, &status, 0) == child);
  assert(WIFSIGNALED(status));
  assert(WTERMSIG(status) == SIGILL || WTERMSIG(status) == SIGTRAP);
}

static void export_short_destination(void) {
  RuntimeCpuFeatureRegisters output;
  (void)runtime_cpu_feature_registers_export(&output, sizeof(output) - 1);
}

static void save_without_task(void) {
  state.tpidr_el1 = 0;
  runtime_selected_task_save_control_registers();
}

static void test_key_programming(void) {
  runtime_write_apiakey_el1(0x10, 0x11);
  runtime_write_apibkey_el1(0x20, 0x21);
  runtime_write_apdakey_el1(0x30, 0x31);
  runtime_write_apdbkey_el1(0x40, 0x41);
  runtime_write_apgakey_el1(0x50, 0x51);

  for (size_t index = 0; index < RUNTIME_CPU_PAUTH_KEY_COUNT; ++index) {
    assert(state.keys[index].high == (index + 1) * 0x10);
    assert(state.keys[index].low == (index + 1) * 0x10 + 1);
  }
  assert(state.barrier_count == RUNTIME_CPU_PAUTH_KEY_COUNT);
  runtime_data_memory_barrier_system();
  assert(state.barrier_count == RUNTIME_CPU_PAUTH_KEY_COUNT + 1);
}

static void test_feature_export(void) {
  RuntimeCpuFeatureRegisters output = {0};

  runtime_cpu_recovered_host_features_set(UINT64_C(0x1020304050607080),
                                          UINT64_C(0x8877665544332211));
  assert(runtime_cpu_feature_registers_export(&output, sizeof(output)) == 2);
  assert(output.id_aa64isar0_el1 == UINT64_C(0x1020304050607080));
  assert(output.id_aa64pfr0_el1 == UINT64_C(0x8877665544332211));
  expect_trap(export_short_destination);
}

static void test_cpu_identity_reads(void) {
  state.isr_el1 = UINT64_C(0xa5a55a5a);
  state.mpidr_el1 = UINT64_C(0x800000001234ab56);
  assert(runtime_read_isr_el1() == UINT64_C(0xa5a55a5a));
  assert(runtime_read_mpidr_el1() == UINT64_C(0x800000001234ab56));
  assert(runtime_read_mpidr_affinity_level1() == 0xab);
}

static void test_pointer_authentication_enable_mapping(void) {
  state.sctlr_el1 = UINT64_C(0x1005);
  runtime_enable_sctlr_el1_pointer_authentication(0);
  assert(state.sctlr_el1 == UINT64_C(0x1005));
  runtime_enable_sctlr_el1_pointer_authentication(0x0f);
  assert((state.sctlr_el1 & (UINT64_C(1) << 31)) != 0);
  assert((state.sctlr_el1 & (UINT64_C(1) << 30)) != 0);
  assert((state.sctlr_el1 & (UINT64_C(1) << 27)) != 0);
  assert((state.sctlr_el1 & (UINT64_C(1) << 13)) != 0);
}

static void test_task_control_register_save(void) {
  RuntimeCpuTaskControlRegisters task = {0};

  state.sctlr_el1 = UINT64_C(0x12345678);
  state.tcr_el1 = UINT64_C(0xabcdef01);
  runtime_write_tpidr_el1(&task);
  runtime_selected_task_save_control_registers();
  assert(task.sctlr_el1 == UINT64_C(0x12345678));
  assert(task.tcr_el1 == UINT64_C(0xabcdef01));
  expect_trap(save_without_task);
}

int main(void) {
  memset(&state, 0, sizeof(state));
  runtime_cpu_recovered_host_state_bind(&state);
  test_key_programming();
  test_feature_export();
  test_cpu_identity_reads();
  test_pointer_authentication_enable_mapping();
  test_task_control_register_save();
  return 0;
}
