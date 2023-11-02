@ Applet writing data to modem memory via command 11
@
@ On input:
@ R0 = the address of the beginning of this code (start point)
@ R1 = address of the response buffer
@

      .org    0	
      .byte   0,0,0x11,0      @ command code 11 and alignment bytes
start: 
       ADD    R0,R0,#(param-start) @ Address from which the data for writing begins
       LDR    R3,dstadr           @ Address where the data is to be written in the modem
       LDR    R4,lenadr           @ Size of the data to be written in bytes
       ADD    R4,R3,R4            @ Address of the end of the data block

locloop:   
       LDR    R2,[R0],#4	  
       STR    R2,[R3],#4
       CMP    R3,R4
       BCC    locloop
       MOV    R0,#0x12         @ Response code 
       STRB   R0,[R1]          @ Save it in the response buffer
       MOV    R4,#1            @ Response size
       BX     LR

dstadr:.word  0
lenadr:.word  0
param:       
