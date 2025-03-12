from pandare2 import Panda
from rich import print
from sys import argv

arch = argv[1] if len(argv) > 1 else "arm"
panda = Panda(generic=arch)

# root snapshots aren't compatible at this point so you'll have to wait for startup
# @panda.queue_blocking
def do_stuff():
    panda.revert_sync("root")
    for command in ["ls -la", "whoami", "sleep 1", "uname -r"]:
        print("Output start:")
        print(panda.run_serial_cmd("LD_SHOW_AUXV=1 "+command,no_timeout=True))
        print("Output end")
    panda.end_analysis()

@panda.ppp("syscalls2","on_all_sys_enter")
def sys_enter(cpu, pc, *a):
    print(a)

panda.run()