#pragma once

#include "avp/runtime/memory/runtime_page_attributes_recovered.h"

enum {
    RUNTIME_PAGE_ATTRIBUTE_BITMAP_WORD_BITS = 64,
};

_Noreturn void runtime_page_attributes_fail(void);
size_t runtime_page_attribute_bitmap_bytes(uint32_t page_count);
RuntimePageAttributePageSpan
runtime_page_attribute_range_to_pages(RuntimePageAttributeMemoryRange range);
RuntimePageAttributeRecord *
runtime_page_attribute_require_record(RuntimePageAttributeRegistry *registry,
                                      RuntimePageAttributeLookupResult lookup,
                                      RuntimePageAttributePageSpan pages);
RuntimeFirebloomSpan runtime_page_attribute_make_bitmap_span(uint8_t *storage,
                                                             size_t size);
