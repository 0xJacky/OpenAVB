ENTRY(_source_ibss_start)

SOURCE_BASE = 0x7006c000;
SOURCE_LIMIT = 0x700ec000;
DATA_BASE = 0x70300000;
DATA_LIMIT = 0x70310000;
STACK_BOTTOM = 0x703f0000;
STACK_TOP = 0x70400000;

SECTIONS
{
    . = SOURCE_BASE;
    __source_image_base = .;
    .source_image_header : ALIGN(8) {
        KEEP(*(.source_image_header))
    }
    .text : ALIGN(16) {
        KEEP(*(.text.start))
        *(.text .text.*)
    }
    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
        KEEP(*(.source_image_tail))
        . = ALIGN(16);
    }
    __data_load_start = .;

    .data DATA_BASE : AT(__data_load_start) {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.* COMMON)
        __bss_end = .;
    }

    __source_image_end = __data_load_start + SIZEOF(.data);
    __stack_bottom = STACK_BOTTOM;
    __stack_top = STACK_TOP;
    /* The same byte the ROM publishes, at the top of this payload's writable
     * window rather than the ROM aperture.  The payload never boots a chain and
     * so never clears it; it needs the symbol because source_image.c is shared,
     * and the branch that writes it stopped folding away once the image
     * description could come from the host instead of the compiled profile. */
    avp_el2_present = DATA_LIMIT - 16;

    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_addr 0 : { *(.debug_addr) }
    .debug_frame 0 : { *(.debug_frame) }
    .debug_info 0 : { *(.debug_info .debug_info.*) }
    .debug_line 0 : { *(.debug_line .debug_line.*) }
    .debug_line_str 0 : { *(.debug_line_str) }
    .debug_loclists 0 : { *(.debug_loclists) }
    .debug_rnglists 0 : { *(.debug_rnglists) }
    .debug_str 0 : { *(.debug_str .debug_str.*) }
    .debug_str_offsets 0 : { *(.debug_str_offsets) }

    /DISCARD/ : {
        *(.comment)
        *(.eh_frame*)
        *(.note*)
        *(.llvm_addrsig)
    }
}

ASSERT(SIZEOF(.source_image_header) == 32, "source image header size changed")
ASSERT(__source_image_end <= SOURCE_LIMIT, "source iBSS exceeds its staging region")
ASSERT(__bss_end <= DATA_LIMIT - 16, "source iBSS writable state exceeds its region")
ASSERT(__stack_bottom >= DATA_LIMIT, "source iBSS stack overlaps writable state")
