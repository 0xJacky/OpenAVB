.section .text.mailbox,"ax",@progbits
.p2align 2

.global avp_mailbox_load_pair
.type avp_mailbox_load_pair,@function
avp_mailbox_load_pair:
    mov x8, x0
    mov x11, x1
    mov x12, x2
    ldp x8, x9, [x8]
    str x8, [x11]
    str x9, [x12]
    ret
.size avp_mailbox_load_pair, .-avp_mailbox_load_pair

.global avp_mailbox_store_pair
.type avp_mailbox_store_pair,@function
avp_mailbox_store_pair:
    mov x8, x0
    mov x9, x1
    mov x10, x2
    stp x9, x10, [x8]
    ret
.size avp_mailbox_store_pair, .-avp_mailbox_store_pair
