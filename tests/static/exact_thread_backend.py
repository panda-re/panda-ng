"""Compile actual exact native adapter with synchronized-hook/OSI stubs; no guest.
Run: panda-python tests/static/exact_thread_backend.py
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
HARNESS = r'''
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <climits>
#include <functional>
#include <vector>
#include <map>
#include <optional>
#include <mutex>
#include <cstring>
#define TARGET_X86_64 1
#define R_EDI 7
#define R_EAX 0
#define PROFILE_LINUX_X64 0
using target_ptr_t = uint64_t;
using target_pid_t = int32_t;
struct CPUArchState { uint64_t regs[16]; };
struct CPUState { CPUArchState env; };
struct syscall_ctx_t { int no; uint64_t retaddr; bool double_return; uint64_t args[6]; };
#include "panda/syscall-observer.h"
static int delivered;
static uint64_t delivered_pc;
static void syscall_enter_switch_linux_x64(CPUState *, int, uint64_t, int) {}
static void syscall_return_switch_linux_x64(CPUState *, uint64_t pc, syscall_ctx_t *) {
    delivered++; delivered_pc = pc;
}
static std::vector<std::function<void(CPUState *,uint64_t)>> preExecCallbacks;
static void *panda_cpu_env(CPUState *cpu) { return &cpu->env; }
static uint64_t active_asid = 900;
static uint64_t get_id(CPUState *) { return active_asid; }
#include "plugins/syscalls2/native_observer.h"
static OsiThreadIdentity current_id;
static std::map<uint64_t,OsiThreadIdentity> identities;
static bool current(CPUState *,OsiThreadIdentity *out) { *out=current_id; return out->lifetime_valid; }
static bool by_task(CPUState *,uint64_t task,OsiThreadIdentity *out) {
    if (!identities.count(task)) return false; *out=identities.at(task); return true;
}
struct H { ExactHookCallback cb; void *opaque; uint64_t pc; };
static std::map<uint64_t,H> hooks;
static uint64_t next_hook;
static uint64_t insert(uint64_t pc,ExactHookCallback cb,void *opaque) {
    hooks[++next_hook] = {cb,opaque,pc}; return next_hook;
}
static bool remove_hook(uint64_t id) { return hooks.erase(id); }
static void fire(CPUState *cpu,uint64_t id) {
    auto h=hooks.at(id); assert(h.cb(cpu,h.pc,h.opaque)==0);
}
static OsiThreadIdentity id(uint64_t task,uint64_t life,uint64_t asid) {
    OsiThreadIdentity i={}; i.task=task;i.lifetime=life;i.lifetime_valid=true;
    i.lifetime_format=OSI_THREAD_LIFETIME_START_TIME_U64;i.mm_valid=i.asid_valid=true;
    i.asid=asid; return i;
}
int main() {
    CPUState cpu={}; exact_backend=true; exact_current=current;exact_by_task=by_task;
    exact_hook=insert;exact_unhook=remove_hook;
    auto parent=id(100,10,800), child=id(200,20,900), unrelated=id(300,30,900);
    identities[200]=child;
    syscall_ctx_t ctx={57,4096,true,{1,2,3,4,5,6}};
    PandaSyscallObservation event={};event.pc=4096;
    current_id=parent;
    auto *cookie=exact_capture(&cpu,&ctx); assert(cookie);
    cpu.env.regs[R_EDI]=200;
    exact_birth(&cpu,0,nullptr);
    assert(hooks.size()==1);
    auto handle=hooks.begin()->first;
    current_id=unrelated;fire(&cpu,handle);assert(!delivered);
    current_id=child;
    assert(!native_accept_return(&cpu,&event,cookie,nullptr)); // vfork mismatch retained
    active_asid=901;fire(&cpu,handle);assert(!delivered);
    active_asid=900;cpu.env.regs[R_EAX]=0;fire(&cpu,handle);
    assert(delivered==1 && delivered_pc==4096 && hooks.empty());
    current_id=parent;
    assert(native_accept_return(&cpu,&event,cookie,nullptr));
    native_return(&cpu,&event,cookie,nullptr);native_drop(cookie,nullptr);
    assert(delivered==2 && exact_records.empty());
    cookie=exact_capture(&cpu,&ctx);assert(cookie);
    current_id=child;assert(!native_accept_return(&cpu,&event,cookie,nullptr));
    exact_drop(cookie);assert(exact_records.empty()); // no observed birth => no child
    current_id=parent;cookie=exact_capture(&cpu,&ctx);
    exact_birth(&cpu,0,nullptr);assert(!hooks.empty());
    uint64_t epoch=exact_epoch;exact_reset();assert(exact_epoch==epoch+1 && hooks.empty());
    native_return(&cpu,&event,cookie,nullptr);native_drop(cookie,nullptr);
    assert(delivered==2); // stale cookie after restore cannot deliver
    cookie=exact_capture(&cpu,&ctx);exact_birth(&cpu,0,nullptr);
    current_id=child;exact_invalidate(&cpu,0,nullptr);
    assert(hooks.empty() && exact_records.size()==1);
    current_id=parent;native_return(&cpu,&event,cookie,nullptr);native_drop(cookie,nullptr);
    assert(delivered==3 && exact_records.empty());
    current_id=parent;cookie=exact_capture(&cpu,&ctx);
    event.restart=true;native_return(&cpu,&event,cookie,nullptr);native_drop(cookie,nullptr);
    assert(delivered==3 && exact_records.empty());
    // Parent-first keeps child independently deliverable after core cookie drop.
    event.restart=false;
    cookie=exact_capture(&cpu,&ctx);exact_birth(&cpu,0,nullptr);
    handle=hooks.begin()->first;
    native_return(&cpu,&event,cookie,nullptr);native_drop(cookie,nullptr);
    assert(delivered==4 && !hooks.empty());
    current_id=child;fire(&cpu,handle);
    assert(delivered==5 && hooks.empty() && exact_records.empty());
    current_id=parent;cookie=exact_capture(&cpu,&ctx);exact_birth(&cpu,0,nullptr);
    handle=hooks.begin()->first;
    exact_invalidate(&cpu,0,nullptr);native_drop(cookie,nullptr);
    assert(!hooks.empty()); // parent death preserves observed child
    current_id=child;fire(&cpu,handle);
    assert(delivered==6 && exact_records.empty());
    current_id=parent;
    // Ambiguous creator calls cannot attach a child based on newest/retpc.
    event.restart=false;
    auto *a=exact_capture(&cpu,&ctx), *b=exact_capture(&cpu,&ctx);
    exact_birth(&cpu,0,nullptr);assert(hooks.empty());
    exact_drop(a);exact_drop(b);
    current_id.lifetime_valid=false;
    Syscalls2NativeFrame frame={&event,nullptr};native_frame=&frame;
    assert(syscalls2_capture_native_context(&cpu,&ctx) && !frame.cookie);
    native_frame=nullptr;assert(exact_records.empty());
    return 0;
}
'''


class ExactThreadBackend(unittest.TestCase):
    def test_sdk_cpu_lookup(self):
        source = (ROOT / 'plugins/syscalls2/syscalls2.cpp').read_text()
        readiness = source[source.index('    if (use_exact) {'):source.index('    if (use_native) {')]
        self.assertIn('CPUState *cpu = qemu_get_cpu(0);', readiness)
        self.assertNotIn('first_cpu', readiness)
        self.assertIn('!cpu || !exact_current(cpu, &probe)', readiness)

    def test_native_adapter(self):
        with tempfile.TemporaryDirectory(prefix='exact-thread-') as tmp:
            tmp = Path(tmp)
            # Real OSI value declarations, with the gmodule include dependency
            # avoided: this tiny harness does not link the OSI plugin.
            types = (ROOT / 'plugins/osi/osi_types.h').read_text()
            start = types.index('typedef enum {\n    OSI_THREAD_LIFETIME')
            end = types.index('} OsiThreadIdentity;', start) + len('} OsiThreadIdentity;')
            header = tmp / 'osi/osi_types.h'
            header.parent.mkdir()
            header.write_text(types[start:end])
            source = tmp / 'adapter.cpp'
            # Exercise both external raw-header consumers and the plugin's
            # flattened panda.h shape (declarations already present, no macros,
            # and no raw observer header available to include a second time).
            observer = (ROOT / 'engine/src/include/panda/syscall-observer.h').read_text()
            first = observer.index('typedef struct PandaSyscallObservation {')
            last = observer.index('} PandaSyscallObserverOps;', first) + len('} PandaSyscallObserverOps;')
            flattened = HARNESS.replace('#include "panda/syscall-observer.h"', observer[first:last])
            for abi, harness in [('raw', HARNESS), ('flattened', flattened)]:
                with self.subTest(abi=abi):
                    source.write_text(harness)
                    executable = tmp / ('adapter-' + abi)
                    includes = ['-I', str(tmp), '-I', str(ROOT)]
                    if abi == 'raw':
                        includes += ['-I', str(ROOT / 'engine/src/include')]
                    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17', '-O2'] +
                                   includes + [str(source), '-o', str(executable)], check=True)
                    subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
