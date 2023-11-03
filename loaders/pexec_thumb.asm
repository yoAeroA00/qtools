@
@  Command 11 patch for loaders running in Thumb-2 mode
@
.syntax unified
.code 16           @  Thumb-2

pkt_data_len_off=N

.ORG    0xXXXXXXXX

leavecmd:

.ORG    0xXXXXXXXX

cmd_11_exec:
        PUSH    {R4,LR}
        LDR.N   R1, cmd_reply_code_ptr          @ Address of the reply buffer
        ADD     R0, R0,    #8                   @ R0 now points to a byte +4 from the beginning
                                                @ of the command buffer
        MCR     p15, 0, R1,c7,c5,0              @ Flush I-cache
        BLX     R0                              @ Pass control there
        LDR.N   R0, cmd_processor_data_ptr      @ Command handler data structure
        STRH    R4, [R0,#pkt_data_len_off]      @ Save the size of the reply buffer
        B       leavecmd

@ Identification block

        .word           0xdeadbeef          @ Signature
        .byte           N                   @ Chipset code

.ORG    0xXXXXXXXX

cmd_processor_data_ptr: .word  0
cmd_reply_code_ptr:     .word  0

