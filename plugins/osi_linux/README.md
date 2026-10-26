Plugin: osi_linux
===========

Summary
-------

`osi_linux` provides Linux introspection information and makes it available through the OSI interface. It does so by knowing about the offsets for various Linux kernel data structures and then providing algorithms that traverse these data structures in the guest virtual machine.

Because the offsets of fields in Linux kernel data structures change frequently (and can even depend on the specific compilation flags used), `osi_linux` uses a configuration file to specify the offsets of critical data structures. A portion of such a configuration file, which is in the [GLib key-value format](https://docs.gtk.org/glib/struct.KeyFile.html) (similar to .ini files), is given below:

    [ubuntu:4.4.0-98-generic:32]
    name = 4.4.0-98-generic|#121-Ubuntu SMP Tue Oct 10 14:23:20 UTC 2017|i686
    version.a = 4
    version.b = 4
    version.c = 90
    task.init_addr = 3249445504
    #task.init_addr = 0xC1AE9A80
    #task.per_cpu_offset_0 = 0x34B42000
    task.per_cpu_offset_0 = 884219904
    #task.current_task_addr = 0xC1C852A8
    task.current_task_addr = 3251131048
    task.size = 5824
    task.tasks_offset = 624
    task.pid_offset = 776

    [... omitted ...]

    [debian:4.9.0-6-686-pae:32]
    name = 4.9.0-6-686-pae|#1 SMP Debian 4.9.82-1+deb9u3 (2018-03-02)|i686
    version.a = 4
    version.b = 9
    version.c = 88
    task.init_addr = 3245807232
    #task.init_addr = 0xC1771680
    #task.per_cpu_offset_0 = 0x36127000
    task.per_cpu_offset_0 = 907177984
    #task.current_task_addr = 0xC18C3208
    task.current_task_addr = 3247190536
    task.size = 5888
    task.tasks_offset = 708
    task.pid_offset = 864

    [... omitted ...]

Of course, generating this file by hand would be extremely painful. For the images it builds, `pandare2.mkimage` writes one (`<name>.kernelinfo.conf`, named in the image's json as `kconf_file`) from the kernel's debug symbols with the gdb extractor in `utils/kernelinfo_gdb/`; see the repository's `tests/README.md`. For other guests, `utils/README.md` describes both ways to make one. The rest of this section is the other way: building and loading a kernel module in the guest OS.  If available, it is important that Address Space Layout Randomization (ASLR) be disabled in the guest before the kernel information configuration file is generated.

To generate the kernel configuration file, you will need the kernel headers and a compiler installed in the guest. On a Debian guest, you can do:

```sh
    apt-get install build-essential linux-headers-`uname -r`
```

Then copy this plugin's `utils/kernelinfo` directory into the guest (e.g., using `scp` from inside the guest or by cloning the repository), and run `make` to build `kernelinfo.ko`. Finally, insert the kernel module and run `dmesg` to get the values. Note that although `insmod` will return an "Operation not permitted" error, it will still print the right information to the log:

    # insmod kernelinfo.ko
    Error: could not insert module kernelinfo.ko: Operation not permitted
    # dmesg

You should see output in the `dmesg` log like:

    [  335.936312] --KERNELINFO-BEGIN--
    [  335.936352] name = 4.9.0-6-686-pae|#1 SMP Debian 4.9.82-1+deb9u3 (2018-03-02)|i686
    [  335.936371] version.a = 4
    [  335.936380] version.b = 9
    [  335.936389] version.c = 88
    [  335.936400] task.init_addr = 3245807232
    [  335.936415] #task.init_addr = 0xC1771680
    [  335.936425] #task.per_cpu_offset_0 = 0x36127000
    [  335.936435] task.per_cpu_offset_0 = 907177984
    [  335.936443] #task.current_task_addr = 0xC18C3208
    [  335.936452] task.current_task_addr = 3247190536
    [  335.936473] task.size = 5888
    [  335.936502] task.tasks_offset = 708
    [  335.936528] task.pid_offset = 864
    [  335.936553] task.tgid_offset = 868
    [  335.936578] task.group_leader_offset = 900
    [  335.936602] task.thread_group_offset = 956
    [  335.936627] task.real_parent_offset = 876
    [  335.936651] task.parent_offset = 880
    [  335.936676] task.mm_offset = 748
    [  335.936700] task.stack_offset = 12
    [  335.936724] task.real_cred_offset = 1092
    [  335.936748] task.cred_offset = 1096
    [  335.936772] task.comm_offset = 1100
    [...]
    [  335.937668] path.mnt_parent_offset = -8
    [  335.937694] path.mnt_mountpoint_offset = -4
    [  335.937701] ---KERNELINFO-END---

Copy this information (without the KERNELINFO-BEGIN and KERNELINFO-END lines) into the `kernelinfo.conf`. Be sure to put it in its own configuration section, i.e.:

    [my_kernel_info]
    name = #1 SMP Debian 3.2.51-1 i686
    version.a = 3
    [...]

The name you give (`my_kernel_info` in this case) should then be passed as the `kconf_group` argument to the plugin.

Task change notifications are an optional feature provided by `osi_linux`. To enable it, you must extract an address that is executed during a context switch from from the `System.map` file for your kernel. To do this, you can simply run the following command in your guest:

For Linux 2.6 and later:
```
grep finish_task_switch /boot/System.map-<Kernel Version>
```

For Linux 2.4.X variants:
```
grep __switch_to /boot/System.map-<Kernel Version>
```

Once you've obtained the address, you can add the task.switch_task_hook_addr field to your kernel config. For example:

    [my_kernel_info]
    name = #1 SMP Debian 3.2.51-1 i686
    version.a = 3
    [...]
    task.switch_task_hook_addr = <Address extracted from System.map>


When taking a recording to be used with `osi_linux`, ASLR must be disabled in the guest, if it is available.

Hypercall Profile
----------------

As an alternative to using kernel offsets for introspection, `osi_linux` also supports a hypercall-based approach for obtaining introspection information. This method uses a cooperative guest that exposes OS-level information through hypercalls, making it more reliable across different kernel versions and configurations.

To use the hypercall profile:

1. Enable the hypercall option when loading the plugin:
   ```python
   panda.load_plugin("osi_linux", {"hypercall": True})
   ```

2. Ensure your guest is configured to handle the appropriate hypercalls. This typically requires installing a custom module or kernel patch in the guest that can respond to the hypercall requests from PANDA.

The hypercall profile is especially useful in environments where:
- Traditional offset-based introspection is difficult due to kernel customizations
- You have control over the guest environment and can install the necessary hypercall handlers
- You need more reliable introspection across kernel updates

This is the only method which supports a multi-cpu environment

Arguments
---------

* `kconf_file`: string. The location of the configuration file that gives the required offsets for different versions of Linux. Without it, `osi_linux` looks for `<plugin dir>/osi_linux/kernelinfo.conf` and under `$PANDA_DIR`; neither the flake nor the wheel installs one, so pass it. This directory's `kernelinfo.conf` has groups for some older Ubuntu, Debian, CentOS and Buildroot kernels.
* `kconf_group`: string, defaults to `<os variant>:<bits>` from the guest OS name, e.g. `debian:6.12.113+deb13-amd64:64` for `linux-64-debian:6.12.113+deb13-amd64` (the group `pandare2.mkimage` writes). The specific configuration desired from the kernelinfo file (multiple configurations can be stored in a single `kernelinfo.conf`). If the file has no such group, `osi_linux` tries to download it from `https://panda-re.mit.edu/kernelinfos/<group>` into the file.
* `load_now`: bool, defaults to false. When set, we will raise a fatal error if OSI cannot be initialized immediately. Otherwise, the plugin will attempt to provide introspection immediately, but if that fails, it will wait until the first syscall. If OSI is still unavailable at the first syscall, a fatal error will always be raised.
* `pagewalk`: bool, defaults to false. When set, we will attempt to use pagewalk to resolve addresses. This is useful on embedded targets where TLB misses abound and your target page table configuration is supported.
* `hypercall`: bool, defaults to false. When set, we will use cooperative hypercalls for kernel introspection instead of relying on kernel offsets.

Dependencies
------------

`osi_linux` is an introspection provider for the `osi` plugin. It uses `syscalls2`, `hooks` and `proc_start_linux`, `hw_proc_id` on mips, and `hypercaller` with `hypercall`.

APIs and Callbacks
------------------

In addition to providing the standard APIs used by OSI, `osi_linux` provides Linux-specific API calls (`osi_linux_int_fns.h` has them all). Two resolve file descriptors to filenames and tell you the current file position:

```C
    // returns fd for a filename or a NULL if failed
    char *osi_linux_fd_to_filename(CPUState *env, OsiProc *p, int fd);

    // returns pos in a file
    unsigned long long osi_linux_fd_to_pos(CPUState *env, OsiProc *p, int fd);
```

From Python they are `panda.plugins["osi_linux"].osi_linux_fd_to_filename(cpu, proc, fd)`, and so on; `Panda.get_file_name(cpu, fd)` wraps the first for the current process.

Example
-------

On an image built by `pandare2.mkimage`, load `osi` without its autoload and give `osi_linux` the image's kernelinfo. From `tests/guest.py`:

```python
import json, os, sys
from pandare2 import Panda

panda = Panda(generic=sys.argv[1])       # <dir>/debian-13-x86_64.json
with open(sys.argv[1]) as f:
    kconf = os.path.join(os.path.dirname(os.path.abspath(sys.argv[1])), json.load(f)["kconf_file"])
panda.load_plugin("osi", {"disable-autoload": "true"})
panda.load_plugin("osi_linux", {"kconf_file": kconf})
```

The group defaults to the image's, so `kconf_group` isn't needed. `osi`'s README shows how to call the API.

KASLR and ASLR
--------------

Kernels that randomize their own layout (KASLR) need it turned off with the `nokaslr` boot parameter (`pandare2.mkimage` boots its images with it). It is often helpful to turn off ASLR for user programs as well. This can be done in the guest with
```
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
```
