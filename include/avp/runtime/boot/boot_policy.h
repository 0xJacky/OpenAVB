#pragma once

#include <stdbool.h>
#include <stdint.h>


typedef enum {
    AVP_BOOT_SELECTION_PLATFORM = 0,
    AVP_BOOT_SELECTION_FORCE_DFU = 1,
} AVPBootSelection;

typedef enum {
    AVP_BOOT_SOURCE_DFU = 6,
    AVP_BOOT_SOURCE_VIRTUAL_FIRMWARE = 14,
} AVPBootSource;

typedef struct {
    uint8_t status;
    uint8_t configuration;
    uint8_t board_id;
    uint8_t security_epoch;
    uint32_t reserved[3];
} AVPBootContext;

typedef struct {
    int32_t attempt;
    AVPBootSource source;
    uint32_t selector;
    uint32_t instance;
} AVPBootSourceCandidate;

typedef struct {
    uint32_t configuration;
    uint32_t options;
    AVPBootSelection selection;
} AVPBootPolicy;

#define BOOT_POLICY_PUBLIC __attribute__((used, retain))

AVPBootPolicy boot_policy_decode(uint32_t configuration, uint32_t options);
AVPBootContext boot_context_create(
    uint8_t configuration,
    uint8_t board_id,
    uint8_t security_epoch
);
BOOT_POLICY_PUBLIC void boot_context_initialize(
    AVPBootContext *context,
    uint8_t security_epoch
);
BOOT_POLICY_PUBLIC int boot_context_populate_platform(
    AVPBootContext *context,
    uint8_t configuration,
    uint8_t board_id
);
BOOT_POLICY_PUBLIC void boot_dispatch_preflight_hook_noop(void);
void boot_context_mark_boot_nonce_valid(AVPBootContext *context);
void boot_context_mark_remote_boot(AVPBootContext *context);
bool boot_source_next_candidate(
    const AVPBootContext *context,
    int32_t attempt,
    AVPBootSourceCandidate *candidate
);
