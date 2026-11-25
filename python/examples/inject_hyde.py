#!/usr/bin/env python3
'''
inject_hyde.py: make a running program run system calls for the host, HyDE style.

Run: panda-python python/examples/inject_hyde.py <dir>/debian-13-x86_64.json

The argument is an image description written by pandare2.mkimage (tests/README.md says how to
build images); any Linux guest with a shell on its serial console works. Run it on a copy of the
image: reverting and running the guest writes to it.

The guest runs `sleep 3`. When the sleep enters its nanosleep, the entry callback holds the call
and has the sleep run a sequence of system calls first: getpid, an anonymous mapping, openat and
read of /etc/hostname, write() of what it read to its own stdout, close. The host gets every
value back in the generator. Then the sleep's nanosleep runs as if nothing had happened (its
entry is delivered again, flagged resumed), and the sleep exits normally; its output shows the
bytes the injected write() wrote.

Each `yield syscall(...)` is one system call the task makes: the generator gets its value (a
negative errno on failure). `ctx.alloc`, `ctx.write` and `ctx.read` use the task's own memory
with its own permissions, paging it in with an injected madvise when needed.
'''
from sys import argv
from pandare2 import Panda
from pandare2.inject import syscall

panda = Panda(generic=argv[1])
SECONDS = 3
results = {}


def hyde(ctx):
    '''What the sleep does for us before its nanosleep'''
    results["pid"] = yield syscall("getpid")
    buf = yield from ctx.alloc(4096)            # unmapped again when the generator ends
    yield from ctx.write(buf, b"/etc/hostname\0")
    fd = yield syscall("openat", -100, buf, 0, check=True)     # AT_FDCWD, O_RDONLY
    n = yield syscall("read", fd, buf + 256, 200)
    results["read"] = yield from ctx.read(buf + 256, max(n, 0))
    results["write"] = yield syscall("write", 1, buf + 256, max(n, 0))
    yield syscall("close", fd)
    return n


def is_the_sleep(cpu, callno):
    '''A nanosleep (or clock_nanosleep) of SECONDS seconds: the shell's `sleep`, not another'''
    abi = panda.inject.abi(cpu)
    from pandare2.inject_numbers import NUMBERS
    for name, i in (("nanosleep", 1), ("clock_nanosleep", 3)):
        if abi and NUMBERS[abi].get(name, (None,))[0] == callno:
            ts = panda.arch.get_arg(cpu, i, convention="syscall")
            try:
                sec = panda.virtual_memory_read(cpu, ts, 4)
            except ValueError:
                return False
            return SECONDS in (int.from_bytes(sec, "little"), int.from_bytes(sec, "big"))
    return False


@panda.ppp("syscalls2", "on_all_sys_enter")
def sys_enter(cpu, pc, callno):
    if "session" in results or not is_the_sleep(cpu, callno):
        return
    if panda.inject.resumed(cpu):
        print("the held nanosleep goes on")
        return
    s = panda.inject(hyde, cpu=cpu, before_syscall=True)
    s.done(lambda n, err: print(f"injected: pid {results.get('pid')}, read {n} bytes "
                                f"{results.get('read')!r}, wrote {results.get('write')}; "
                                f"error {err!r}"))
    results["session"] = s


@panda.queue_blocking
def run_cmds():
    panda.revert_sync("root")
    print(panda.run_serial_cmd(f"sleep {SECONDS}; echo sleep exited $?"))
    print(panda.run_serial_cmd("cat /etc/hostname"))
    panda.end_analysis()


panda.run()
print("engine:", panda.inject.stats())
