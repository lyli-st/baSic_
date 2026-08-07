; baSic_ - boot/boot.asm
; Copyright (C) 2026 Dhrubo
; GPL v2 — see LICENSE

%if 0
    MBR bootloader — loads stage2+kernel into 0x8000 via INT 13h AH=42h
    real hardware bug fixed here:
    single large DAP reads silently short read on real BIOSes without
    setting CF. QEMU tolerated 128 sectors in one call; Dell BIOS did not.
    * fix: chunked 32-sector reads with 3 retries each.
%endif
[BITS 16]
[ORG 0x7C00]

KERNEL_LOAD_ADDR  equ 0x8000   ; physical address to load stage2+kernel
TOTAL_SECTORS     equ 192      ; sectors to load (was 64+128; keep same
                                ; total budget, just read it safely)
CHUNK_SECTORS     equ 32       ; sectors per INT 13h call (16KB, safe
                                ; on essentially all real BIOSes)
MAX_RETRIES       equ 3        ; retries per chunk before giving up

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; stack grows down from MBR
    sti
    mov [boot_drive], dl    ; BIOS passes boot drive in DL
    mov si, msg_loading
    call bios_print
    call load_kernel
    mov si, msg_ok
    call bios_print
    ; hand off to stage2
    jmp KERNEL_LOAD_ADDR

; chunked read: 32 sectors at a time, 3 retries per chunk
load_kernel:
    mov dword [cur_lba], 1                 ; starting LBA (sector 2, 0-indexed=1)
    mov dword [cur_phys], KERNEL_LOAD_ADDR
    mov word  [sectors_left], TOTAL_SECTORS

.next_chunk:
    mov ax, [sectors_left]
    or  ax, ax
    jz  .done                              ; all sectors loaded

    ; this_chunk = min(CHUNK_SECTORS, sectors_left)
    mov ax, CHUNK_SECTORS
    cmp ax, [sectors_left]
    jbe .size_ok
    mov ax, [sectors_left]
.size_ok:
    mov [this_chunk], ax

    ; build DAP for this chunk
    mov byte  [dap_size], 0x10
    mov byte  [dap_zero], 0x00
    mov ax,   [this_chunk]
    mov [dap_count], ax
    mov word  [dap_off], 0                 ; always offset 0 (see header note)
    ; segment = physical_addr >> 4
    mov eax, [cur_phys]
    shr eax, 4
    mov [dap_seg], ax
    mov eax, [cur_lba]
    mov [dap_lba], eax
    mov dword [dap_lba+4], 0

    mov si, MAX_RETRIES
.retry:
    push si
    mov si, dap
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    pop si
    jnc .chunk_ok
    dec si
    jnz .retry
    jmp disk_error                         ; exhausted retries -> real failure

.chunk_ok:
    ; advance LBA by this_chunk sectors
    movzx eax, word [this_chunk]
    add [cur_lba], eax
    ; advance physical dest by this_chunk * 512 bytes
    movzx eax, word [this_chunk]
    shl eax, 9                             ; * 512
    add [cur_phys], eax
    ; sectors_left -= this_chunk
    mov ax, [this_chunk]
    sub [sectors_left], ax
    jmp .next_chunk

.done:
    ret

disk_error:
    mov si, msg_disk_err
    call bios_print
    cli
    hlt

bios_print:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    xor bh, bh
    int 0x10
    jmp bios_print
.done:
    ret

msg_loading  db 'baSic_: loading kernel...', 0x0D, 0x0A, 0
msg_ok       db 'OK. Entering stage2.', 0x0D, 0x0A, 0
msg_disk_err db '[ERROR] Disk read failed!', 0x0D, 0x0A, 0
boot_drive   db 0

; scratch state for load_kernel (flat .com-style image, so just
; reserve space inline before the 510-byte pad)
cur_lba       dd 0
cur_phys      dd 0
sectors_left  dw 0
this_chunk    dw 0

; single reusable Disk Address Packet
dap:
dap_size  db 0x10
dap_zero  db 0x00
dap_count dw 0
dap_off   dw 0
dap_seg   dw 0
dap_lba   dq 0

; Pad to exactly 510 bytes and place boot signature
times 510 - ($ - $$) db 0
dw 0xAA55