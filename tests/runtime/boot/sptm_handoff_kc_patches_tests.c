/*
 * kc_patches, run on the host: the handoff's own matcher over a buffer.
 *
 * The rows are encoding-matched rewrites of kernel text.  What makes one safe
 * is not that it matches but how many times: a row meant for one compare that
 * also lands on a second one rewrites code nobody looked at.  So this runs the
 * real kc_relax_variant_check -- the file is included, not copied -- and reads
 * the per-row counts it leaves, which are the same words the ROM leaves at
 * scratch + 0x400 on the machine.
 *
 * Without arguments it checks a synthetic buffer.  With
 *
 *     KC_IMAGE=<decompressed kernelcache Mach-O>
 *
 * it runs the matcher over that image's __TEXT_EXEC and __PPLTEXT, prints
 * every row's count, and with KC_EXPECT=<w2>:<n>[,<w2>:<n>...] fails unless
 * the row whose third word is w2 (hex) matched exactly n times.
 */
#define SPTM_HANDOFF_HOSTED 1
#include "../../../src/runtime/boot/sptm_handoff_tree.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROWS (sizeof(kc_patches) / sizeof(kc_patches[0]))

static int failures;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (!(cond)) {                                                     \
            fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);           \
            fprintf(stderr, __VA_ARGS__);                                  \
            fputc('\n', stderr);                                           \
            failures++;                                                    \
        }                                                                  \
    } while (0)

static int row_by_third(uint32_t w0, uint32_t w1, uint32_t w2)
{
    for (unsigned i = 0; i < ROWS; i++) {
        if (kc_patches[i].word[0] == w0 && kc_patches[i].word[1] == w1 &&
            kc_patches[i].word[2] == w2) {
            return (int)i;
        }
    }
    return -1;
}

static void put(uint8_t *b, size_t word, uint32_t v)
{
    adt_wr32(b + 4 * word, v);
}

static void synthetic(void)
{
    enum { N = 256 };
    static uint8_t text[N * 4];
    uint8_t mark[4], clock[4], counts[4 + 4 * ROWS];
    int tail = row_by_third(0x529f9008u, 0x72a00148u, 0xeb08001fu);
    int budget = row_by_third(0x529f9008u, 0x72a00148u, 0xf94017f4u);

    CHECK(tail >= 0, "no row for movz/movk/cmp x0,x8");
    CHECK(budget >= 0, "no row for movz/movk/ldr");
    if (tail < 0 || budget < 0) {
        return;
    }

    memset(text, 0, sizeof(text));
    for (size_t i = 0; i < N; i++) {
        put(text, i, 0xd503201fu);                 /* nop filler          */
    }
    /* _updateCPUComplexPerfState's tail compare (19A348 / 20A362 shape) */
    put(text, 10, 0x529f9008u);
    put(text, 11, 0x72a00148u);
    put(text, 12, 0xeb08001fu);
    put(text, 13, 0x54000003u);                    /* b.cs                */
    /* ApplePMGR::start's budget store (21A329 shape) */
    put(text, 40, 0x529f9008u);
    put(text, 41, 0x72a00148u);
    put(text, 42, 0xf94017f4u);
    /* the pair followed by something else: must be left alone */
    put(text, 80, 0x529f9008u);
    put(text, 81, 0x72a00148u);
    put(text, 82, 0x5292e00au);

    kc_relax_variant_check(text, sizeof(text), mark, clock, counts);

    CHECK(adt_rd32(counts) == (0x4b430000u | ROWS),
          "counts header %08x", adt_rd32(counts));
    for (unsigned i = 0; i < ROWS; i++) {
        uint32_t want = (i == (unsigned)tail || i == (unsigned)budget) ? 1 : 0;
        CHECK(adt_rd32(counts + 4 + 4 * i) == want, "row %u counted %u, want %u",
              i, adt_rd32(counts + 4 + 4 * i), want);
    }
    CHECK(adt_rd32(text + 4 * 11) == 0x72a057e8u, "tail movk not rewritten: %08x",
          adt_rd32(text + 4 * 11));
    CHECK(adt_rd32(text + 4 * 10) == 0x529f9008u && adt_rd32(text + 4 * 12) == 0xeb08001fu &&
          adt_rd32(text + 4 * 13) == 0x54000003u, "tail neighbours changed");
    CHECK(adt_rd32(text + 4 * 41) == 0x72a057e8u, "budget movk not rewritten");
    CHECK(adt_rd32(text + 4 * 81) == 0x72a00148u, "unmatched pair was rewritten");

    /* a second run starts its counts from zero, not from the last ones */
    kc_relax_variant_check(text, sizeof(text), mark, clock, counts);
    CHECK(adt_rd32(counts + 4 + 4 * (unsigned)tail) == 0,
          "already-rewritten tail matched again (%u)",
          adt_rd32(counts + 4 + 4 * (unsigned)tail));
}

static int scan_image(const char *path, const char *expect)
{
    FILE *f = fopen(path, "rb");
    long n;
    uint8_t *img;
    uint32_t ncmds;
    size_t off = 32;
    uint32_t total[ROWS];

    if (f == NULL) {
        perror(path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    img = malloc((size_t)n);
    if (img == NULL || fread(img, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "%s: short read\n", path);
        return 1;
    }
    fclose(f);
    if (adt_rd32(img) != 0xfeedfacfu) {
        fprintf(stderr, "%s: not a 64-bit Mach-O (decompress it first)\n", path);
        return 1;
    }
    ncmds = adt_rd32(img + 16);
    memset(total, 0, sizeof(total));
    for (uint32_t c = 0; c < ncmds && off + 8 <= (size_t)n; c++) {
        uint32_t cmd = adt_rd32(img + off), size = adt_rd32(img + off + 4);

        if (cmd == 0x19u) {
            char seg[17] = { 0 };
            uint64_t fileoff, filesize;

            memcpy(seg, img + off + 8, 16);
            memcpy(&fileoff, img + off + 40, 8);
            memcpy(&filesize, img + off + 48, 8);
            if ((strcmp(seg, "__TEXT_EXEC") == 0 || strcmp(seg, "__PPLTEXT") == 0) &&
                fileoff + filesize <= (uint64_t)n) {
                uint8_t mark[4], clock[4], counts[4 + 4 * ROWS];

                kc_relax_variant_check(img + fileoff, filesize, mark, clock, counts);
                for (unsigned i = 0; i < ROWS; i++) {
                    total[i] += adt_rd32(counts + 4 + 4 * i);
                }
                printf("%s: %s, %llu bytes\n", path, seg, (unsigned long long)filesize);
            }
        }
        off += size;
    }
    for (unsigned i = 0; i < ROWS; i++) {
        printf("  row %2u {%08x %08x %08x} [%u] -> %08x : %u\n", i,
               kc_patches[i].word[0], kc_patches[i].word[1], kc_patches[i].word[2],
               kc_patches[i].replace, kc_patches[i].with, total[i]);
    }
    while (expect != NULL && *expect != 0) {
        unsigned w2 = 0, want = 0;
        int used = 0, matched = 0;

        if (sscanf(expect, "%x:%u%n", &w2, &want, &used) != 2) {
            fprintf(stderr, "bad KC_EXPECT near '%s'\n", expect);
            return 1;
        }
        for (unsigned i = 0; i < ROWS; i++) {
            if (kc_patches[i].word[2] == w2) {
                matched = 1;
                CHECK(total[i] == want, "row %u (third %08x) matched %u times, want %u",
                      i, w2, total[i], want);
            }
        }
        CHECK(matched, "no row has third word %08x", w2);
        expect += used;
        if (*expect == ',') {
            expect++;
        }
    }
    free(img);
    return 0;
}

int main(void)
{
    const char *image = getenv("KC_IMAGE");

    if (image != NULL && *image != 0) {
        if (scan_image(image, getenv("KC_EXPECT")) != 0) {
            return 1;
        }
    } else {
        synthetic();
    }
    if (failures != 0) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    puts("sptm_handoff_kc_patches_tests: ok");
    return 0;
}
