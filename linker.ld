ENTRY(_start)

ROM_BASE = 0x00100000;
ROM_LIMIT = 0x00180000;
RAM_BASE = 0x70000000;
RAM_STATE_LIMIT = 0x70001000;
/* The d16 iBSS expands through 0x70284a08. Keep the live boot stack beyond
 * both that handoff image and the loader's LZFSE scratch window. */
STACK_BOTTOM = 0x70400000;
STACK_TOP = 0x70410000;

SECTIONS
{
    . = ROM_BASE;
    .text : ALIGN(16) {
        KEEP(*(.text.start))
        KEEP(*(.text.runtime_task_lifecycle))
        KEEP(*(.text.runtime_firebloom_ranges))
        KEEP(*(.text.runtime_firebloom_metadata))
        KEEP(*(.text.runtime_rpc_ranges))
        KEEP(*(.text.runtime_domain_recovered))
        KEEP(*(.text.runtime_address_space_recovered))
        KEEP(*(.text.runtime_task_context_recovered))
        KEEP(*(.text.runtime_bootstrap_memory_recovered))
        KEEP(*(.text.runtime_support_recovered))
        *(.text .text.*)
    }
    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    }
    /* A fixed home for the code iBoot is patched to call at the handoff.
     * The address is baked into that patch, so it has to stay put; the ROM
     * aperture is a megabyte and the image is nowhere near it. */
    . = ROM_BASE + 0x70000;
    .handoff : ALIGN(16) {
        KEEP(*(.handoff))
    }

    . = ALIGN(16);
    __data_load_start = .;

    .data RAM_BASE : AT(__data_load_start) {
        __data_start = .;
        *(.data .data.*)
        __data_end = .;
    }

    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(EXCLUDE_FILE (
            *usb_device.o
            *simple_virtio_dcd.o
            *vendor/tinyusb*
            *vendor/mbedtls*
            *recovered.o
            *owner.o
            *compat.o
            *adapter.o
        ) .bss .bss.* COMMON)
        __bss_end = .;
    }

    .usb_device.state 0x70008000 (NOLOAD) : {
        *usb_device.o(.bss .bss.* COMMON)
        *simple_virtio_dcd.o(.bss .bss.* COMMON)
        *vendor/tinyusb*(.bss .bss.* COMMON)
    }

    .runtime_prng.state 0x70010000 (NOLOAD) : ALIGN(16) {
        KEEP(*(.runtime_prng.state))
        *vendor/mbedtls*(.bss .bss.* COMMON)
    }

    .runtime_auxiliary.state 0x70014000 (NOLOAD) : ALIGN(16) {
        /* Recovered owners keep source-only binding state outside the fixed queue window. */
        __runtime_auxiliary_start = .;
        *recovered.o(.bss .bss.* COMMON)
        *owner.o(.bss .bss.* COMMON)
        *compat.o(.bss .bss.* COMMON)
        *adapter.o(.bss .bss.* COMMON)
        __runtime_auxiliary_end = .;
    }

    .stack_guard.active 0x7002a230 (NOLOAD) : {
        KEEP(*(.stack_guard.active))
    }
    .runtime_provider.head 0x7002a238 (NOLOAD) : {
        KEEP(*(.runtime_provider.head))
    }
    .stack_guard.staging 0x7003ca70 (NOLOAD) : {
        KEEP(*(.stack_guard.staging))
    }
    .runtime_task.boot 0x7003cad0 (NOLOAD) : {
        KEEP(*(.runtime_task.boot))
    }
    .runtime_scheduler.state 0x7003d090 (NOLOAD) : {
        KEEP(*(.runtime_scheduler.state))
    }
    .stack_guard.xorshift 0x7003d0a8 (NOLOAD) : {
        KEEP(*(.stack_guard.xorshift))
    }
    .runtime_ready.count 0x7003d0a0 (NOLOAD) : {
        KEEP(*(.runtime_ready.count))
    }
    .runtime_task.parked 0x7003d160 (NOLOAD) : {
        KEEP(*(.runtime_task.parked))
    }
    .runtime_ready.queue 0x7003d1e0 (NOLOAD) : {
        KEEP(*(.runtime_ready.queue))
    }
    .runtime_timer.queue 0x7003d220 (NOLOAD) : {
        KEEP(*(.runtime_timer.queue))
    }
    .runtime_task.idle 0x7003d7c0 (NOLOAD) : {
        KEEP(*(.runtime_task.idle))
    }
    .runtime_task.state 0x7003e000 (NOLOAD) : {
        /* Source-only registry storage stays clear of canonical fixed globals. */
        KEEP(*(.runtime_task.state))
    }
    .runtime_page_tables 0x70200000 (NOLOAD) : ALIGN(0x4000) {
        KEEP(*(.runtime_page_tables))
    }

    __rom_end = __data_load_start + SIZEOF(.data);
    __stack_bottom = STACK_BOTTOM;
    __stack_top = STACK_TOP;
    /* start.S publishes whether the core was entered at EL2 before the
     * runtime is up, and the handoff reads it much later. The bring-up wipes
     * .bss and the low RAM state window, and the page-table pool covers the
     * space past the boot stack, so it takes the last 16 bytes of the patch
     * store window, which nothing clears and no real table reaches.
     *
     * It must not live in the ROM aperture: Virtualization.framework maps
     * the ROM read-only, and the store there faults before the first C
     * frame (external abort, ESR 0x97130050, at the strb in _start). */
    avp_el2_present = 0x71200000 + 0x00100000 - 16;

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

ASSERT(__rom_end <= ROM_LIMIT, "source ROM exceeds the 512 KiB functional budget")
ASSERT(__bss_end <= RAM_STATE_LIMIT, "runtime state overlaps queue descriptors")
ASSERT(ADDR(.usb_device.state) + SIZEOF(.usb_device.state) <= 0x70010000,
       "USB device state exceeds its reserved window")
ASSERT(ADDR(.runtime_prng.state) + SIZEOF(.runtime_prng.state) <= 0x70014000,
       "runtime PRNG state exceeds its reserved window")
ASSERT(ADDR(.runtime_auxiliary.state) + SIZEOF(.runtime_auxiliary.state) <= 0x7002a230,
       "runtime auxiliary state overlaps canonical fixed globals")
ASSERT(ADDR(.stack_guard.active) == 0x7002a230, "active stack guard IPA")
ASSERT(ADDR(.runtime_provider.head) == 0x7002a238, "provider head IPA")
ASSERT(SIZEOF(.runtime_provider.head) == 0x20, "provider head size")
ASSERT(ADDR(.stack_guard.staging) == 0x7003ca70, "staged stack guard IPA")
ASSERT(ADDR(.runtime_task.boot) == 0x7003cad0, "boot task IPA")
ASSERT(ADDR(.runtime_scheduler.state) == 0x7003d090, "runtime state IPA")
ASSERT(SIZEOF(.runtime_scheduler.state) == 0x10, "runtime state size")
ASSERT(ADDR(.stack_guard.xorshift) == 0x7003d0a8, "xorshift state IPA")
ASSERT(ADDR(.runtime_ready.count) == 0x7003d0a0, "ready count IPA")
ASSERT(SIZEOF(.runtime_ready.count) == 0x4, "ready count size")
ASSERT(ADDR(.runtime_task.parked) == 0x7003d160, "parked task slot IPA")
ASSERT(SIZEOF(.runtime_task.parked) == 0x20, "parked task slot size")
ASSERT(ADDR(.runtime_ready.queue) == 0x7003d1e0, "ready queue IPA")
ASSERT(SIZEOF(.runtime_ready.queue) == 0x40, "ready queue size")
ASSERT(ADDR(.runtime_timer.queue) == 0x7003d220, "timer queue IPA")
ASSERT(SIZEOF(.runtime_timer.queue) == 0x40, "timer queue size")
ASSERT(ADDR(.runtime_task.idle) == 0x7003d7c0, "idle task IPA")
ASSERT(SIZEOF(.runtime_task.idle) == 0x5b0, "idle task size")
ASSERT(ADDR(.runtime_page_tables) == 0x70200000, "runtime page-table IPA")
ASSERT(SIZEOF(.runtime_page_tables) == 0x7c000, "runtime page-table pool")
