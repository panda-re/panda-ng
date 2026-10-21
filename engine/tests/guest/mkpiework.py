#!/usr/bin/env python3
"""Generate piework.c: a freestanding position-independent program that runs
N distinct small functions once each, then exits -- translation-heavy work
whose code lands at a new address per exec when ASLR is on."""
import random, sys
n = int(sys.argv[1]) if len(sys.argv) > 1 else 2000
r = random.Random(1)
out = ['typedef unsigned long u;']
for i in range(n):
    a, b, c = r.randrange(1, 1 << 16), r.randrange(1, 31), r.randrange(1, 1 << 12)
    out.append(f'__attribute__((noinline, used)) u f{i}(u x) {{ if (x & {1 << (i % 13)}) x = x * {a} + {c}; else x ^= x >> {b}; return x + {i}; }}')
out.append('static volatile u seed = 1;')
out.append('static u run(void) { u x = seed;')
out += [f'  x = f{i}(x);' for i in range(n)]
out.append('  return x; }')
out.append(r'''
#if defined(__x86_64__)
__attribute__((force_align_arg_pointer))
void _start(void) { volatile u r = run(); (void)r;
  __asm__ volatile("mov $231, %%eax; xor %%edi, %%edi; syscall" ::: "memory"); for (;;); }
#elif defined(__aarch64__)
void _start(void) { volatile u r = run(); (void)r;
  __asm__ volatile("mov x8, #94; mov x0, #0; svc #0" ::: "memory"); for (;;); }
#elif defined(__arm__)
void _start(void) { volatile u r = run(); (void)r;
  __asm__ volatile("mov r7, #248; mov r0, #0; swi #0" ::: "memory"); for (;;); }
#elif defined(__riscv)
void _start(void) { volatile u r = run(); (void)r;
  __asm__ volatile("li a7, 94; li a0, 0; ecall" ::: "memory"); for (;;); }
#elif defined(__mips__)
void _start(void) { volatile u r = run(); (void)r;
  __asm__ volatile("li $2, 4246; move $4, $0; syscall" ::: "memory"); for (;;); }
#endif
''')
print('\n'.join(out))
