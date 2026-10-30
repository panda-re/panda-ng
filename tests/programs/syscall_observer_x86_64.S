/* SPDX-License-Identifier: GPL-2.0-or-later
 * Functional native/legacy observation fixture. Build only on manager approval:
 * as --64 SOURCE -o fixture.o
 * ld -static -z noseparate-code -Ttext-segment=0x400000 -e _start fixture.o -o fixture
 * Keep symbols (host test reads exact syscall PCs with nm). No libc/downloads.
 * Modes: default basic; argv[1] starting 's': signal-restarted blocking read.
 */
.intel_syntax noprefix
.global _start, six_call, late_call, unknown_call, fork_call, parent_exit, child_exit
.global restart_read, signal_exit
.text
_start:
    cmp qword ptr [rsp], 1
    jbe basic
    mov rax, [rsp+16]
    cmp byte ptr [rax], 's'
    je signals
basic:
    mov eax, 9 /* mmap: all six native x64 syscall argument registers */
    xor edi, edi
    mov esi, 4096
    mov edx, 3
    mov r10d, 0x22
    mov r8, -1
    xor r9d, r9d
six_call:
    syscall
    test rax, rax
    js fail
    mov rdi, rax
    mov esi, 4096
    mov eax, 11
    syscall
    mov eax, 39 /* callback subscribes to this return during entry */
late_call:
    syscall
    test rax, rax
    jle fail
    mov eax, 7000 /* unknown to generated tables, within trap watch capacity */
unknown_call:
    syscall
    cmp rax, -38
    jne fail
    mov eax, 57 /* child receives a different CR3: never waive this assertion */
fork_call:
    syscall
    test rax, rax
    js fail
    jz child
    mov rdi, rax
    xor esi, esi
    xor edx, edx
    xor r10d, r10d
    mov eax, 61
    syscall
    test rax, rax
    jle fail
    xor edi, edi
    mov eax, 60
parent_exit:
    syscall
child:
    xor edi, edi
    mov eax, 60
child_exit:
    syscall
fail:
    mov edi, 71
    mov eax, 60
    syscall

signals:
    /* SA_RESTART | SA_RESTORER, SIGUSR1; supplied restorer issues rt_sigreturn. */
    mov eax, 13
    mov edi, 10
    lea rsi, [rip+action]
    xor edx, edx
    mov r10d, 8
    syscall
    test rax, rax
    js fail
    mov eax, 39
    syscall
    mov r12, rax
    lea rdi, [rip+fds]
    mov eax, 22
    syscall
    test rax, rax
    js fail
    mov eax, 57
    syscall
    test rax, rax
    js fail
    jz signal_child
    mov r13, rax
    mov eax, 0
    mov edi, [rip+fds]
    lea rsi, [rip+pipe_byte]
    mov edx, 1
restart_read:
    syscall
    cmp rax, 1
    jne fail
    cmp byte ptr [rip+handled], 1
    jne fail
    mov eax, 61
    mov rdi, r13
    xor esi, esi
    xor edx, edx
    xor r10d, r10d
    syscall
    test rax, rax
    jle fail
    xor edi, edi
    mov eax, 60
signal_exit:
    syscall
signal_child:
    lea rdi, [rip+delay]
    xor esi, esi
    mov eax, 35 /* functional delay lets parent block; not a timing metric */
    syscall
    mov rdi, r12
    mov esi, 10
    mov eax, 62
    syscall
    lea rdi, [rip+delay]
    xor esi, esi
    mov eax, 35
    syscall
    mov edi, [rip+fds+4]
    lea rsi, [rip+pipe_byte]
    mov edx, 1
    mov eax, 1
    syscall
    xor edi, edi
    mov eax, 60
    syscall
handler:
    mov byte ptr [rip+handled], 1
    ret
restorer:
    mov eax, 15
    syscall
.data
.balign 8
action: .quad handler, 0x14000000, restorer, 0
delay: .quad 1, 0
fds: .long 0, 0
pipe_byte: .byte 42
handled: .byte 0
