#pragma once

#include <stddef.h>
#include <stdint.h>

#if __has_attribute(retain)
#define RUNTIME_CPU_RECOVERED_ENTRY __attribute__((used, retain))
#else
#define RUNTIME_CPU_RECOVERED_ENTRY __attribute__((used))
#endif

typedef enum {
  RUNTIME_CPU_PAUTH_KEY_APIA,
  RUNTIME_CPU_PAUTH_KEY_APIB,
  RUNTIME_CPU_PAUTH_KEY_APDA,
  RUNTIME_CPU_PAUTH_KEY_APDB,
  RUNTIME_CPU_PAUTH_KEY_APGA,
  RUNTIME_CPU_PAUTH_KEY_COUNT,
} RuntimeCpuPointerAuthenticationKey;

typedef struct {
  uint64_t high;
  uint64_t low;
} RuntimeCpuPointerAuthenticationKeyPair;

typedef struct {
  uint64_t sctlr_el1;
  uint64_t tcr_el1;
} RuntimeCpuTaskControlRegisters;

typedef struct {
  uint64_t id_aa64isar0_el1;
  uint64_t id_aa64pfr0_el1;
} RuntimeCpuFeatureRegisters;

typedef struct {
  uint64_t isr_el1;
  uint64_t mpidr_el1;
  uint64_t sctlr_el1;
  uint64_t tcr_el1;
  uintptr_t tpidr_el1;
  RuntimeCpuPointerAuthenticationKeyPair keys[RUNTIME_CPU_PAUTH_KEY_COUNT];
  uint32_t barrier_count;
} RuntimeCpuRecoveredHostState;

void runtime_cpu_recovered_host_state_bind(
    RuntimeCpuRecoveredHostState *host_state);
void runtime_cpu_recovered_host_features_set(
  uint64_t id_aa64isar0_el1,
  uint64_t id_aa64pfr0_el1
);

RUNTIME_CPU_RECOVERED_ENTRY void runtime_data_memory_barrier_system(void);
RUNTIME_CPU_RECOVERED_ENTRY void runtime_write_apiakey_el1(
  uint64_t high,
  uint64_t low
);
RUNTIME_CPU_RECOVERED_ENTRY void runtime_write_apibkey_el1(
  uint64_t high,
  uint64_t low
);
RUNTIME_CPU_RECOVERED_ENTRY void runtime_write_apdakey_el1(
  uint64_t high,
  uint64_t low
);
RUNTIME_CPU_RECOVERED_ENTRY void runtime_write_apdbkey_el1(
  uint64_t high,
  uint64_t low
);
RUNTIME_CPU_RECOVERED_ENTRY void runtime_write_apgakey_el1(
  uint64_t high,
  uint64_t low
);
RUNTIME_CPU_RECOVERED_ENTRY uint32_t runtime_cpu_feature_registers_export(
  void *unprivileged_destination,
  size_t destination_size
);
RUNTIME_CPU_RECOVERED_ENTRY void runtime_write_tpidr_el1(void *task);
RUNTIME_CPU_RECOVERED_ENTRY uint64_t runtime_read_isr_el1(void);
RUNTIME_CPU_RECOVERED_ENTRY uint32_t runtime_read_mpidr_affinity_level1(void);
RUNTIME_CPU_RECOVERED_ENTRY uint64_t runtime_read_mpidr_el1(void);
RUNTIME_CPU_RECOVERED_ENTRY void
runtime_enable_sctlr_el1_pointer_authentication(uint32_t enable_mask);
RUNTIME_CPU_RECOVERED_ENTRY void
runtime_selected_task_save_control_registers(void);
