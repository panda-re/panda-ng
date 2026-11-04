"""The host side of the sctrace plugin (tests/plugins/sctrace): load it into a Panda, start and stop
recording the syscall trap's events, read them. Used by tests/syscall_cover.py and
tests/syscall_truth.py.

sctrace observes the trap through the QEMU plugin syscall API, the interface the engine has for it
today; a test that only uses this module's Event list can move to another observer (the engine's
syscall.enter and syscall.return events) by swapping this module.

The plugin comes from PANDA_PLUGIN_DIR (`nix build .#test-plugins`, lib/panda/plugins) or the
Panda's plugin path: libpanda-sctrace_<arch>-softmmu.so.
"""
import os
from collections import namedtuple

from cffi import FFI

# enum qemu_plugin_syscall_abi
ABI_X86_64, ABI_I386, ABI_AARCH64, ABI_ARM, ABI_ARM_OABI, ABI_MIPS, ABI_PPC64, ABI_PPC, \
    ABI_RISCV64, ABI_RISCV32, ABI_LOONGARCH64 = range(11)
ABI_NAMES = ["x86_64", "i386", "aarch64", "arm", "arm-oabi", "mips", "ppc64", "ppc", "riscv64",
             "riscv32", "loongarch64"]
ENTER, RETURN = 1, 2
M64 = (1 << 64) - 1

# kind: ENTER or RETURN; id: the call's (an enter's, given back at its return); restart: a return
# the kernel is restarting
Event = namedtuple("Event", "kind abi cpu restart id num args pc ret")

_CDEF = """
typedef struct { uint32_t kind; int32_t abi; int32_t cpu; uint32_t flags; uint64_t id; int64_t num;
                 uint64_t args[6]; uint64_t pc; int64_t ret; } SctraceEvent;
int sctrace_start(uint64_t lo, uint64_t hi, uint64_t cap, unsigned abis);
void sctrace_stop(void);
uint64_t sctrace_count(void);
uint64_t sctrace_lost(void);
uint64_t sctrace_read(uint64_t first, uint64_t n, SctraceEvent *out);
void sctrace_stats(uint64_t *pending, uint64_t *full, uint64_t *stale);
"""


def plugin_path(arch, plugin_dir=None):
    d = plugin_dir or os.environ.get("PANDA_PLUGIN_DIR")
    if not d:
        from pandare2.panda import _PLUGIN_DIR
        d = _PLUGIN_DIR
    so = os.path.join(d, f"libpanda-sctrace_{arch}-softmmu.so")
    if not os.path.isfile(so):
        raise FileNotFoundError(f"{so}: build the test plugins (nix build .#test-plugins) and set "
                                "PANDA_PLUGIN_DIR to its lib/panda/plugins")
    return so


def extra_args(arch):
    """The Panda arguments that load sctrace (QEMU's -plugin)."""
    return ["-plugin", plugin_path(arch)]


class Sctrace:
    """sctrace in a Panda that was given extra_args(arch). The calls are made from a
    queue_blocking function."""

    def __init__(self, arch):
        self.ffi = FFI()
        self.ffi.cdef(_CDEF)
        self.lib = self.ffi.dlopen(plugin_path(arch))

    def start(self, lo=0, hi=M64, cap=1 << 18, abis=(1 << 11) - 1):
        """Record the calls with pc in [lo, hi) of the ABIs in abis (a bit each), up to cap
        events."""
        rc = self.lib.sctrace_start(lo, hi, cap, abis)
        if rc:
            raise RuntimeError(f"sctrace_start: {rc} (-1: the plugin doesn't hold the syscall "
                               "callbacks; another plugin has them)")

    def stop(self):
        """Stop; ([Event], lost): the events, and how many didn't fit."""
        self.lib.sctrace_stop()
        n = self.lib.sctrace_count()
        buf = self.ffi.new("SctraceEvent[]", max(n, 1))
        self.lib.sctrace_read(0, n, buf)
        events = [Event(e.kind, e.abi, e.cpu, e.flags & 1, e.id, e.num, list(e.args), e.pc, e.ret)
                  for e in buf[0:n]]
        return events, self.lib.sctrace_lost()

    def stats(self):
        """The trap's (pending, full, stale) counters."""
        p, f, s = (self.ffi.new("uint64_t *") for _ in range(3))
        self.lib.sctrace_stats(p, f, s)
        return p[0], f[0], s[0]
