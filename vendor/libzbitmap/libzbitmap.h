/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2022 Corellium LLC */

#pragma once

#include <stddef.h>

#define ZBM_NOMEM (-1)
#define ZBM_INVAL (-2)
#define ZBM_RANGE (-3)
#define ZBM_OVERFLOW (-4)

int zbm_decompress(
    void *destination,
    size_t destination_size,
    const void *source,
    size_t source_size,
    size_t *output_size
);
