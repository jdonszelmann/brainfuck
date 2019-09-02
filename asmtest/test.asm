
BITS 64;

section .bss 
	memory: 
		resb 30000

	iobuffer: 
		resb 30000
; .data
; 	scanf_int_format: .asciz "%c"
; 	inputint:.long 0
section .text
	; get_int:
	; 	push %rbp
	; 	movq %rsp, %rbp
	; 	push $inputint
	; 	push $scanf_int_format
	; 	call scanf
	; 	movq inputint, %rax
	; 	movq %rbp, %rsp
	; 	pop %rbp
	; 	ret

	global main
	main:
		mov rbx, memory
		add rbx, $15000

		mov rcx, iobuffer

		nop
		nop
		nop
		nop

		inc byte [rbx+300]

		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop
		nop

		nop
		nop
		nop
		nop

		ret
