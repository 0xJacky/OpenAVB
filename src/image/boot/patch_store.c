#include "avp/image/boot/patch_store.h"

#ifdef PATCH_STORE_HOSTED
/*
 * The store a host build writes into.  Sized the same as the real window so the
 * bounds checks below mean the same thing in both.
 */
static uint8_t patch_store_hosted[PATCH_STAGE_COUNT * PATCH_STAGE_STRIDE];

uint8_t *patch_store_hosted_origin(void)
{
    return patch_store_hosted;
}
#endif


static void store_zero(uint8_t *at, uint32_t size)
{
    uint32_t index;

    for (index = 0; index < size; ++index) {
        at[index] = 0;
    }
}

static void store_copy(uint8_t *to, const uint8_t *from, uint32_t size)
{
    uint32_t index;

    for (index = 0; index < size; ++index) {
        to[index] = from[index];
    }
}

static void write_u32(uint8_t *at, uint32_t value)
{
    at[0] = (uint8_t)value;
    at[1] = (uint8_t)(value >> 8);
    at[2] = (uint8_t)(value >> 16);
    at[3] = (uint8_t)(value >> 24);
}

void patch_store_reset(void)
{
    uint32_t stage;

    for (stage = 0; stage < PATCH_STAGE_COUNT; ++stage) {
        store_zero((uint8_t *)patch_store_stage(stage),
                   (uint32_t)sizeof(AVPPatchStageHeader));
    }
}

/*
 * Open a stage.  Only the header is cleared: the patch array behind it is
 * overwritten by the appends that follow, and a stage that is opened and never
 * sealed reads as empty because the magic is gone.
 */
bool patch_store_open(uint32_t stage)
{
    AVPPatchStageHeader *header = patch_store_stage(stage);

    if (header == 0) {
        return false;
    }
    store_zero((uint8_t *)header, (uint32_t)sizeof(*header));
    return true;
}

/*
 * Take one chunk.  The ordinal has to be the one expected, so a retried or
 * reordered control transfer cannot quietly leave a hole in the table.
 */
bool patch_store_append(
    uint32_t stage,
    uint32_t chunk,
    const uint8_t *bytes,
    uint32_t size
) {
    AVPPatchStageHeader *header = patch_store_stage(stage);

    if (header == 0 || bytes == 0 || size == 0) {
        return false;
    }
    if (header->magic == PATCH_STORE_MAGIC) {
        return false;                       /* already sealed */
    }
    if (chunk != header->next_chunk) {
        return false;
    }
    if (header->received > PATCH_STAGE_STRIDE - size) {
        return false;
    }
    store_copy((uint8_t *)header + header->received, bytes, size);
    /*
     * The first chunk carries the header the host built, magic included.  That
     * magic must not stand until the whole blob is here, so it is cleared on
     * the way in and written back by the seal.
     */
    if (header->received == 0 && size >= sizeof(*header)) {
        header->magic = 0;
    }
    header->received += size;
    header->next_chunk = chunk + 1;
    return true;
}

/*
 * Seal it.  The count the host states has to agree with the bytes that arrived,
 * which is what makes a truncated upload an error here rather than a wrong
 * patch somewhere far away.
 */
bool patch_store_seal(uint32_t stage, uint32_t expected_count)
{
    AVPPatchStageHeader *header = patch_store_stage(stage);
    uint32_t body;

    if (header == 0 || header->magic == PATCH_STORE_MAGIC) {
        return false;
    }
    if (header->received < sizeof(*header)) {
        return false;
    }
    body = header->received - (uint32_t)sizeof(*header);
    if (body != expected_count * (uint32_t)sizeof(AVPImagePatch)) {
        return false;
    }
    if (header->version != PATCH_STORE_VERSION) {
        return false;
    }
    header->count = expected_count;
    header->magic = PATCH_STORE_MAGIC;
    return true;
}

/*
 * What the host can read back to see what the ROM believes it holds: for each
 * stage the patch count, the bytes taken and whether it is sealed.
 */
uint32_t patch_store_report(uint8_t *out, uint32_t capacity)
{
    enum { PATCH_STORE_REPORT_RECORD_SIZE = 12 };
    const uint32_t size = PATCH_STAGE_COUNT * PATCH_STORE_REPORT_RECORD_SIZE;
    uint32_t stage;

    if (out == 0 || capacity < size) {
        return 0;
    }
    for (stage = 0; stage < PATCH_STAGE_COUNT; ++stage) {
        const AVPPatchStageHeader *header = patch_store_stage(stage);
        const bool sealed = header->magic == PATCH_STORE_MAGIC;
        uint8_t *record = out + stage * PATCH_STORE_REPORT_RECORD_SIZE;

        write_u32(record, sealed ? header->count : 0);
        write_u32(record + 4, header->received);
        write_u32(record + 8, sealed ? 1 : 0);
    }
    return size;
}
