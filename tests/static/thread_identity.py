"""Extract and compile exact-thread identity queries with checked-memory stubs.
Run: panda-python tests/static/thread_identity.py (uses CXX, default c++).
No engine build, guest, profile database or native bindings needed.
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(text, name):
    match = re.search(r'^(?:static )?(?:bool|void) ' + name + r'\([^;{}]*\)\s*\{', text, re.M)
    if not match:
        raise AssertionError(name)
    depth = 0
    for token in re.finditer(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|[{}]',
                             text[match.end() - 1:], re.S):
        if token.group() == '{':
            depth += 1
        elif token.group() == '}':
            depth -= 1
            if not depth:
                return text[match.start():match.end() + token.end() - 1] + '\n'
    raise AssertionError('unclosed ' + name)


STUBS = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <map>
using target_ptr_t = uint64_t;
using target_pid_t = int32_t;
struct CPUState {};
static std::map<uint64_t, uint64_t> memory;
static uint64_t fail_at, translated = 0x9000;
static int reads;
static bool read_uint(CPUState *, uint64_t addr, size_t, uint64_t *out) {
    reads++;
    if (addr == fail_at || !memory.count(addr)) return false;
    *out = memory[addr]; return true;
}
static int panda_virtual_memory_read(CPUState *cpu, uint64_t addr, uint8_t *p, size_t n) {
    uint64_t value;
    if (!read_uint(cpu, addr, n, &value)) return 1; /* non--1 errors must fail too */
    memcpy(p, &value, n); return 0;
}
static target_ptr_t panda_virt_to_phys(CPUState *, uint64_t) { return translated; }
#define flipbadendian(x) (x)
static bool is_kernel_ptr(uint64_t p) { return p >= 0x1000; }
struct Field { std::string kind = "base", type_name = "unsigned long long"; } field;
static bool have_field = true;
static struct {
    int pointer_size = 8;
    const Field *field(const char *, const char *) { return have_field ? &::field : nullptr; }
} prof;
static struct {
    int task_pid = 4, task_tgid = 8, task_start_time = 16, task_mm = 24, mm_pgd = 8;
} off;
static struct {
    struct { int a = 6, b = 1; } version;
    struct { int pid_offset = 4, tgid_offset = 8, start_time_offset = 16,
                 mm_offset = 24; size_t size = 64; } task;
    struct { int pgd_offset = 8; } mm;
} ki;
static uint64_t current_task(CPUState *) { return 0x1000; }
static struct { target_ptr_t (*get_current_task_struct)(CPUState *) = current_task; } profile;
static auto *kernel_profile = &profile;
static bool provider_present = true;
#define PPP_RUN_CB(name, ...) do { if (provider_present) name(__VA_ARGS__); } while (0)
'''

CHECKS = r'''
int main() {
    CPUState cpu;
    OsiThreadIdentity id;
    memory = {{0x1004, 17}, {0x1008, 10}, {0x1010, 0}, {0x1018, 0x2000},
              {0x2008, 0x3000}};
    assert(get_thread_identity(&cpu, 0x1000, &id));
    assert(id.task == 0x1000 && id.tid == 17 && id.tgid == 10);
    assert(id.lifetime_valid && id.lifetime == 0);
    assert(id.lifetime_format == OSI_THREAD_LIFETIME_START_TIME_U64);
    assert(id.mm_valid && id.mm == 0x2000 && id.asid_valid && id.asid == 0x9000);
    assert(get_current_thread_identity(&cpu, &id) && id.task == 0x1000 && id.tid == 17);
    assert(!get_current_thread_identity(nullptr, &id) && !id.task);
    /* Changing/reusing a task descriptor changes the discriminator, not taskd. */
    memory[0x1010] = 123;
    assert(get_thread_identity(&cpu, 0x1000, &id) && id.lifetime == 123);
    for (uint64_t addr : {0x1004, 0x1008, 0x1010}) {
        fail_at = addr;
        memset(&id, 0xff, sizeof(id));
        assert(!get_thread_identity(&cpu, 0x1000, &id));
        assert(!id.task && !id.lifetime_valid && !id.mm_valid && !id.asid_valid);
    }
    fail_at = 0x1018;
    assert(get_thread_identity(&cpu, 0x1000, &id));
    assert(!id.mm_valid && !id.asid_valid && id.lifetime_valid);
    fail_at = 0x2008;
    assert(get_thread_identity(&cpu, 0x1000, &id));
    assert(id.mm_valid && !id.asid_valid);
    fail_at = 0;
    translated = (target_ptr_t)-1;
    assert(get_thread_identity(&cpu, 0x1000, &id) && !id.asid_valid && !id.asid);
    translated = 0; /* physical page zero is not an unavailable sentinel */
    assert(get_thread_identity(&cpu, 0x1000, &id) && id.asid_valid && !id.asid);
    memory[0x1018] = 0;
    assert(get_thread_identity(&cpu, 0x1000, &id) && id.mm_valid && !id.mm && !id.asid_valid);
    provider_present = false;
    memset(&id, 0xff, sizeof(id));
    assert(!get_thread_identity(&cpu, 0x1000, &id) && !id.task);
    assert(!get_current_thread_identity(&cpu, &id) && !id.task);
    assert(!get_thread_identity(&cpu, 0x1000, nullptr));
    provider_present = true;
    reads = 0;
    assert(!get_thread_identity(&cpu, 0, &id) && reads == 0);
#ifdef ISF
    for (const char *kind : {"struct", "union", "typedef"}) {
        field.kind = kind;
        assert(!get_thread_identity(&cpu, 0x1000, &id));
    }
    field.kind = "base"; field.type_name = "unknown_u64";
    assert(!get_thread_identity(&cpu, 0x1000, &id));
    field.type_name = "unsigned long"; prof.pointer_size = 4;
    assert(!get_thread_identity(&cpu, 0x1000, &id));
    prof.pointer_size = 8;
    assert(get_thread_identity(&cpu, 0x1000, &id));
    have_field = false;
    assert(!get_thread_identity(&cpu, 0x1000, &id));
#else
    ki.version.a = 3; ki.version.b = 16;
    assert(!get_thread_identity(&cpu, 0x1000, &id));
    ki.version.b = 17;
    assert(get_thread_identity(&cpu, 0x1000, &id));
    ki.task.start_time_offset = 0;
    assert(!get_thread_identity(&cpu, 0x1000, &id));
#endif
    return 0;
}
'''


class ThreadIdentity(unittest.TestCase):
    def test_checked_provider_queries(self):
        types = (ROOT / 'plugins/osi/osi_types.h').read_text()
        identity = types[types.index('typedef enum {\n    OSI_THREAD_LIFETIME'):]
        identity = identity[:identity.index('} OsiThreadIdentity;') + len('} OsiThreadIdentity;')]
        frontend = (ROOT / 'plugins/osi/os_intro.c').read_text()
        for provider in ('osi_linux', 'osi_isf'):
            with self.subTest(provider=provider), tempfile.TemporaryDirectory() as tmp:
                text = (ROOT / f'plugins/{provider}/{provider}.cpp').read_text()
                names = ['read_thread_identity', 'on_get_thread_identity',
                         'on_get_current_thread_identity']
                if provider == 'osi_isf':
                    names.insert(0, 'identity_start_time_u64')
                source = STUBS + identity + '\n' + ''.join(function(text, n) for n in names)
                source += ''.join(function(frontend, n) for n in
                                  ('get_current_thread_identity', 'get_thread_identity'))
                source += CHECKS
                path = Path(tmp) / 'identity.cpp'
                path.write_text(source)
                exe = Path(tmp) / 'identity'
                command = shlex.split(os.environ.get('CXX', 'c++'))
                flags = ['-DISF'] if provider == 'osi_isf' else []
                subprocess.run(command + ['-std=c++17', '-O2'] + flags +
                               [str(path), '-o', str(exe)], check=True)
                subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
