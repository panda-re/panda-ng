# syscalls: syscalls2 compatibility

A plugin written for syscalls2 moves to `syscalls` by changing the plugin name in `PPP_REG_CB`:
`syscalls` declares the same PPP callbacks, with the same names and C signatures.

| File | What |
|---|---|
| `gen_compat.py` | writes `generated/` from the syscalls tables (`../tables`) |
| `generated/syscalls_ext_typedefs_<arch>.h` | the per-syscall callback typedefs, per syscalls2 arch |
| `generated/syscall_ppp_{extern,boilerplate,register}_{enter,return}` | the PPP declarations, definitions and registrations |

The generated files are committed. `python3 plugins/syscalls/compat/gen_compat.py` regenerates
them after the tables change; `../tables/regen.sh` and `nix run .#regen-syscalls` run it too.

A callback's parameters are the compat fields of the tables' arguments: syscalls2's types,
including its widths (`size_t` is a `uint32_t` on every ABI), and its parameter names. Where the
profiles of one syscalls2 arch give a name two signatures (FreeBSD's `sigaction`, `getsockname`,
`mknodat`; FreeBSD's and Linux's `sys_exit` on x64), the one syscalls2 declares is used: within a
profile the last, across profiles the one whose OS sorts last.

The flake check `syscalls-compat-test` (`tests/static/syscalls_compat.py`) checks that every
generated file equals syscalls2's `generated/` file of the same name, byte for byte, and
`syscalls-tables` that the committed files are current.

Not yet generated: callbacks for the ABIs syscalls2 has no profile for (riscv32, loongarch64,
x32, ARM OABI, MIPS n64 as its own ABI), the dispatch from `syscall.enter` and `syscall.return`
to these callbacks, `get_syscall_info`, `get_syscall_meta`, `get_syscall_retval`, and the
`syscalls2_compat.h` header that includes them.
