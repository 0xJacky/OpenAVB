#pragma once

#include <stdint.h>

#define AVP_CCSHA3_PUBLIC __attribute__((used, retain))

AVP_CCSHA3_PUBLIC void AccelerateCrypto_SHA3_keccak(uint64_t state[25]);
