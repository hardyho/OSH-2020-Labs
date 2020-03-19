[BITS 16]
[ORG 0x7c00]

mov ax,3 
int 10h

and cx,0
and EAX,0
sti
timer:
	mov EBX,EAX
	mov EAX,[0x046c]
	cmp EBX,EAX
	je  timer
	add cx,1
	cmp cx,18
	je print
	jmp timer
print:
	and cx,0
	mov si,OSH
print_str:
	lodsb
	cmp al,0
	je end
	mov ah,0x0e
	int 0x10
	jmp print_str
end:
	mov ah,2
	int 21h
	jmp timer

OSH db 'Hello, OSH 2020 Lab1!' ,0dh,0ah

TIMES 510 - ($ -$$) db 0
DW 0xAA55
