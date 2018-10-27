; Bootsector
;
;  Loads `kernel` from hard disk. Filesystem on the disk is TAR.
;  * the file to load must be within the first ~ 20Mb of the primary HDD

[BITS 16]
[ORG 0x7C00]

mov sp, 0x7C00
mov si, loadingMsg
call println

; Get drive CHS dimensions:
	mov ah, 0x08      ; BIOS function
	mov dl, 0x80      ; drive selection (0x80 primary)
	mov di, 0x0       ; A bios quirk
	mov es, di
	int 0x13

	mov [maxSector], cl    ; bits 0-5 of cl are the max Sector
	and word [maxSector], 0x3F

	shr cl, 6              ; bits 6,7 of cl are the high 2 bits of the max Cylinder
                           ; ch is the low 8 bits of the max Cylinder
	xchg cl, ch

	mov [maxCylinder], cx
	mov [maxHead], dh

findLoader:
; Read file entries until we find the kernel:

.findLoaderLoop:
	mov bx, 0x0
	mov es, bx                 ; destination segment
	mov bx, FILE_HEADER_ADDRESS  ; destination  offset
	mov ax, [fileheaderSector] ; start reading from LBA sector 1 (0 = bootsector)
	call readsectorLBA

; Check if we hit the end (Null-filled entry)
	mov ax, [FILE_HEADER_ADDRESS]
	test al, al
	jz kernelNotFound

;Get the filesize:
	; Numbers in the TAR format are ASCII octals :(
	; Location of the filesize within the TAR entry:

	mov si, FILE_HEADER_ADDRESS + FILE_SIZE_OFFSET 

	; 12 characters null-terminated, per TAR format spec
	mov cx, 0x0B
	call parseOctal

	; TAR files have a granularity of 512 bytes.
	; file_sectors = ( (filesize + 511) / 512 )
	mov edx, 0
	add eax, 511
	mov ebx, 512
	div ebx

	; the number of sectors occupied by this file:
	mov [filesizeSectors], ax
	
; Check the file name:
	mov si, kernelName
	mov cx, kernelNameLength
	mov di, FILE_HEADER_ADDRESS   ; location of the filename within the tar entry
	
	call strncmp

	; if the file name matched, break loop
	jz kernelFound

	; Else, skip past this file and check the next one.

	; add the size of the TAR entry header (1 sector):
	mov ax, [filesizeSectors]
	inc ax
	
	; add this to our current sector to get the next entry:
	add [fileheaderSector], ax
	jmp .findLoaderLoop

kernelFound:

	; Destination address:
	mov bx, LOADER_ADDRESS
	shr bx, 0x04
	mov es, bx
	mov bx, 0x0
	mov ax, [fileheaderSector]  
	inc ax                     ; Skip the header, load the file contents
.loadFileLoop:
	push es                    ; phys. address destination
	push ax                    ; sector number
	call readsectorLBA
	pop ax
	pop es
	
	inc ax
	mov di, es
	add di, 0x20              ;add sector size
	mov es, di

	dec word [filesizeSectors]
	jnz .loadFileLoop

	mov si, kernelFoundMsg
	call println

; Jump to kernel:
	jmp LOADER_ADDRESS

	
kernelNotFound:
	mov si, kernelNotFoundMsg
	call println
	.hltloop:
	wait
	jmp .hltloop

; parseOctal - turn an ascii string octal into an integer
;
; params:
;  si - input ascii string
;
; returns:
;  ax - parsed integer
parseOctal:

	mov ebx, 0x00      ; result
	mov ds, bx        ; clear ds for lodsb instruction
	cld               ; make sure we scan in the right direction

	.loop:
		shl ebx, 0x03      ; each octal digit is 3 bits
		lodsb             ; load character from [si] into al
		
		sub al, '0'       ; subtract ascii '0'
		and al, 0x07      ; mask away bad bits because fuck validation (for now)
	
		add bl, al

		dec cx
		jnz .loop

	mov eax, ebx
	ret


; readsectorLBA - Read HDD sector
;
; Params:
;  ax - 16-bit LBA address
;  es:bx - destination

readsectorLBA:
	mov dx, 0x00          ; high word of numerator
	div word [maxSector]  ; LBA / Sectors
	inc dx                ; sectors start at 1

	mov cl, dl            ; sector number
	and cl, 0x3F          ; likely unnecessary, but mask bits anway

	mov dx, 0x00          ; clear high word again

	div word [maxHead]    ; ( LBA / Sectors ) / Heads

	mov dh, dl            ; head number
	mov ch, al            ; low 8 bits of cylinder

	and ah, 0x03
	shl ah, 0x06          ; high 2 bits of cylinder

	add cl, ah            ; bits 0-5 are the sector, bits 6,7 are the high 2 bits of the cylinder	

	mov dl, 0x80    ; drive number (0x80 primary, 0x81 secondary)

	mov ah, 0x02    ; function
	mov al, 0x01    ; number of sectors to read
	int 0x13

	ret

; strncmp - Compare strings
;
; params:
;  si - str1
;  di - str2
;  cx - length
; 
; return:
;  zf - 1 if equal, else 0

strncmp:
	;assume length > 0
		.loop:

	;compare characters:
		cmpsb
		jne .endloop

		dec cx
		jnz .loop
		.endloop:

	;equal: zf set
	;not equal: zf clear
		ret

; print null terminated string
;
; params:
;  ds:si - string
print:
	mov ax, [cursorY]
	mov bx, 80
	mul bl
	add ax, [cursorX]
	shl ax, 1
	mov di, ax
	mov dx, 0xb800
	mov es, dx
	; limit string to 255
	mov cx, 0
	mov ax, 0x0700
	.printloop:
		dec cl
		jz .endprint
		lodsb
		test al, al
		jz .endprint
		stosw
		jmp .printloop

	.endprint:
	mov ax, di
	shr di, 1
	mov dx, 0
	div bx
	mov [cursorX], dx
	mov [cursorY], ax
	ret

println:
	call print
	inc word [cursorY]
	mov word [cursorX], 0
	ret


; Variables:

loadingMsg: db 'Searching for '
kernelName: db 'kernel', 0x00
kernelNameLength equ $ - kernelName - 1

kernelFoundMsg: db 'kernel found and loaded', 0x00
kernelNotFoundMsg: db 'Error: kernel not found', 0x00

LOADER_ADDRESS      equ 0x7E00 ; a common place to put a second kernel, ca 480Kb free.
FILE_HEADER_ADDRESS equ 0x0500 ; just an available spot in real memory
FILE_SIZE_OFFSET    equ 0x7C   ; where the fileSize field is in the Tar entry

fileheaderSector: dw 0x01    ; start scanning for the file at sector 1 (0 = bootsector)
filesizeSectors: dw 0x00     ; this will be the number of sectors occupied by the file

maxCylinder: dw 0x0000
maxHead:     dw 0x0000
maxSector:   dw 0x0000

cursorX:   dw 0x0000
cursorY:   dw 0x0000


; Padding
	times (512 - ($-$$) - 2) db 0x90

; Boot signature
	db 0x55, 0xAA

