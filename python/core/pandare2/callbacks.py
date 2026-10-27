"""
Which PANDA callbacks panda-ng calls.

Every member of the engine's `panda_cb` (engine/src/include/panda/callbacks/cb-defs.h) gets a
`@panda.cb_<name>` decorator, but not every one is called in panda-ng. For the ones here the
decorator raises NotImplementedError, saying why:

- `ABORTS`: registering one aborts the engine (it has no trampoline). `Panda.register_callback`
  refuses these too.
- `NO_CALL_SITE`: the engine never calls it. `Panda.register_callback` still takes one, so
  tests/callbacks.py can check that it stays silent: that test registers every callback on a
  real guest and fails when one listed here starts firing, which keeps these sets true.

docs/reference/callbacks.md is generated from these sets.
"""

ABORTS = {"before_loadvm", "replay_hd_transfer", "replay_net_transfer", "replay_serial_receive",
          "replay_serial_read", "replay_serial_send", "replay_serial_write", "replay_before_dma",
          "replay_after_dma", "replay_handle_packet"}

NO_CALL_SITE = {
    "before_tcg_codegen": "no call site", "after_insn_translate": "no call site",
    "after_insn_exec": "after_insn_translate, which selects its instructions, has none",
    "hd_read": "no call site", "hd_write": "no call site", "monitor": "no call site", "qmp": "no call site",
    "cpu_restore_state": "no call site", "after_cpu_exec_enter": "commented out in wrap_ops.c",
    "before_cpu_exec_exit": "commented out in wrap_ops.c", "top_loop": "no call site",
    "during_machine_init": "no call site", "pre_shutdown": "no call site",
    "unassigned_io_read": "no call site", "unassigned_io_write": "no call site",
    "before_handle_interrupt": "the cpu_exec_interrupt wrapper is commented out in wrap_ops.c",
}

# The callbacks that fire under KVM (Panda(accel="kvm")): the ones the main loop and snapshots
# call. Every other callback that fires is called from TCG, while it translates or runs guest
# code: the block, instruction and memory callbacks, asid_changed (from the TCG helpers that write
# CR3/TTBR/satp/EntryHi), before_handle_exception (TCG's cpu loop) and guest_hypercall (the TCG
# hypercall helpers and the PC's port 0x88, which KVM's exit handler serves itself: patches 0009,
# 0023 and 0045 hand a KVM hypercall to penguin's consumers only). Under KVM, registering one of
# those raises NotImplementedError.
KVM = {"main_loop_wait", "after_machine_init", "after_loadvm"}

# Callbacks that return the index they were given to leave it alone: after a Python exception
# their wrapper returns that, not 0 (0 is a different exception, which QEMU may abort on).
PASSTHROUGH = {"before_handle_exception", "before_handle_interrupt"}


def unavailable(name):
    """Why panda-ng can't run callback `name`, or None if it can."""
    if name in ABORTS:
        return "registering it aborts the engine (panda-ng has no trampoline for it)"
    if name in NO_CALL_SITE:
        why = NO_CALL_SITE[name]
        return "panda-ng never calls it" + ("" if why == "no call site" else f" ({why})")
    return None


def tcg_only(name):
    """Whether callback `name` fires only under TCG (see KVM)."""
    return name not in KVM
