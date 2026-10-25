#include <assert.h>
#include <stdio.h>
#include <stdbool.h>

#include "panda/plugin.h"
#include "panda/panda_api.h"
#include "panda/common.h"
#include "system/system.h"

// for map_memory
#include "qapi/error.h"
#include "system/replay.h"
#include "migration/vmstate.h"
#include "migration/snapshot.h"
#include "qemu/error-report.h"
#include "qemu/main-loop.h"
#include "system/memory.h"
#include "system/address-spaces.h"

// for panda_{set/get}_library_mode
#include "qemu/osdep.h"
#include "system/system.h"
#include "system/runstate.h"
#include "system/cpus.h"
#include "panda/callbacks/cb-support.h"
#include "panda/wrap_ops.h"
#include "accel/tcg/hooks.h"
#include "hw/core/panda-modules.h"

void panda_mark_hook_address(target_ulong pc)
{
    tcg_hooks_mark_pc(pc);
}

// call main_aux and run everything up to and including panda_callbacks_after_machine_init
int panda_init(int argc, char **argv, char **envp) {
    qemu_init(argc, argv);
    wrap_cpu_ops();
    panda_callbacks_after_machine_init(first_cpu);
    return 0;
}

// extern void pandalog_cc_init_write(const char * fname);
int panda_in_main_loop;

// vl.c
extern char *panda_snap_name;
// extern bool panda_aborted;

int panda_run(void) {
    int status = 0;
    panda_in_main_loop = 1;
    status = qemu_main_loop();
    panda_in_main_loop = 0;
    qemu_cleanup(status);
    return status;
}

void panda_stop(int code) {
    // default of 4 = run_state_paused
    if (qemu_in_vcpu_thread() || bql_locked()) {
        vm_stop(code);
    } else {
        /* any other thread (a pandare2 blocking function): ask the main loop */
        qemu_system_vmstop_request_prepare();
        qemu_system_vmstop_request(code);
    }
}

void panda_cont(void) {
    vm_start();
}

void panda_main_loop_kick(void) {
    qemu_notify_event();
}

/*
 * Snapshots of the running VM, from the main loop (pandare2 queues these on main_loop_wait).
 * savevm/loadvm stop the VM themselves and leave it stopped; panda_cont() resumes.
 */
int panda_snap(char *snapshot_name) {
    Error *err = NULL;

    panda_modules_check_vmstate("savevm");
    if (!save_snapshot(snapshot_name, true, NULL, false, NULL, &err)) {
        error_report_err(err);
        return -1;
    }
    return 0;
}

int panda_revert(char *snapshot_name) {
    Error *err = NULL;
    /* refused (while recording or replaying) before the VM stops: it keeps running */
    if (!replay_can_load_snapshot(&err)) {
        error_report_err(err);
        return -1;
    }
    if (runstate_is_running()) {
        vm_stop(RUN_STATE_RESTORE_VM);
    }
    if (!load_snapshot(snapshot_name, NULL, false, NULL, &err)) {
        error_report_err(err);
        return -1;
    }
    return 0;
}

int panda_delvm(char *snapshot_name) {
    Error *err = NULL;
    if (!delete_snapshot(snapshot_name, false, NULL, &err)) {
        error_report_err(err);
        return -1;
    }
    return 0;
}

void panda_reset(void) {
    qemu_system_reset_request(SHUTDOWN_CAUSE_HOST_QMP_SYSTEM_RESET);
}

// void panda_cleanup_record(void){
//     if(rr_in_record()){
//         rr_do_end_record();
//     }
// }

// int panda_finish(void) {
//     return main_aux(0, 0, 0, PANDA_FINISH);
// }

// bool panda_was_aborted(void) {
//   return panda_aborted;
// }

extern const char *qemu_file;

void panda_set_qemu_path(char* filepath) {
    // *copy* filepath into a new buffer, also free & update qemu_file
    if (qemu_file != NULL)
      free((void*)qemu_file);

    qemu_file=strdup(filepath);
}

extern const char *extra_plugin_path;

void panda_set_extra_plugin_path(char* filepath) {
    printf("set plugin path %s\n", filepath);
    extra_plugin_path = strdup(filepath);
}

int panda_init_plugin(char *plugin_name, char **plugin_args, uint32_t num_args) {
    for (uint32_t i=0; i<num_args; i++)
        panda_add_arg(plugin_name, plugin_args[i]);
    char *plugin_path = panda_plugin_path((const char *) plugin_name);
    return panda_load_plugin(plugin_path, plugin_name);
}


// panda_cb is defined in callbacks/cb-defs.h
void panda_register_callback_helper(void *plugin, panda_cb_type type, panda_cb* cb) {
	panda_cb cb_copy;
	memcpy(&cb_copy,cb, sizeof(panda_cb));
	panda_register_callback(plugin, type, cb_copy);
}

void panda_enable_callback_helper(void *plugin, panda_cb_type type, panda_cb* cb) {
	panda_cb cb_copy;
	memcpy(&cb_copy,cb, sizeof(panda_cb));
	panda_enable_callback(plugin, type, cb_copy);
}

void panda_disable_callback_helper(void *plugin, panda_cb_type type, panda_cb* cb) {
	panda_cb cb_copy;
	memcpy(&cb_copy,cb, sizeof(panda_cb));
	panda_disable_callback(plugin, type, cb_copy);
}

//int panda_replay(char *replay_name) -> Now use panda_replay_being(char * replay_name)

// uint64_t rr_get_guest_instr_count_external(void){
// 	return rr_get_guest_instr_count();
// }

// XXX: why do we have these as _external wrappers instead of just using the real fns?
// XXX: b/c funcs called via wrapper are inlined, don't otherwise get exported. Unclear if inlining has any significat perf benefit.
int panda_virtual_memory_read_external(CPUState *env, target_ulong addr, char *buf, int len){
	return panda_virtual_memory_read(env, addr, (uint8_t*) buf, len);
}

int panda_virtual_memory_write_external(CPUState *env, target_ulong addr, char *buf, int len){
	return panda_virtual_memory_write(env, addr, (uint8_t*) buf, len);
}

#ifdef CONFIG_SOFTMMU
int panda_physical_memory_read_external(hwaddr addr, uint8_t *buf, int len){
	return panda_physical_memory_read(addr, buf, len);
}

int panda_physical_memory_write_external(hwaddr addr, uint8_t *buf, int len){
	return panda_physical_memory_write(addr,buf,len);
}
#endif

bool panda_in_kernel_external(const CPUState *cpu){
	return panda_in_kernel(cpu);
}

bool panda_in_kernel_mode_external(const CPUState *cpu){
	return panda_in_kernel_mode(cpu);
}

bool panda_in_kernel_code_linux_external(CPUState *cpu){
	return panda_in_kernel_code_linux(cpu);
}

target_ulong panda_current_sp_external(const CPUState *cpu){
	return panda_current_sp(cpu);
}

target_ulong panda_current_ksp_external(CPUState *cpu){
	return panda_current_ksp(cpu);
}

target_ulong panda_current_sp_masked_pagesize_external(const CPUState *cpu, target_ulong mask){
	return (panda_current_sp(cpu) & (~(mask+mask-1)));
}

#ifdef CONFIG_SOFTMMU
target_ulong panda_virt_to_phys_external(CPUState *cpu, target_ulong virt_addr) {
  return panda_virt_to_phys(cpu, virt_addr);
}
#endif

target_ulong panda_get_retval_external(const CPUState *cpu){
	return panda_get_retval(cpu);
}

// MemTxResult PandaPhysicalAddressToRamOffset_external(ram_addr_t* out, hwaddr addr, bool is_write){
//     return PandaPhysicalAddressToRamOffset(out, addr, is_write);
// }
// don't expose to API  because we don't want to add siginfo_t understanding
// set to true if panda_setup_signal_handling is called
void (*panda_external_signal_handler)(int, siginfo_t*,void*);

void panda_setup_signal_handling(void (*f) (int, void*, void *))
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_sigaction = (void(*)(int,siginfo_t*,void*))f;
    act.sa_flags = SA_SIGINFO;
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);
    panda_external_signal_handler = (void(*)(int,siginfo_t*,void*)) f;
}

// Taken from Avatar2's Configurable Machine (hw/avatar/configurable_machine.c), as wrapup-rebase
// (6b2103e, 2b5b2db) restored it. memory_region_init_ram registers the RAM for snapshots itself.
// Takes the BQL if the caller (a vCPU callback) doesn't hold it.
int map_memory(char *name, uint64_t size, uint64_t address) {
    Error *err = NULL;
    bool locked = bql_locked();
    MemoryRegion *ram = g_new(MemoryRegion, 1);

    if (!locked) {
        bql_lock();
    }
    memory_region_init_ram(ram, NULL, name, size, &err);
    if (err) {
        error_reportf_err(err, "map_memory %s: ", name);
        g_free(ram);
    } else {
        memory_region_add_subregion(get_system_memory(), address, ram);
    }
    if (!locked) {
        bql_unlock();
    }
    return err ? -1 : 0;
}

/* The running vCPU; first_cpu outside a vCPU thread (current_cpu is NULL in the main loop). */
CPUState* get_cpu(void) {
  return current_cpu ? current_cpu : first_cpu;
}

int panda_cpu_get_index(CPUState *q){
    CPUState *cpu;
    int i = 0;
    CPU_FOREACH(cpu) {
        if (cpu == q){
            return i;
        }
        i++;
    }
    return 0;
}

// Get the length of a GArray list
unsigned long garray_len(GArray *list) {
    if (list == NULL)
        return 0;
    return list->len;
}

// For enabling library mode
// void _panda_set_library_mode(const bool b) {
//   panda_set_library_mode(b);
// }

CPUArchState * panda_cpu_env(CPUState *cpu){
    return cpu_env(cpu);
}
