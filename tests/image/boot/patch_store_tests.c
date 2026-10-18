/*
 * The host-uploaded patch store: open / append / seal / report, and the shape
 * the uploader (tools/patchup.py) builds.
 *
 * With no arguments the built-in cases run.  With `--blob FILE --stage N` the
 * file is replayed the way `patchup.py send` streams it (0x400-byte chunks,
 * ordinals from zero, then a seal carrying the patch count derived from the
 * blob size), and the sealed stage is checked against the bytes in the file.
 * tools/check_patch_uploader_contract.py drives that mode.
 */
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avp/image/boot/patch_store.h"

enum { UPLOADER_CHUNK = 0x400 };

_Static_assert(offsetof(AVPPatchStageHeader, magic) == 0, "magic offset");
_Static_assert(offsetof(AVPPatchStageHeader, count) == 8, "count offset");
_Static_assert(offsetof(AVPPatchStageHeader, received) == 16,
               "received offset");
_Static_assert(offsetof(AVPPatchStageHeader, load_base) == 32,
               "load_base offset");
_Static_assert(offsetof(AVPPatchStageHeader, compressed_digest) == 48,
               "compressed_digest offset");
_Static_assert(offsetof(AVPPatchStageHeader, raw_digest) == 80,
               "raw_digest offset");
_Static_assert(sizeof(AVPImagePatch) == 8, "AVPImagePatch size");

static uint32_t read_u32(const uint8_t *at)
{
    return (uint32_t)at[0] | ((uint32_t)at[1] << 8) |
           ((uint32_t)at[2] << 16) | ((uint32_t)at[3] << 24);
}

static size_t make_blob(uint8_t *out, uint32_t count, uint32_t flags)
{
    AVPPatchStageHeader header;
    AVPImagePatch patch;
    uint32_t index;

    memset(&header, 0, sizeof(header));
    header.magic = PATCH_STORE_MAGIC;
    header.version = PATCH_STORE_VERSION;
    header.count = count;
    header.flags = flags;
    header.compressed_size = 0x1234;
    header.raw_size = 0x5678;
    header.load_base = UINT64_C(0x1fc050000);
    header.entry_exception_level = 2;
    memset(header.compressed_digest, 0xc5, sizeof(header.compressed_digest));
    memset(header.raw_digest, 0x5a, sizeof(header.raw_digest));
    memcpy(out, &header, sizeof(header));
    for (index = 0; index < count; ++index) {
        patch.offset = 0x1000 + index * 4;
        patch.instruction = 0xd503201f; /* nop */
        memcpy(
            out + sizeof(header) + index * sizeof(patch),
            &patch,
            sizeof(patch)
        );
    }
    return sizeof(header) + count * sizeof(patch);
}

static bool upload(uint32_t stage, const uint8_t *blob, size_t size)
{
    uint32_t chunk = 0;
    size_t at;

    if (!patch_store_open(stage)) {
        return false;
    }
    for (at = 0; at < size; at += UPLOADER_CHUNK) {
        const size_t piece =
            size - at < UPLOADER_CHUNK ? size - at : UPLOADER_CHUNK;
        if (!patch_store_append(stage, chunk++, blob + at,
                                (uint32_t)piece)) {
            return false;
        }
    }
    return true;
}

static void test_reset_leaves_every_stage_empty(void)
{
    uint8_t report[PATCH_STAGE_COUNT * 12];
    uint32_t stage;

    patch_store_reset();
    for (stage = 0; stage < PATCH_STAGE_COUNT; ++stage) {
        assert(patch_store_patches(stage, 0) == 0);
        assert(patch_store_identity(stage) == 0);
    }
    assert(patch_store_report(report, sizeof(report)) == sizeof(report));
    for (stage = 0; stage < PATCH_STAGE_COUNT; ++stage) {
        assert(read_u32(report + stage * 12) == 0);
        assert(read_u32(report + stage * 12 + 4) == 0);
        assert(read_u32(report + stage * 12 + 8) == 0);
    }
    assert(patch_store_report(report, sizeof(report) - 1) == 0);
    assert(patch_store_report(0, sizeof(report)) == 0);
}

static void test_stage_bounds(void)
{
    static const uint8_t byte = 0;

    assert(patch_store_stage(PATCH_STAGE_COUNT) == 0);
    assert(!patch_store_open(PATCH_STAGE_COUNT));
    assert(!patch_store_append(PATCH_STAGE_COUNT, 0, &byte, 1));
    assert(!patch_store_seal(PATCH_STAGE_COUNT, 0));
    assert((uint8_t *)patch_store_stage(PATCH_STAGE_IBEC) -
               (uint8_t *)patch_store_stage(PATCH_STAGE_IBSS) ==
           PATCH_STAGE_STRIDE);
    assert(PATCH_STAGE_COUNT * PATCH_STAGE_STRIDE <= PATCH_STORE_CAPACITY);
}

static void test_magic_is_not_visible_until_sealed(void)
{
    static uint8_t blob[sizeof(AVPPatchStageHeader) + 300 * 8];
    const size_t size = make_blob(blob, 300, PATCH_STAGE_FLAG_IDENTITY);
    const AVPPatchStageHeader *header;
    const AVPImagePatch *patches;
    uint32_t count = 0;

    patch_store_reset();
    assert(upload(PATCH_STAGE_IBSS, blob, size));
    header = patch_store_stage(PATCH_STAGE_IBSS);
    assert(header->magic == 0);
    assert(header->received == size);
    assert(header->next_chunk == (size + UPLOADER_CHUNK - 1) / UPLOADER_CHUNK);
    assert(patch_store_patches(PATCH_STAGE_IBSS, &count) == 0);
    assert(patch_store_identity(PATCH_STAGE_IBSS) == 0);

    assert(patch_store_seal(PATCH_STAGE_IBSS, 300));
    patches = patch_store_patches(PATCH_STAGE_IBSS, &count);
    assert(patches != 0);
    assert(count == 300);
    assert(patches[0].offset == 0x1000);
    assert(patches[299].offset == 0x1000 + 299 * 4);
    assert(patches[299].instruction == 0xd503201f);

    header = patch_store_identity(PATCH_STAGE_IBSS);
    assert(header != 0);
    assert(header->load_base == UINT64_C(0x1fc050000));
    assert(header->compressed_size == 0x1234);
    assert(header->raw_size == 0x5678);
    assert(header->entry_exception_level == 2);
    assert(header->raw_digest[31] == 0x5a);

    /* Sealed is final until the stage is opened again. */
    assert(!patch_store_append(PATCH_STAGE_IBSS,
                               header->next_chunk, blob, 8));
    assert(!patch_store_seal(PATCH_STAGE_IBSS, 300));
    assert(patch_store_open(PATCH_STAGE_IBSS));
    assert(patch_store_patches(PATCH_STAGE_IBSS, 0) == 0);
}

static void test_out_of_order_chunk_is_refused(void)
{
    static uint8_t blob[sizeof(AVPPatchStageHeader) + 200 * 8];
    const size_t size = make_blob(blob, 200, 0);

    assert(size > UPLOADER_CHUNK);
    patch_store_reset();
    assert(patch_store_open(PATCH_STAGE_IBEC));
    assert(!patch_store_append(PATCH_STAGE_IBEC, 1, blob, UPLOADER_CHUNK));
    assert(patch_store_append(PATCH_STAGE_IBEC, 0, blob, UPLOADER_CHUNK));
    /* A retried chunk 0 would leave a hole; it is refused. */
    assert(!patch_store_append(PATCH_STAGE_IBEC, 0, blob, UPLOADER_CHUNK));
    assert(!patch_store_append(PATCH_STAGE_IBEC, 1, blob, 0));
    assert(!patch_store_append(PATCH_STAGE_IBEC, 1, 0, 4));
    assert(patch_store_stage(PATCH_STAGE_IBEC)->received == UPLOADER_CHUNK);
}

static void test_seal_rejects_a_count_that_disagrees(void)
{
    static uint8_t blob[sizeof(AVPPatchStageHeader) + 10 * 8];
    size_t size = make_blob(blob, 10, 0);

    patch_store_reset();
    assert(upload(PATCH_STAGE_SPTM, blob, size - 8)); /* one row short */
    assert(!patch_store_seal(PATCH_STAGE_SPTM, 10));
    assert(patch_store_seal(PATCH_STAGE_SPTM, 9));
    assert(patch_store_identity(PATCH_STAGE_SPTM) == 0); /* no identity flag */

    /* A header that never fully arrived cannot be sealed. */
    assert(patch_store_open(PATCH_STAGE_KERNEL));
    assert(patch_store_append(PATCH_STAGE_KERNEL, 0, blob, 16));
    assert(!patch_store_seal(PATCH_STAGE_KERNEL, 0));

    /* Nor can one with the wrong version. */
    size = make_blob(blob, 0, 0);
    ((AVPPatchStageHeader *)(void *)blob)->version = PATCH_STORE_VERSION + 1;
    assert(upload(PATCH_STAGE_KERNEL, blob, size));
    assert(!patch_store_seal(PATCH_STAGE_KERNEL, 0));
}

static void test_stage_cannot_overrun_its_stride(void)
{
    static uint8_t chunk[UPLOADER_CHUNK];
    uint32_t ordinal;

    patch_store_reset();
    assert(patch_store_open(PATCH_STAGE_IBSS));
    for (ordinal = 0; ordinal < PATCH_STAGE_STRIDE / UPLOADER_CHUNK;
         ++ordinal) {
        assert(patch_store_append(PATCH_STAGE_IBSS, ordinal, chunk,
                                  UPLOADER_CHUNK));
    }
    assert(!patch_store_append(PATCH_STAGE_IBSS, ordinal, chunk, 1));
    /* The neighbouring stage's header was not touched. */
    assert(patch_store_stage(PATCH_STAGE_IBEC)->received == 0);
}

static void test_report_names_each_stage(void)
{
    static uint8_t blob[sizeof(AVPPatchStageHeader) + 7 * 8];
    uint8_t report[PATCH_STAGE_COUNT * 12];
    const size_t size = make_blob(blob, 7, 0);

    patch_store_reset();
    assert(upload(PATCH_STAGE_IBEC, blob, size));
    assert(patch_store_seal(PATCH_STAGE_IBEC, 7));
    assert(upload(PATCH_STAGE_KERNEL, blob, size));      /* left open */
    assert(patch_store_report(report, sizeof(report)) == sizeof(report));
    assert(read_u32(report + 1 * 12) == 7);
    assert(read_u32(report + 1 * 12 + 4) == size);
    assert(read_u32(report + 1 * 12 + 8) == 1);
    assert(read_u32(report + 3 * 12) == 0);
    assert(read_u32(report + 3 * 12 + 4) == size);
    assert(read_u32(report + 3 * 12 + 8) == 0);
}

static int replay_blob(const char *path, uint32_t stage)
{
    static uint8_t blob[PATCH_STAGE_STRIDE];
    const AVPPatchStageHeader *expected =
        (const AVPPatchStageHeader *)(const void *)blob;
    const AVPImagePatch *patches;
    uint32_t count = 0;
    size_t size;
    FILE *file = fopen(path, "rb");

    if (file == 0) {
        perror(path);
        return 1;
    }
    size = fread(blob, 1, sizeof(blob), file);
    fclose(file);
    assert(size >= sizeof(AVPPatchStageHeader));
    assert((size - sizeof(AVPPatchStageHeader)) % sizeof(AVPImagePatch) == 0);

    patch_store_reset();
    assert(upload(stage, blob, size));
    assert(patch_store_seal(
        stage,
        (uint32_t)((size - sizeof(AVPPatchStageHeader)) /
                   sizeof(AVPImagePatch))));
    patches = patch_store_patches(stage, &count);
    assert(patches != 0);
    assert(count == expected->count);
    assert(memcmp(patches, blob + sizeof(AVPPatchStageHeader),
                  count * sizeof(AVPImagePatch)) == 0);
    if (expected->flags & PATCH_STAGE_FLAG_IDENTITY) {
        const AVPPatchStageHeader *identity = patch_store_identity(stage);
        assert(identity != 0);
        assert(identity->load_base == expected->load_base);
        assert(identity->compressed_size == expected->compressed_size);
        assert(identity->raw_size == expected->raw_size);
        assert(identity->entry_exception_level ==
               expected->entry_exception_level);
        assert(memcmp(identity->compressed_digest,
                      expected->compressed_digest, 32) == 0);
        assert(memcmp(identity->raw_digest, expected->raw_digest, 32) == 0);
    } else {
        assert(patch_store_identity(stage) == 0);
    }
    printf(
        "stage %u: %u patches, %zu bytes, sealed%s\n",
        stage,
        count,
        size,
        (expected->flags & PATCH_STAGE_FLAG_IDENTITY) ? ", identity" : ""
    );
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 5 && strcmp(argv[1], "--blob") == 0 &&
        strcmp(argv[3], "--stage") == 0) {
        return replay_blob(argv[2], (uint32_t)strtoul(argv[4], 0, 0));
    }
    if (argc != 1) {
        fprintf(stderr, "usage: %s [--blob FILE --stage N]\n", argv[0]);
        return 64;
    }
    test_reset_leaves_every_stage_empty();
    test_stage_bounds();
    test_magic_is_not_visible_until_sealed();
    test_out_of_order_chunk_is_refused();
    test_seal_rejects_a_count_that_disagrees();
    test_stage_cannot_overrun_its_stride();
    test_report_names_each_stage();
    puts("patch_store tests passed");
    return 0;
}
