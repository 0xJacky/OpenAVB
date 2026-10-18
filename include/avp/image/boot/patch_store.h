/*
 * A patch table the host uploads, instead of one compiled into the ROM.
 *
 * The ROM used to carry the whole description of the image it boots: its size,
 * its two digests, where it loads and the words to rewrite in it, all generated
 * out of the platform JSON.  That is exactly right for one build and useless
  * for the next one, and every offset in it is wrong the moment the payload is
 * rebuilt.  The host can work all of it out -- an anchor scanner finds the
 * sites by anchor rather than by offset -- so the ROM stops holding the
 * answer and starts being told it.
 *
 * A stage is uploaded as one flat blob: this header, then `count` patches.  The
 * blob arrives in chunks over the recovery interface and is assembled in place;
 * `magic` is written last, by the seal request, so a torn upload never looks
 * like a finished one.
 *
 * The identity fields matter as much as the patches.  Without them the ROM
 * would still measure the image it was given against the digest of the build it
 * was compiled for and refuse a different iBoot before any patch could be
 * applied.  With `PATCH_STAGE_FLAG_IDENTITY` set, the uploaded description is
 * what the image is checked against, so the same ROM boots any build the host
 * can describe.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "avp/platform/platform_profile.h"

enum {
    /* Main RAM, past the DFU upload window, below anything the payload uses. */
    PATCH_STORE_BASE     = 0x71200000,
    PATCH_STAGE_STRIDE   = 0x00040000,
    PATCH_STORE_CAPACITY = 0x00100000,

    PATCH_STORE_MAGIC    = 0x48435450, /* 'PTCH' */
    PATCH_STORE_VERSION  = 1,

    PATCH_STAGE_IBSS     = 0,
    PATCH_STAGE_IBEC     = 1,
    PATCH_STAGE_SPTM     = 2,
    PATCH_STAGE_KERNEL   = 3,
    PATCH_STAGE_COUNT    = 4,

    PATCH_STAGE_FLAG_IDENTITY = 1u << 0,

    /* A kernel-stage row whose offset has this bit set patches TXM-rx. */
    PATCH_TXM_OFFSET_FLAG = 0x80000000u,
};

typedef struct __attribute__((packed)) {
    uint32_t magic;              /* PATCH_STORE_MAGIC, written by the seal   */
    uint32_t version;
    uint32_t count;              /* patches in this stage                    */
    uint32_t flags;
    uint32_t received;           /* bytes accepted so far, header included   */
    uint32_t next_chunk;         /* ordinal the next append must carry       */
    uint32_t compressed_size;    /* identity: what the ROM should expect     */
    uint32_t raw_size;
    uint64_t load_base;
    uint32_t entry_exception_level;
    uint32_t reserved;
    uint8_t  compressed_digest[32];
    uint8_t  raw_digest[32];
} AVPPatchStageHeader;

_Static_assert(sizeof(AVPPatchStageHeader) == 112, "AVPPatchStageHeader size");

/*
 * Where a stage's blob is assembled.  On the machine that is a fixed address in
 * main RAM, which is what lets the handoff read it with the MMU off and nothing
 * but inline code available.  A host build -- the transcript tests link the same
 * USB dispatch -- gets a plain array instead, so replaying a transcript that
 * happens to carry one of these requests writes into memory it owns rather than
 * into address 0x71200000.
 */
#ifdef PATCH_STORE_HOSTED
uint8_t *patch_store_hosted_origin(void);
#define PATCH_STORE_ORIGIN (patch_store_hosted_origin())
#else
#define PATCH_STORE_ORIGIN ((uint8_t *)(uintptr_t)PATCH_STORE_BASE)
#endif

static inline AVPPatchStageHeader *patch_store_stage(uint32_t stage)
{
    if (stage >= PATCH_STAGE_COUNT) {
        return 0;
    }
    return (AVPPatchStageHeader *)(void *)
        (PATCH_STORE_ORIGIN + (uintptr_t)stage * PATCH_STAGE_STRIDE);
}

/* The patches of a sealed stage, or 0 when nothing usable was uploaded. */
static inline const AVPImagePatch *patch_store_patches(
    uint32_t stage,
    uint32_t *count
) {
    const AVPPatchStageHeader *header = patch_store_stage(stage);

    if (header == 0 || header->magic != PATCH_STORE_MAGIC ||
        header->version != PATCH_STORE_VERSION) {
        return 0;
    }
    if (count != 0) {
        *count = header->count;
    }
    return (const AVPImagePatch *)(const void *)(header + 1);
}

/* The uploaded identity for a stage, or 0 when it carries none. */
static inline const AVPPatchStageHeader *patch_store_identity(uint32_t stage)
{
    const AVPPatchStageHeader *header = patch_store_stage(stage);

    if (header == 0 || header->magic != PATCH_STORE_MAGIC ||
        header->version != PATCH_STORE_VERSION ||
        (header->flags & PATCH_STAGE_FLAG_IDENTITY) == 0) {
        return 0;
    }
    return header;
}

void patch_store_reset(void);
bool patch_store_open(uint32_t stage);
bool patch_store_append(
    uint32_t stage,
    uint32_t chunk,
    const uint8_t *bytes,
    uint32_t size
);
bool patch_store_seal(uint32_t stage, uint32_t expected_count);
uint32_t patch_store_report(uint8_t *out, uint32_t capacity);
