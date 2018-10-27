[BITS 16]
extern initTextRender
extern render_text_xy
extern _start

; BIOS enable A20 line
	mov ax, 0x2401
	int 0x15

; BIOS set SVGA mode
	mov bx, 0x116 ; 1024x768x32k
	mov ax, 0x4f02
	int 0x10

; BIOS get specific SVGA mode info
	mov bx, 0x0
	mov es, bx
	mov di, 0x0500
	mov cx, 0x116
	mov ax, 0x4f01
	int 0x10

	mov bx, 0x0
	mov es, bx
	mov di, 0x0500

	xor eax, eax
	mov ax, word [0x0512]
	mov dword [VGA_WIDTH], eax
	mov ax, word [0x0514]
	mov dword [VGA_HEIGHT], eax

; BIOS: Get system memory map:
;	mov ebx, 0x0
;	mov es, bx
;	mov di, 0x0500

;memmap_loop:
;	mov eax, 0xE820
;	mov edx, 0x534D4150
;	mov ecx, 0x18

; 	int 0x15
; 	jc after_memmap
; 	test ebx, ebx
; 	jz after_memmap
	
; 	add edi, 0x18
; 	inc dword [memMapSize]

; 	jmp memmap_loop

;after_memmap:
;mov eax, [memMapSize]

; Enter protected mode
	cli
	mov bx, 0x0
	mov es, bx
	mov ds, bx
	mov ss, bx
	mov fs, bx
	mov gs, bx	

	lgdt [GDTR]
	o32 mov eax, cr0
	or al, 1
	o32 mov cr0, eax

	jmp dword 0x08:protectedMode
protectedMode:
[bits 32]
	mov ebx, 0x10
	mov es, bx
	mov ds, bx
	mov ss, bx
	mov fs, bx
	mov gs, bx	

; plot a single pixel while in protected mode
mov word [0xE00C0400], 0xFFFF

; init ascii map for vga printing
	call initTextRender

; print a message using external call
	; color:
	push dword 0xFFFF
	; x, y:
	push dword 0
	push dword 0
	push dword callingStartMsgLen
	push callingStartMsg
	call render_text_xy
	add esp, 0x14 

call _start


memMapSize: dd 0x0

callingStartMsg: db `Calling _start():\n`, 0x00
callingStartMsgLen equ $ - callingStartMsg - 1


global VGA_WIDTH
global VGA_HEIGHT
global exportedMsg

VGA_WIDTH: dd 0
VGA_HEIGHT: dd 0
exportedMsg: db 'EXPORTED STRING', 0x00

ALIGN 16
GDT:

.null:
dd 0x00000000
dd 0x00000000

.system_code:
dw 0xFFFF
dw 0x0000
db 0x00
; NOTE: If this is marked as conforming, interrupts will triple fault.
db 10011010b  ; access byte
db 11001111b  ; flags & limit 16:19
db 0x00

.system_data:
dw 0xFFFF
dw 0x0000
db 0x00
db 10010010b  ; access byte
db 11001111b  ; flags & limit 16:19
db 0x00

.user_code:
dw 0xFFFF
dw 0x0000
db 0x00
db 11111010b  ; access byte
db 11001111b  ; flags & limit 16:19
db 0x00

.user_data:
dw 0xFFFF
dw 0x0000
db 0x00
db 11110010b  ; access byte
db 11001111b  ; flags & limit 16:19
db 0x00

.kernel_ts:
dw 0x0067     ; limit
dw 0x0D00     ; base
db 0x00
db 10001001b  ; access byte
db 00000000b  ; flags & limit 16:19
db 0x00

.kernel_stack:
dw 0x0001
dw 0x0E00
db 0x00
db 10010010b  ; access byte
db 11000000b  ; flags & limit 16:19
db 0x00
.limit equ $ - GDT - 1

GDTR:
dw GDT.limit
dd GDT

