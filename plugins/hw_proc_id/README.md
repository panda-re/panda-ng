Plugin: `HW_PROC_ID`
===========

Summary
-------
This plugin aims to provide an architecture-agnostic way to uniquely identify processes through a single API.

For non-MIPS architectures, this plugin is equivalent to using the `panda_current_asid()` function. However, on MIPS guests, the ASID changes frequently for the same process so a different implementation is needed. For these guests, we return the address of the `current` `task_struct` object which will be different for different processes. But, (like with ASIDs) after a process is terminated, another process could end up with it's `task_struct` object in the same location.


Arguments
---------
None

Dependencies
------------
None

APIs and Callbacks
------------------

`target_ulong get_id(CPUState *cpu)`: Returns an integer that represents the current process running on the CPU.

`bool id_is_initialized(void)`: Whether `get_id` can answer yet (always true except on MIPS, where the kernel's r28, which points at the current task, has to be seen in kernel mode first).

Example
-------
