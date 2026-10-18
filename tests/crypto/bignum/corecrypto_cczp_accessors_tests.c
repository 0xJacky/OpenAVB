#include "avp/crypto/bignum/corecrypto_cczp_accessors.h"

#include <assert.h>
#include <stdint.h>


typedef struct {
    AVPCCSize n;
    AVPCCUnit bit_length;
    const AVPCCZPFunctions *functions;
    AVPCCUnit prime[4];
} TestCCZP;

typedef struct {
    uint32_t operation;
    AVPCCWorkspace workspace;
    const AVPCCZP *zp;
    const AVPCCUnit *left;
    const AVPCCUnit *right;
} DispatchTrace;

static DispatchTrace trace;

static void record_binary(
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *left,
    const AVPCCUnit *right
) {
    trace = (DispatchTrace){5, workspace, zp, left, right};
    result[0] = left[0] + right[0];
}

static void record_unary_operation(
    uint32_t operation,
    AVPCCWorkspace workspace,
    const AVPCCZP *zp,
    AVPCCUnit *result,
    const AVPCCUnit *source
) {
    trace = (DispatchTrace){operation, workspace, zp, source, 0};
    result[0] = source[0] ^ operation;
}

#define DEFINE_UNARY(name, operation) \
    static void name(AVPCCWorkspace workspace, const AVPCCZP *zp, \
                     AVPCCUnit *result, const AVPCCUnit *source) { \
        record_unary_operation(operation, workspace, zp, result, source); \
    }

DEFINE_UNARY(record_square, 4)
DEFINE_UNARY(record_reduce, 3)
DEFINE_UNARY(record_to, 2)
DEFINE_UNARY(record_from, 1)

int main(void) {
    const AVPCCZPFunctions functions = {
        .multiply = record_binary,
        .square = record_square,
        .reduce = record_reduce,
        .to_representation = record_to,
        .from_representation = record_from,
    };
    const TestCCZP context = {
        .n = 4,
        .bit_length = 255,
        .functions = &functions,
        .prime = {
            UINT64_C(0xffffffffffffffff),
            UINT64_C(0x00000000ffffffff),
            UINT64_C(0),
            UINT64_C(0x7fffffffffffffff),
        },
    };
    const AVPCCZP *zp = (const AVPCCZP *)&context;

    assert(cczp_n(zp) == 4);
    assert(cczp_prime(zp) == context.prime);
    assert(cczp_prime(zp)[0] == UINT64_C(0xffffffffffffffff));
    assert(cczp_prime(zp)[3] == UINT64_C(0x7fffffffffffffff));

    AVPCCUnit left[] = {7};
    const AVPCCUnit right[] = {11};
    AVPCCUnit result[] = {0};
    AVPCCWorkspace workspace = (void *)(uintptr_t)0x1234;
    cczp_mod_ws(workspace, zp, result, left);
    assert(trace.operation == 3 && trace.workspace == workspace &&
           trace.zp == zp && result[0] == (7 ^ 3));
    cczp_mul_ws(workspace, zp, result, left, right);
    assert(trace.operation == 5 && trace.left == left &&
           trace.right == right && result[0] == 18);
    cczp_sqr_ws(workspace, zp, result, left);
    assert(trace.operation == 4 && result[0] == (7 ^ 4));
    cczp_to_ws(workspace, zp, result, left);
    assert(trace.operation == 2 && result[0] == (7 ^ 2));
    cczp_from_ws(workspace, zp, result, left);
    assert(trace.operation == 1 && result[0] == (7 ^ 1));
    return 0;
}
