"""serial_check.py <qemu-system-x86_64>: the engine's host serial path (tty_serial_init calls
cfsetispeed/cfsetospeed, which the wheel's engine gets from nix/glibc-compat) sets a host tty's
speed. A pty starts at 9600 baud; QEMU opens it as -chardev serial and sets 115200."""
import os, pty, subprocess, sys, termios, time
master, slave = pty.openpty()
name = os.ttyname(slave)
a = termios.tcgetattr(slave)
a[4] = a[5] = termios.B9600
termios.tcsetattr(slave, termios.TCSANOW, a)
before = termios.tcgetattr(slave)
p = subprocess.Popen([sys.argv[1], "-M", "none", "-display", "none", "-monitor", "none", "-S",
                      "-chardev", f"serial,id=s0,path={name}"], stderr=subprocess.PIPE)
speed = None
for _ in range(100):
    time.sleep(0.1)
    now = termios.tcgetattr(slave)
    if now[4] != before[4] or p.poll() is not None:
        break
speed = (now[4], now[5])
alive = p.poll() is None
p.kill()
err = p.communicate()[1].decode()[-500:]
ok = speed == (termios.B115200, termios.B115200)
print(f"SERIAL {'PASS' if ok else 'FAIL'}: {name} ispeed/ospeed 9600 -> "
      f"{'115200' if ok else speed} (qemu {'running' if alive else 'exited'}) {err.strip()}")
sys.exit(0 if ok else 1)
