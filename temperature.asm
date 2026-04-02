; temperature.asm — x86-64 Linux (NASM): count lines in temperature_data.txt
; Assemble/link (64-bit):  nasm -f elf64 temperature.asm && ld temperature.o -o temperature
;
; ---------------------------------------------------------------------------
; FILE HANDLING
; We open "temperature_data.txt" read-only (O_RDONLY = 0), read the file in a
; loop into a fixed buffer until EOF. Each sys_read returns how many bytes
; were read; 0 means EOF. Large files are handled by processing each chunk
; immediately while preserving line state across reads (so a line can span
; two reads). The file descriptor is closed before exit.
; ---------------------------------------------------------------------------
; LINE COUNTING / STRING TRAVERSAL
; We walk the buffer byte-by-byte. A "line" ends at LF (\n) or CR (\r).
; CR followed by LF (Windows CRLF) counts as a single line terminator: we
; consume both bytes but increment totals once. A line is "valid" if it
; contains at least one non-whitespace character (tab, space, CR, LF are not
; content). Empty lines (only whitespace or nothing between newlines) count
; toward total readings only.
; ---------------------------------------------------------------------------
; CONTROL FLOW
; Outer loop: read chunks until EOF. Inner loop: scan each byte with
; conditional branches for CR/LF/whitespace/content. Program terminates via
; sys_exit after printing two decimal numbers and a clean shutdown.
; ---------------------------------------------------------------------------

section .data
    filename    db "temperature_data.txt", 0
    msg_total   db "Total readings: ", 0
    msg_valid   db "Valid readings: ", 0
    err_open    db "Error: could not open temperature_data.txt", 10
    err_open_len equ $ - err_open

section .bss
    alignb 8
    fd          resq 1
    buffer      resb 65536
    ; Counters persist across chunked reads:
    total_lines resq 1
    valid_lines resq 1
    ; Current line has at least one non-whitespace char
    line_nonempty resb 1
    ; Previous chunk ended with CR and we could not peek LF (CRLF may span reads)
    pending_cr    resb 1

section .text
    DEFAULT ABS
    global _start

; Write C-string in rsi (null-terminated) to stdout
print_zstring:
    push rbx
    mov rbx, rsi
.sz_loop:
    cmp byte [rbx], 0
    je .sz_done
    inc rbx
    jmp .sz_loop
.sz_done:
    mov rdx, rbx
    sub rdx, rsi
    mov rax, 1
    mov rdi, 1
    syscall
    pop rbx
    ret

; Print unsigned 64-bit number in rax (decimal), followed by newline
print_u64:
    push rbx
    push r12
    mov r12, rsp
    sub rsp, 40
    mov rbx, 10
    lea rdi, [rsp + 31]
    mov byte [rdi], 10
    dec rdi
    mov rcx, 1
.pu_loop:
    xor rdx, rdx
    div rbx
    add dl, '0'
    mov [rdi], dl
    dec rdi
    inc rcx
    test rax, rax
    jnz .pu_loop
    inc rdi
    mov rax, 1
    mov rsi, rdi
    mov rdx, rcx
    mov rdi, 1
    syscall
    mov rsp, r12
    pop r12
    pop rbx
    ret

; Process rcx bytes starting at rbx — updates total_lines, valid_lines, line_nonempty
process_chunk:
    push rbx
    push r12
    push r13
    lea r13, [rbx + rcx]

    ; If last read ended with CR, pair with leading LF in this chunk if present
    cmp byte [pending_cr], 0
    je .pc_byte
    cmp rbx, r13
    jge .pc_done
    mov byte [pending_cr], 0
    movzx eax, byte [rbx]
    cmp al, 10
    jne .pc_pending_lone_cr
    inc rbx
.pc_pending_lone_cr:
    jmp .pc_finish_line

.pc_byte:
    cmp rbx, r13
    jge .pc_done
    movzx eax, byte [rbx]

    cmp al, 13
    je .pc_cr
    cmp al, 10
    je .pc_lf

    ; whitespace? tab=9, space=32
    cmp al, 9
    je .pc_ws
    cmp al, 32
    je .pc_ws

    mov byte [line_nonempty], 1
    inc rbx
    jmp .pc_byte

.pc_ws:
    inc rbx
    jmp .pc_byte

.pc_cr:
    lea rdx, [rbx + 1]
    cmp rdx, r13
    jge .pc_cr_split
    cmp byte [rdx], 10
    jne .pc_endline
    add rbx, 2
    jmp .pc_finish_line
.pc_cr_split:
    mov byte [pending_cr], 1
    inc rbx
    jmp .pc_byte
.pc_endline:
    inc rbx
    jmp .pc_finish_line

.pc_lf:
    inc rbx
    jmp .pc_finish_line

.pc_finish_line:
    mov rax, [total_lines]
    inc rax
    mov [total_lines], rax
    cmp byte [line_nonempty], 0
    je .pc_reset_line
    mov rax, [valid_lines]
    inc rax
    mov [valid_lines], rax
.pc_reset_line:
    mov byte [line_nonempty], 0
    jmp .pc_byte

.pc_done:
    pop r13
    pop r12
    pop rbx
    ret

_start:
    xor rax, rax
    mov [total_lines], rax
    mov [valid_lines], rax
    mov byte [line_nonempty], 0
    mov byte [pending_cr], 0

    mov rax, 2
    mov rdi, filename
    xor esi, esi
    syscall
    cmp rax, 0
    js .open_fail
    mov [fd], rax

.read_loop:
    mov rax, 0
    mov rdi, [fd]
    mov rsi, buffer
    mov rdx, 65536
    syscall
    cmp rax, 0
    jle .after_read
    mov rcx, rax
    mov rbx, buffer
    call process_chunk
    jmp .read_loop

.after_read:
    ; Trailing CR without LF (or CR split at EOF): count that line
    cmp byte [pending_cr], 0
    je .after_trailing_cr_done
    mov byte [pending_cr], 0
    mov rax, [total_lines]
    inc rax
    mov [total_lines], rax
    cmp byte [line_nonempty], 0
    je .after_trailing_clear
    mov rax, [valid_lines]
    inc rax
    mov [valid_lines], rax
.after_trailing_clear:
    mov byte [line_nonempty], 0
.after_trailing_cr_done:
    ; If file had no trailing newline, count the last partial line
    cmp byte [line_nonempty], 0
    je .close_file
    mov rax, [total_lines]
    inc rax
    mov [total_lines], rax
    mov rax, [valid_lines]
    inc rax
    mov [valid_lines], rax

.close_file:
    mov rax, 3
    mov rdi, [fd]
    syscall

    mov rsi, msg_total
    call print_zstring
    mov rax, [total_lines]
    call print_u64

    mov rsi, msg_valid
    call print_zstring
    mov rax, [valid_lines]
    call print_u64

    mov rax, 60
    xor edi, edi
    syscall

.open_fail:
    mov rax, 1
    mov rdi, 2
    mov rsi, err_open
    mov rdx, err_open_len
    syscall
    mov rax, 60
    mov edi, 1
    syscall
