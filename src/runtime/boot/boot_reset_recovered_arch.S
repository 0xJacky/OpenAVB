.section .text.boot_reset_recovered_arch,"axR",@progbits
.p2align 2

/* The canonical source reset implementation remains the linked _start path. */
.global avp_reset_entry
.type avp_reset_entry,@function
avp_reset_entry:
    b _start
.size avp_reset_entry, .-avp_reset_entry
