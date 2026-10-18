#include "avp/runtime/support/runtime_cpu_recovered.h"

static RuntimeCpuRecoveredHostState *host_state;
static RuntimeCpuFeatureRegisters host_features;

static _Noreturn void runtime_cpu_recovered_trap(void) {
  __builtin_trap();
  __builtin_unreachable();
}

#if !defined(__aarch64__) || !defined(__ELF__)
static RuntimeCpuRecoveredHostState *runtime_cpu_require_host_state(void) {
  if (host_state == NULL) {
    runtime_cpu_recovered_trap();
  }
  return host_state;
}
#endif

void runtime_cpu_recovered_host_state_bind(
    RuntimeCpuRecoveredHostState *new_host_state) {
  host_state = new_host_state;
}

void runtime_cpu_recovered_host_features_set(
    uint64_t id_aa64isar0_el1,
    uint64_t id_aa64pfr0_el1
) {
  host_features = (RuntimeCpuFeatureRegisters){
      .id_aa64isar0_el1 = id_aa64isar0_el1,
      .id_aa64pfr0_el1 = id_aa64pfr0_el1,
  };
}

void RUNTIME_CPU_RECOVERED_ENTRY runtime_data_memory_barrier_system(void) {
#if defined(__aarch64__) && defined(__ELF__)
  __asm__ volatile("dmb sy" ::: "memory");
#else
  ++runtime_cpu_require_host_state()->barrier_count;
#endif
}

#if !defined(__aarch64__) || !defined(__ELF__)
static void runtime_cpu_write_key(
    RuntimeCpuPointerAuthenticationKey key,
    uint64_t high,
    uint64_t low
) {
  RuntimeCpuRecoveredHostState *state = runtime_cpu_require_host_state();

  state->keys[key] = (RuntimeCpuPointerAuthenticationKeyPair){
      .high = high,
      .low = low,
  };
  ++state->barrier_count;
}

void RUNTIME_CPU_RECOVERED_ENTRY runtime_write_apiakey_el1(
    uint64_t high,
    uint64_t low
) {
  runtime_cpu_write_key(RUNTIME_CPU_PAUTH_KEY_APIA, high, low);
}

void RUNTIME_CPU_RECOVERED_ENTRY runtime_write_apibkey_el1(
    uint64_t high,
    uint64_t low
) {
  runtime_cpu_write_key(RUNTIME_CPU_PAUTH_KEY_APIB, high, low);
}

void RUNTIME_CPU_RECOVERED_ENTRY runtime_write_apdakey_el1(
    uint64_t high,
    uint64_t low
) {
  runtime_cpu_write_key(RUNTIME_CPU_PAUTH_KEY_APDA, high, low);
}

void RUNTIME_CPU_RECOVERED_ENTRY runtime_write_apdbkey_el1(
    uint64_t high,
    uint64_t low
) {
  runtime_cpu_write_key(RUNTIME_CPU_PAUTH_KEY_APDB, high, low);
}

void RUNTIME_CPU_RECOVERED_ENTRY runtime_write_apgakey_el1(
    uint64_t high,
    uint64_t low
) {
  runtime_cpu_write_key(RUNTIME_CPU_PAUTH_KEY_APGA, high, low);
}
#endif

uint32_t RUNTIME_CPU_RECOVERED_ENTRY runtime_cpu_feature_registers_export(
    void *unprivileged_destination,
    size_t destination_size
) {
  RuntimeCpuFeatureRegisters features;

  if (unprivileged_destination == NULL ||
      destination_size < sizeof(RuntimeCpuFeatureRegisters)) {
    runtime_cpu_recovered_trap();
  }
#if defined(__aarch64__) && defined(__ELF__)
  __asm__ volatile("mrs %0, id_aa64isar0_el1"
                   : "=r"(features.id_aa64isar0_el1));
  __asm__ volatile("mrs %0, id_aa64pfr0_el1" : "=r"(features.id_aa64pfr0_el1));
#else
  features = host_features;
#endif
  if (features.id_aa64isar0_el1 == 0 || features.id_aa64pfr0_el1 == 0) {
    runtime_cpu_recovered_trap();
  }

  *(RuntimeCpuFeatureRegisters *)unprivileged_destination = features;
  return 2;
}

void RUNTIME_CPU_RECOVERED_ENTRY runtime_write_tpidr_el1(void *task) {
#if defined(__aarch64__) && defined(__ELF__)
  __asm__ volatile("msr tpidr_el1, %0" : : "r"(task) : "memory");
#else
  runtime_cpu_require_host_state()->tpidr_el1 = (uintptr_t)task;
#endif
}

uint64_t RUNTIME_CPU_RECOVERED_ENTRY runtime_read_isr_el1(void) {
#if defined(__aarch64__) && defined(__ELF__)
  uint64_t value;
  __asm__ volatile("mrs %0, isr_el1" : "=r"(value));
  return value;
#else
  return runtime_cpu_require_host_state()->isr_el1;
#endif
}

uint64_t RUNTIME_CPU_RECOVERED_ENTRY runtime_read_mpidr_el1(void) {
#if defined(__aarch64__) && defined(__ELF__)
  uint64_t value;
  __asm__ volatile("mrs %0, mpidr_el1" : "=r"(value));
  return value;
#else
  return runtime_cpu_require_host_state()->mpidr_el1;
#endif
}

uint32_t RUNTIME_CPU_RECOVERED_ENTRY runtime_read_mpidr_affinity_level1(void) {
  return (uint32_t)((runtime_read_mpidr_el1() >> 8) & UINT64_C(0xff));
}

void RUNTIME_CPU_RECOVERED_ENTRY
runtime_enable_sctlr_el1_pointer_authentication(uint32_t enable_mask) {
  const uint64_t mapped_bits = ((uint64_t)(enable_mask & 1u) << 31) |
                               ((uint64_t)((enable_mask >> 1) & 1u) << 30) |
                               ((uint64_t)((enable_mask >> 2) & 1u) << 27) |
                               ((uint64_t)((enable_mask >> 3) & 1u) << 13);

  if (mapped_bits == 0) {
    return;
  }
#if defined(__aarch64__) && defined(__ELF__)
  uint64_t sctlr;
  __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
  sctlr |= mapped_bits;
  __asm__ volatile("msr sctlr_el1, %0\nisb" : : "r"(sctlr) : "memory");
#else
  runtime_cpu_require_host_state()->sctlr_el1 |= mapped_bits;
#endif
}

void RUNTIME_CPU_RECOVERED_ENTRY
runtime_selected_task_save_control_registers(void) {
#if defined(__aarch64__) && defined(__ELF__)
  RuntimeCpuTaskControlRegisters *task;
  uint64_t sctlr;
  uint64_t tcr;

  __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
  __asm__ volatile("mrs %0, tcr_el1" : "=r"(tcr));
  __asm__ volatile("mrs %0, tpidr_el1" : "=r"(task));
  task->sctlr_el1 = sctlr;
  task->tcr_el1 = tcr;
#else
  RuntimeCpuRecoveredHostState *state = runtime_cpu_require_host_state();
  RuntimeCpuTaskControlRegisters *task =
      (RuntimeCpuTaskControlRegisters *)state->tpidr_el1;

  if (task == NULL) {
    runtime_cpu_recovered_trap();
  }
  task->sctlr_el1 = state->sctlr_el1;
  task->tcr_el1 = state->tcr_el1;
#endif
}
