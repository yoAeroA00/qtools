@ Applet for reading the chipset identification word from the modem via command 11
@
@ Input:
@ R0 = address of the beginning of this code (start point)
@ R1 = address of the response buffer
@

      .org    0	
      .byte   0,0 	  @ Alignment bytes - trimmed from the object module
      .byte   0x11,0      @ Command code 11 and alignment byte - remains in the object module

start: 
       PUSH	{R1}
       MOV	R0,LR		@ Address of the command 11 handler in the loader
       BIC	R0,#3           @ Round to the word boundary
       ADD	R3,R0,#0xFF	@ Boundary for pattern search
       LDR	R1,=0xDEADBEEF  @ Pattern to search for
floop:                          
       LDR	R2,[R0],#4      @ Next word
       CMP	R2,R1           @ Is this the pattern?
       BEQ	found           @ Yes - finally found
       CMP	R0,R3           @ Reached the boundary?
       BCC	floop           @ No - continue searching
@ Pattern not found
       MOV	R0,#0           @ Response 0 - pattern not found
       B	done            
@ Found the pattern
found:                          
       LDR	R0,[R0]         @ Extract the chipset code
done:             
       POP	{R1}
       STRB	R0,[R1,#1]      @ Save the code in byte 1
       MOV	R0,#0xAA        @ AA - response code for identification mode
       STRB	R0,[R1]         @ in byte 0
       MOV	R4,#2           @ Response size - 2 bytes
       BX       LR              
