@ Applet reading data from modem memory via command 11
@
@ Input:
@ R0 = address of the beginning of this code (start point)
@ R1 = address of the response buffer
@

      .org    0	
      .byte   0,0 	  @ Alignment bytes - trimmed from the object module
      .byte   0x11,0      @ Command code 11 and alignment byte - remains in the object module

start: 
       LDR    R3,srcadr           @ R3 = Address of the data to be read from modem memory
       LDR    R4,lenadr           @ R4 = Size of the data to be read in bytes
       MOV    R0,#0x12            @ Response code
       STR    R0,[R1],#4          @ Save it in the response buffer
       ADD    R0,R3,R4            @ R0 = Address of the end of the data block

locloop:   
       LDR    R2,[R3],#4	  
       STR    R2,[R1],#4
       CMP    R3,R0
       BCC    locloop
       ADD    R4,#4               @ Response size - 4 bytes for the code and data
       BX     LR

srcadr:.word  0
lenadr:.word  0
