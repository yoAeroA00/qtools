@ Applet for testing the loader's functionality
@
@ Input:
@ R0 = the address of the beginning of this code (start point)
@ R1 = address of the response buffer
@
@ Returns response 12
@

      .org    0	
      .byte   0,0 	  @ Alignment bytes - trimmed from the object module
      .byte   0x11,0      @ Command code 11 and alignment byte - remains in the object module

start: 
       MOV	R0,#0x12;
       STRB	R0,[R1]
       MOV	R4,#1
       BX       LR
