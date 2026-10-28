#include "panda.h"

#include "syscalls2.h"
#include "syscalls2_info.h"
#include "hooks/hooks_int_fns.h"
//#include "hw_proc_id/hw_proc_id_ext.h"

extern bool load_info;
#if defined(TARGET_X86_64)
static bool first_load = true;
static syscall_info_t *info;
static syscall_meta_t *meta;
#endif

extern "C" {
#include "syscalls_ext_typedefs.h"
#include "syscall_ppp_extern_enter.h"
#include "syscall_ppp_extern_return.h"

extern Profile profiles[];
}

target_ulong syscalls_get_id(CPUState* cpu);

/**
 * @brief Called when a system call invocation is identified.
 * Invokes all registered callbacks that should run for the call.
 *
 * Additionally, stores the context of the system call (number, asid,
 * arguments, return address) to prepare for handling the respective
 * system call return callbacks.
 */
void syscall_enter_switch_freebsd_x64(CPUState *cpu, int profile, target_ptr_t pc, int static_callno) {
#if defined(TARGET_X86_64)
	CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
	syscall_ctx_t ctx = {0};
	ctx.profile = profile;
	if (static_callno == -1) {
	  ctx.no = env->regs[R_EAX];
	} else {
	  ctx.no = static_callno;
	}
	ctx.asid = syscalls_get_id(cpu);
	ctx.retaddr = calc_retaddr(cpu, &ctx, pc);
	ctx.double_return = false;
	bool panda_noreturn;	// true if PANDA should not track the return of this system call
	// Read these counts after enter callbacks: a callback can subscribe to this return.
	const int *return_count;
	const int *return_context_count;
	const syscall_info_t *call = NULL;
	syscall_info_t zero = {0};

	// only try this once
	if (first_load){
		first_load = false;
		if (load_info){
			sysinfo_load_profile(ctx.profile, &info, &meta);
		}
	}

	if (meta != NULL && ctx.no <= meta->max_generic) {
	  // If the syscall_info object from dso_info_....c doesn't have an entry
	  // for this syscall, we want to leave it as a NULL pointer
	  if (memcmp(&info[ctx.no], &zero, sizeof(syscall_info_t)) != 0) {
		call = &info[ctx.no];
	  }
	}

	switch (ctx.no) {
	// 0, 198 int nosys ['void']
	case 0: case 198: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nosys_return_num_cb;
		return_context_count = &ppp_on_nosys_return_num_cb_with_context;
		PPP_RUN_CB(on_nosys_enter, cpu, pc);
	}; break;
	// 1 void sys_exit ['int rval']
	case 1: {
		panda_noreturn = true;
		ctx.double_return = false;
		return_count = &ppp_on_sys_exit_return_num_cb;
		return_context_count = &ppp_on_sys_exit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sys_exit_return)));
		if (save_args || PPP_CHECK_CB(on_sys_exit_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_exit_enter, cpu, pc, arg0);
		}
	}; break;
	// 2 int fork ['void']
	case 2: {
		panda_noreturn = false;
		ctx.double_return = true;
		return_count = &ppp_on_fork_return_num_cb;
		return_context_count = &ppp_on_fork_return_num_cb_with_context;
		PPP_RUN_CB(on_fork_enter, cpu, pc);
	}; break;
	// 3 ssize_t read ['int fd', 'void *buf', 'size_t nbyte']
	case 3: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_read_return_num_cb;
		return_context_count = &ppp_on_read_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_read_return)));
		if (save_args || PPP_CHECK_CB(on_read_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_read_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 4 ssize_t write ['int fd', 'const void *buf', 'size_t nbyte']
	case 4: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_write_return_num_cb;
		return_context_count = &ppp_on_write_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_write_return)));
		if (save_args || PPP_CHECK_CB(on_write_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_write_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 5 int open ['const char *path', 'int flags', 'mode_t mode']
	case 5: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_open_return_num_cb;
		return_context_count = &ppp_on_open_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_open_return)));
		if (save_args || PPP_CHECK_CB(on_open_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_open_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 6 int close ['int fd']
	case 6: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_close_return_num_cb;
		return_context_count = &ppp_on_close_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_close_return)));
		if (save_args || PPP_CHECK_CB(on_close_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_close_enter, cpu, pc, arg0);
		}
	}; break;
	// 7 int wait4 ['int pid', 'int *status', 'int options', 'struct rusage *rusage']
	case 7: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_wait4_return_num_cb;
		return_context_count = &ppp_on_wait4_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_wait4_return)));
		if (save_args || PPP_CHECK_CB(on_wait4_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_wait4_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 8 int creat ['const char *path', 'int mode']
	case 8: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_creat_return_num_cb;
		return_context_count = &ppp_on_creat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_creat_return)));
		if (save_args || PPP_CHECK_CB(on_creat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_creat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 9 int link ['const char *path', 'const char *link']
	case 9: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_link_return_num_cb;
		return_context_count = &ppp_on_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_link_return)));
		if (save_args || PPP_CHECK_CB(on_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_link_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 10 int unlink ['const char *path']
	case 10: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_unlink_return_num_cb;
		return_context_count = &ppp_on_unlink_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_unlink_return)));
		if (save_args || PPP_CHECK_CB(on_unlink_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_unlink_enter, cpu, pc, arg0);
		}
	}; break;
	// 12 int chdir ['const char *path']
	case 12: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_chdir_return_num_cb;
		return_context_count = &ppp_on_chdir_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_chdir_return)));
		if (save_args || PPP_CHECK_CB(on_chdir_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_chdir_enter, cpu, pc, arg0);
		}
	}; break;
	// 13 int fchdir ['int fd']
	case 13: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fchdir_return_num_cb;
		return_context_count = &ppp_on_fchdir_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fchdir_return)));
		if (save_args || PPP_CHECK_CB(on_fchdir_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fchdir_enter, cpu, pc, arg0);
		}
	}; break;
	// 14 int mknod ['const char *path', 'int mode', 'uint32_t dev']
	case 14: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mknod_return_num_cb;
		return_context_count = &ppp_on_mknod_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mknod_return)));
		if (save_args || PPP_CHECK_CB(on_mknod_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_mknod_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 15 int chmod ['const char *path', 'mode_t mode']
	case 15: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_chmod_return_num_cb;
		return_context_count = &ppp_on_chmod_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_chmod_return)));
		if (save_args || PPP_CHECK_CB(on_chmod_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_chmod_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 16 int chown ['const char *path', 'int uid', 'int gid']
	case 16: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_chown_return_num_cb;
		return_context_count = &ppp_on_chown_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_chown_return)));
		if (save_args || PPP_CHECK_CB(on_chown_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_chown_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 18, 395, 557 int getfsstat ['struct ostatfs *buf', 'long bufsize', 'int mode'] OR ['struct freebsd11_statfs *buf', 'long bufsize', 'int mode'] OR ['struct statfs *buf', 'long bufsize', 'int mode']
	case 18: case 395: case 557: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getfsstat_return_num_cb;
		return_context_count = &ppp_on_getfsstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getfsstat_return)));
		if (save_args || PPP_CHECK_CB(on_getfsstat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int64_t arg1 = get_s64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_getfsstat_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 20 pid_t getpid ['void']
	case 20: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getpid_return_num_cb;
		return_context_count = &ppp_on_getpid_return_num_cb_with_context;
		PPP_RUN_CB(on_getpid_enter, cpu, pc);
	}; break;
	// 21 int mount ['const char *type', 'const char *path', 'int flags', 'void *data']
	case 21: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mount_return_num_cb;
		return_context_count = &ppp_on_mount_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mount_return)));
		if (save_args || PPP_CHECK_CB(on_mount_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_mount_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 22 int unmount ['const char *path', 'int flags']
	case 22: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_unmount_return_num_cb;
		return_context_count = &ppp_on_unmount_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_unmount_return)));
		if (save_args || PPP_CHECK_CB(on_unmount_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_unmount_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 23 int setuid ['uid_t uid']
	case 23: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setuid_return_num_cb;
		return_context_count = &ppp_on_setuid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setuid_return)));
		if (save_args || PPP_CHECK_CB(on_setuid_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_setuid_enter, cpu, pc, arg0);
		}
	}; break;
	// 24 uid_t getuid ['void']
	case 24: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getuid_return_num_cb;
		return_context_count = &ppp_on_getuid_return_num_cb_with_context;
		PPP_RUN_CB(on_getuid_enter, cpu, pc);
	}; break;
	// 25 uid_t geteuid ['void']
	case 25: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_geteuid_return_num_cb;
		return_context_count = &ppp_on_geteuid_return_num_cb_with_context;
		PPP_RUN_CB(on_geteuid_enter, cpu, pc);
	}; break;
	// 26 int ptrace ['int req', 'pid_t pid', 'caddr_t addr', 'int data']
	case 26: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ptrace_return_num_cb;
		return_context_count = &ppp_on_ptrace_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ptrace_return)));
		if (save_args || PPP_CHECK_CB(on_ptrace_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_ptrace_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 27, 113 int recvmsg ['int s', 'struct msghdr *msg', 'int flags'] OR ['int s', 'struct omsghdr *msg', 'int flags']
	case 27: case 113: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_recvmsg_return_num_cb;
		return_context_count = &ppp_on_recvmsg_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_recvmsg_return)));
		if (save_args || PPP_CHECK_CB(on_recvmsg_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_recvmsg_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 28, 114 int sendmsg ['int s', 'struct msghdr *msg', 'int flags'] OR ['int s', 'const void *msg', 'int flags']
	case 28: case 114: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sendmsg_return_num_cb;
		return_context_count = &ppp_on_sendmsg_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sendmsg_return)));
		if (save_args || PPP_CHECK_CB(on_sendmsg_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sendmsg_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 29, 125 int recvfrom ['int s', 'void *buf', 'size_t len', 'int flags', 'struct sockaddr *from', '__socklen_t *fromlenaddr'] OR ['int s', 'void *buf', 'size_t len', 'int flags', 'struct sockaddr *from', 'int *fromlenaddr']
	case 29: case 125: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_recvfrom_return_num_cb;
		return_context_count = &ppp_on_recvfrom_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_recvfrom_return)));
		if (save_args || PPP_CHECK_CB(on_recvfrom_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_recvfrom_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 30, 99 int accept ['int s', 'struct sockaddr *name', '__socklen_t *anamelen'] OR ['int s', 'struct sockaddr *name', 'int *anamelen']
	case 30: case 99: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_accept_return_num_cb;
		return_context_count = &ppp_on_accept_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_accept_return)));
		if (save_args || PPP_CHECK_CB(on_accept_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_accept_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 31, 141 int getpeername ['int fdes', 'struct sockaddr *asa', '__socklen_t *alen'] OR ['int fdes', 'struct sockaddr *asa', 'int *alen']
	case 31: case 141: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getpeername_return_num_cb;
		return_context_count = &ppp_on_getpeername_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getpeername_return)));
		if (save_args || PPP_CHECK_CB(on_getpeername_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getpeername_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 32 int getsockname ['int fdes', 'struct sockaddr *asa', '__socklen_t *alen']
	case 32: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getsockname_return_num_cb;
		return_context_count = &ppp_on_getsockname_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getsockname_return)));
		if (save_args || PPP_CHECK_CB(on_getsockname_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getsockname_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 33 int access ['const char *path', 'int amode']
	case 33: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_access_return_num_cb;
		return_context_count = &ppp_on_access_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_access_return)));
		if (save_args || PPP_CHECK_CB(on_access_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_access_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 34 int chflags ['const char *path', 'u_long flags']
	case 34: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_chflags_return_num_cb;
		return_context_count = &ppp_on_chflags_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_chflags_return)));
		if (save_args || PPP_CHECK_CB(on_chflags_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int64_t arg1 = get_s64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int64_t));
			}
			PPP_RUN_CB(on_chflags_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 35 int fchflags ['int fd', 'u_long flags']
	case 35: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fchflags_return_num_cb;
		return_context_count = &ppp_on_fchflags_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fchflags_return)));
		if (save_args || PPP_CHECK_CB(on_fchflags_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int64_t arg1 = get_s64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int64_t));
			}
			PPP_RUN_CB(on_fchflags_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 36 int sync ['void']
	case 36: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sync_return_num_cb;
		return_context_count = &ppp_on_sync_return_num_cb_with_context;
		PPP_RUN_CB(on_sync_enter, cpu, pc);
	}; break;
	// 37 int kill ['int pid', 'int signum']
	case 37: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kill_return_num_cb;
		return_context_count = &ppp_on_kill_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kill_return)));
		if (save_args || PPP_CHECK_CB(on_kill_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_kill_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 38, 188 int stat ['const char *path', 'struct ostat *ub'] OR ['const char *path', 'struct freebsd11_stat *ub']
	case 38: case 188: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_stat_return_num_cb;
		return_context_count = &ppp_on_stat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_stat_return)));
		if (save_args || PPP_CHECK_CB(on_stat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_stat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 39 pid_t getppid ['void']
	case 39: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getppid_return_num_cb;
		return_context_count = &ppp_on_getppid_return_num_cb_with_context;
		PPP_RUN_CB(on_getppid_enter, cpu, pc);
	}; break;
	// 40, 190 int lstat ['const char *path', 'struct ostat *ub'] OR ['const char *path', 'struct freebsd11_stat *ub']
	case 40: case 190: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lstat_return_num_cb;
		return_context_count = &ppp_on_lstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lstat_return)));
		if (save_args || PPP_CHECK_CB(on_lstat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_lstat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 41 int dup ['unsigned fd']
	case 41: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_dup_return_num_cb;
		return_context_count = &ppp_on_dup_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_dup_return)));
		if (save_args || PPP_CHECK_CB(on_dup_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_dup_enter, cpu, pc, arg0);
		}
	}; break;
	// 42 int pipe ['void']
	case 42: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pipe_return_num_cb;
		return_context_count = &ppp_on_pipe_return_num_cb_with_context;
		PPP_RUN_CB(on_pipe_enter, cpu, pc);
	}; break;
	// 43 gid_t getegid ['void']
	case 43: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getegid_return_num_cb;
		return_context_count = &ppp_on_getegid_return_num_cb_with_context;
		PPP_RUN_CB(on_getegid_enter, cpu, pc);
	}; break;
	// 44 int profil ['char *samples', 'size_t size', 'size_t offset', 'unsigned scale']
	case 44: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_profil_return_num_cb;
		return_context_count = &ppp_on_profil_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_profil_return)));
		if (save_args || PPP_CHECK_CB(on_profil_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_profil_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 45 int ktrace ['const char *fname', 'int ops', 'int facs', 'int pid']
	case 45: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ktrace_return_num_cb;
		return_context_count = &ppp_on_ktrace_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ktrace_return)));
		if (save_args || PPP_CHECK_CB(on_ktrace_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_ktrace_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 46 int sigaction ['int signum', 'struct osigaction *nsa', 'struct osigaction *osa']
	case 46: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigaction_return_num_cb;
		return_context_count = &ppp_on_sigaction_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigaction_return)));
		if (save_args || PPP_CHECK_CB(on_sigaction_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigaction_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 47 gid_t getgid ['void']
	case 47: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getgid_return_num_cb;
		return_context_count = &ppp_on_getgid_return_num_cb_with_context;
		PPP_RUN_CB(on_getgid_enter, cpu, pc);
	}; break;
	// 49 int getlogin ['char *namebuf', 'unsigned namelen']
	case 49: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getlogin_return_num_cb;
		return_context_count = &ppp_on_getlogin_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getlogin_return)));
		if (save_args || PPP_CHECK_CB(on_getlogin_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_getlogin_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 50 int setlogin ['const char *namebuf']
	case 50: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setlogin_return_num_cb;
		return_context_count = &ppp_on_setlogin_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setlogin_return)));
		if (save_args || PPP_CHECK_CB(on_setlogin_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_setlogin_enter, cpu, pc, arg0);
		}
	}; break;
	// 51 int acct ['const char *path']
	case 51: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_acct_return_num_cb;
		return_context_count = &ppp_on_acct_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_acct_return)));
		if (save_args || PPP_CHECK_CB(on_acct_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_acct_enter, cpu, pc, arg0);
		}
	}; break;
	// 53 int sigaltstack ['stack_t *ss', 'stack_t *oss']
	case 53: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigaltstack_return_num_cb;
		return_context_count = &ppp_on_sigaltstack_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigaltstack_return)));
		if (save_args || PPP_CHECK_CB(on_sigaltstack_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigaltstack_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 54 int ioctl ['int fd', 'u_long com', 'char *data']
	case 54: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ioctl_return_num_cb;
		return_context_count = &ppp_on_ioctl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ioctl_return)));
		if (save_args || PPP_CHECK_CB(on_ioctl_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int64_t arg1 = get_s64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ioctl_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 55 int reboot ['int opt']
	case 55: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_reboot_return_num_cb;
		return_context_count = &ppp_on_reboot_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_reboot_return)));
		if (save_args || PPP_CHECK_CB(on_reboot_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_reboot_enter, cpu, pc, arg0);
		}
	}; break;
	// 56 int revoke ['const char *path']
	case 56: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_revoke_return_num_cb;
		return_context_count = &ppp_on_revoke_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_revoke_return)));
		if (save_args || PPP_CHECK_CB(on_revoke_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_revoke_enter, cpu, pc, arg0);
		}
	}; break;
	// 57 int symlink ['const char *path', 'const char *link']
	case 57: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_symlink_return_num_cb;
		return_context_count = &ppp_on_symlink_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_symlink_return)));
		if (save_args || PPP_CHECK_CB(on_symlink_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_symlink_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 58 ssize_t readlink ['const char *path', 'char *buf', 'size_t count']
	case 58: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_readlink_return_num_cb;
		return_context_count = &ppp_on_readlink_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_readlink_return)));
		if (save_args || PPP_CHECK_CB(on_readlink_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_readlink_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 59 int execve ['const char *fname', 'char **argv', 'char **envv']
	case 59: {
		panda_noreturn = true;
		ctx.double_return = false;
		return_count = &ppp_on_execve_return_num_cb;
		return_context_count = &ppp_on_execve_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_execve_return)));
		if (save_args || PPP_CHECK_CB(on_execve_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_execve_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 60 int umask ['mode_t newmask']
	case 60: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_umask_return_num_cb;
		return_context_count = &ppp_on_umask_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_umask_return)));
		if (save_args || PPP_CHECK_CB(on_umask_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_umask_enter, cpu, pc, arg0);
		}
	}; break;
	// 61 int chroot ['const char *path']
	case 61: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_chroot_return_num_cb;
		return_context_count = &ppp_on_chroot_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_chroot_return)));
		if (save_args || PPP_CHECK_CB(on_chroot_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_chroot_enter, cpu, pc, arg0);
		}
	}; break;
	// 62, 189, 551 int fstat ['int fd', 'struct ostat *sb'] OR ['int fd', 'struct freebsd11_stat *sb'] OR ['int fd', 'struct stat *sb']
	case 62: case 189: case 551: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fstat_return_num_cb;
		return_context_count = &ppp_on_fstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fstat_return)));
		if (save_args || PPP_CHECK_CB(on_fstat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_fstat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 63 int getkerninfo ['int op', 'char *where', 'size_t *size', 'int arg']
	case 63: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getkerninfo_return_num_cb;
		return_context_count = &ppp_on_getkerninfo_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getkerninfo_return)));
		if (save_args || PPP_CHECK_CB(on_getkerninfo_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_getkerninfo_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 64 int getpagesize ['void']
	case 64: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getpagesize_return_num_cb;
		return_context_count = &ppp_on_getpagesize_return_num_cb_with_context;
		PPP_RUN_CB(on_getpagesize_enter, cpu, pc);
	}; break;
	// 65 int msync ['void *addr', 'size_t len', 'int flags']
	case 65: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_msync_return_num_cb;
		return_context_count = &ppp_on_msync_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_msync_return)));
		if (save_args || PPP_CHECK_CB(on_msync_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_msync_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 66 int vfork ['void']
	case 66: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_vfork_return_num_cb;
		return_context_count = &ppp_on_vfork_return_num_cb_with_context;
		PPP_RUN_CB(on_vfork_enter, cpu, pc);
	}; break;
	// 69 int sbrk ['int incr']
	case 69: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sbrk_return_num_cb;
		return_context_count = &ppp_on_sbrk_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sbrk_return)));
		if (save_args || PPP_CHECK_CB(on_sbrk_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sbrk_enter, cpu, pc, arg0);
		}
	}; break;
	// 70 int sstk ['int incr']
	case 70: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sstk_return_num_cb;
		return_context_count = &ppp_on_sstk_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sstk_return)));
		if (save_args || PPP_CHECK_CB(on_sstk_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sstk_enter, cpu, pc, arg0);
		}
	}; break;
	// 72 int vadvise ['int anom']
	case 72: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_vadvise_return_num_cb;
		return_context_count = &ppp_on_vadvise_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_vadvise_return)));
		if (save_args || PPP_CHECK_CB(on_vadvise_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_vadvise_enter, cpu, pc, arg0);
		}
	}; break;
	// 73 int munmap ['void *addr', 'size_t len']
	case 73: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_munmap_return_num_cb;
		return_context_count = &ppp_on_munmap_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_munmap_return)));
		if (save_args || PPP_CHECK_CB(on_munmap_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_munmap_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 74 int mprotect ['void *addr', 'size_t len', 'int prot']
	case 74: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mprotect_return_num_cb;
		return_context_count = &ppp_on_mprotect_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mprotect_return)));
		if (save_args || PPP_CHECK_CB(on_mprotect_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_mprotect_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 75 int madvise ['void *addr', 'size_t len', 'int behav']
	case 75: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_madvise_return_num_cb;
		return_context_count = &ppp_on_madvise_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_madvise_return)));
		if (save_args || PPP_CHECK_CB(on_madvise_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_madvise_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 78 int mincore ['const void *addr', 'size_t len', 'char *vec']
	case 78: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mincore_return_num_cb;
		return_context_count = &ppp_on_mincore_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mincore_return)));
		if (save_args || PPP_CHECK_CB(on_mincore_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_mincore_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 79 int getgroups ['unsigned gidsetsize', 'gid_t *gidset']
	case 79: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getgroups_return_num_cb;
		return_context_count = &ppp_on_getgroups_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getgroups_return)));
		if (save_args || PPP_CHECK_CB(on_getgroups_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getgroups_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 80 int setgroups ['unsigned gidsetsize', 'gid_t *gidset']
	case 80: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setgroups_return_num_cb;
		return_context_count = &ppp_on_setgroups_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setgroups_return)));
		if (save_args || PPP_CHECK_CB(on_setgroups_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_setgroups_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 81 int getpgrp ['void']
	case 81: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getpgrp_return_num_cb;
		return_context_count = &ppp_on_getpgrp_return_num_cb_with_context;
		PPP_RUN_CB(on_getpgrp_enter, cpu, pc);
	}; break;
	// 82 int setpgid ['int pid', 'int pgid']
	case 82: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setpgid_return_num_cb;
		return_context_count = &ppp_on_setpgid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setpgid_return)));
		if (save_args || PPP_CHECK_CB(on_setpgid_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_setpgid_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 83 int setitimer ['unsigned which', 'struct itimerval *itv', 'struct itimerval *oitv']
	case 83: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setitimer_return_num_cb;
		return_context_count = &ppp_on_setitimer_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setitimer_return)));
		if (save_args || PPP_CHECK_CB(on_setitimer_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_setitimer_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 84 int wait ['void']
	case 84: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_wait_return_num_cb;
		return_context_count = &ppp_on_wait_return_num_cb_with_context;
		PPP_RUN_CB(on_wait_enter, cpu, pc);
	}; break;
	// 85 int swapon ['const char *name']
	case 85: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_swapon_return_num_cb;
		return_context_count = &ppp_on_swapon_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_swapon_return)));
		if (save_args || PPP_CHECK_CB(on_swapon_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_swapon_enter, cpu, pc, arg0);
		}
	}; break;
	// 86 int getitimer ['unsigned which', 'struct itimerval *itv']
	case 86: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getitimer_return_num_cb;
		return_context_count = &ppp_on_getitimer_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getitimer_return)));
		if (save_args || PPP_CHECK_CB(on_getitimer_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getitimer_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 87 int gethostname ['char *hostname', 'unsigned len']
	case 87: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_gethostname_return_num_cb;
		return_context_count = &ppp_on_gethostname_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_gethostname_return)));
		if (save_args || PPP_CHECK_CB(on_gethostname_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_gethostname_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 88 int sethostname ['char *hostname', 'unsigned len']
	case 88: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sethostname_return_num_cb;
		return_context_count = &ppp_on_sethostname_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sethostname_return)));
		if (save_args || PPP_CHECK_CB(on_sethostname_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sethostname_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 89 int getdtablesize ['void']
	case 89: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getdtablesize_return_num_cb;
		return_context_count = &ppp_on_getdtablesize_return_num_cb_with_context;
		PPP_RUN_CB(on_getdtablesize_enter, cpu, pc);
	}; break;
	// 90 int dup2 ['unsigned from', 'unsigned to']
	case 90: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_dup2_return_num_cb;
		return_context_count = &ppp_on_dup2_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_dup2_return)));
		if (save_args || PPP_CHECK_CB(on_dup2_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_dup2_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 92 int fcntl ['int fd', 'int cmd', 'long arg']
	case 92: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fcntl_return_num_cb;
		return_context_count = &ppp_on_fcntl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fcntl_return)));
		if (save_args || PPP_CHECK_CB(on_fcntl_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int64_t arg2 = get_s64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int64_t));
			}
			PPP_RUN_CB(on_fcntl_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 93 int select ['int nd', 'fd_set *in', 'fd_set *ou', 'fd_set *ex', 'struct timeval *tv']
	case 93: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_select_return_num_cb;
		return_context_count = &ppp_on_select_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_select_return)));
		if (save_args || PPP_CHECK_CB(on_select_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_select_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 95 int fsync ['int fd']
	case 95: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fsync_return_num_cb;
		return_context_count = &ppp_on_fsync_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fsync_return)));
		if (save_args || PPP_CHECK_CB(on_fsync_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fsync_enter, cpu, pc, arg0);
		}
	}; break;
	// 96 int setpriority ['int which', 'int who', 'int prio']
	case 96: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setpriority_return_num_cb;
		return_context_count = &ppp_on_setpriority_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setpriority_return)));
		if (save_args || PPP_CHECK_CB(on_setpriority_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_setpriority_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 97 int socket ['int domain', 'int type', 'int protocol']
	case 97: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_socket_return_num_cb;
		return_context_count = &ppp_on_socket_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_socket_return)));
		if (save_args || PPP_CHECK_CB(on_socket_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_socket_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 98 int connect ['int s', 'const struct sockaddr *name', 'int namelen']
	case 98: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_connect_return_num_cb;
		return_context_count = &ppp_on_connect_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_connect_return)));
		if (save_args || PPP_CHECK_CB(on_connect_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_connect_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 100 int getpriority ['int which', 'int who']
	case 100: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getpriority_return_num_cb;
		return_context_count = &ppp_on_getpriority_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getpriority_return)));
		if (save_args || PPP_CHECK_CB(on_getpriority_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_getpriority_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 101 int send ['int s', 'const void *buf', 'int len', 'int flags']
	case 101: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_send_return_num_cb;
		return_context_count = &ppp_on_send_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_send_return)));
		if (save_args || PPP_CHECK_CB(on_send_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_send_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 102 int recv ['int s', 'void *buf', 'int len', 'int flags']
	case 102: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_recv_return_num_cb;
		return_context_count = &ppp_on_recv_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_recv_return)));
		if (save_args || PPP_CHECK_CB(on_recv_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_recv_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 103, 344, 417 int sigreturn ['struct osigcontext *sigcntxp'] OR ['const struct ucontext4 *sigcntxp'] OR ['const struct __ucontext *sigcntxp']
	case 103: case 344: case 417: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigreturn_return_num_cb;
		return_context_count = &ppp_on_sigreturn_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigreturn_return)));
		if (save_args || PPP_CHECK_CB(on_sigreturn_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigreturn_enter, cpu, pc, arg0);
		}
	}; break;
	// 104 int bind ['int s', 'const struct sockaddr *name', 'int namelen']
	case 104: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_bind_return_num_cb;
		return_context_count = &ppp_on_bind_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_bind_return)));
		if (save_args || PPP_CHECK_CB(on_bind_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_bind_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 105 int setsockopt ['int s', 'int level', 'int name', 'const void *val', 'int valsize']
	case 105: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setsockopt_return_num_cb;
		return_context_count = &ppp_on_setsockopt_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setsockopt_return)));
		if (save_args || PPP_CHECK_CB(on_setsockopt_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		int32_t arg4 = get_s32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(int32_t));
			}
			PPP_RUN_CB(on_setsockopt_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 106 int listen ['int s', 'int backlog']
	case 106: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_listen_return_num_cb;
		return_context_count = &ppp_on_listen_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_listen_return)));
		if (save_args || PPP_CHECK_CB(on_listen_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_listen_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 108 int sigvec ['int signum', 'struct sigvec *nsv', 'struct sigvec *osv']
	case 108: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigvec_return_num_cb;
		return_context_count = &ppp_on_sigvec_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigvec_return)));
		if (save_args || PPP_CHECK_CB(on_sigvec_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigvec_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 109 int sigblock ['int mask']
	case 109: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigblock_return_num_cb;
		return_context_count = &ppp_on_sigblock_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigblock_return)));
		if (save_args || PPP_CHECK_CB(on_sigblock_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sigblock_enter, cpu, pc, arg0);
		}
	}; break;
	// 110 int sigsetmask ['int mask']
	case 110: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigsetmask_return_num_cb;
		return_context_count = &ppp_on_sigsetmask_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigsetmask_return)));
		if (save_args || PPP_CHECK_CB(on_sigsetmask_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sigsetmask_enter, cpu, pc, arg0);
		}
	}; break;
	// 111 int sigsuspend ['osigset_t mask']
	case 111: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigsuspend_return_num_cb;
		return_context_count = &ppp_on_sigsuspend_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigsuspend_return)));
		if (save_args || PPP_CHECK_CB(on_sigsuspend_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sigsuspend_enter, cpu, pc, arg0);
		}
	}; break;
	// 112 int sigstack ['struct sigstack *nss', 'struct sigstack *oss']
	case 112: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigstack_return_num_cb;
		return_context_count = &ppp_on_sigstack_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigstack_return)));
		if (save_args || PPP_CHECK_CB(on_sigstack_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigstack_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 116 int gettimeofday ['struct timeval *tp', 'struct timezone *tzp']
	case 116: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_gettimeofday_return_num_cb;
		return_context_count = &ppp_on_gettimeofday_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_gettimeofday_return)));
		if (save_args || PPP_CHECK_CB(on_gettimeofday_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_gettimeofday_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 117 int getrusage ['int who', 'struct rusage *rusage']
	case 117: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getrusage_return_num_cb;
		return_context_count = &ppp_on_getrusage_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getrusage_return)));
		if (save_args || PPP_CHECK_CB(on_getrusage_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getrusage_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 118 int getsockopt ['int s', 'int level', 'int name', 'void *val', 'int *avalsize']
	case 118: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getsockopt_return_num_cb;
		return_context_count = &ppp_on_getsockopt_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getsockopt_return)));
		if (save_args || PPP_CHECK_CB(on_getsockopt_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getsockopt_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 120 int readv ['int fd', 'struct iovec *iovp', 'unsigned iovcnt']
	case 120: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_readv_return_num_cb;
		return_context_count = &ppp_on_readv_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_readv_return)));
		if (save_args || PPP_CHECK_CB(on_readv_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_readv_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 121 int writev ['int fd', 'struct iovec *iovp', 'unsigned iovcnt']
	case 121: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_writev_return_num_cb;
		return_context_count = &ppp_on_writev_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_writev_return)));
		if (save_args || PPP_CHECK_CB(on_writev_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_writev_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 122 int settimeofday ['struct timeval *tv', 'struct timezone *tzp']
	case 122: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_settimeofday_return_num_cb;
		return_context_count = &ppp_on_settimeofday_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_settimeofday_return)));
		if (save_args || PPP_CHECK_CB(on_settimeofday_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_settimeofday_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 123 int fchown ['int fd', 'int uid', 'int gid']
	case 123: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fchown_return_num_cb;
		return_context_count = &ppp_on_fchown_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fchown_return)));
		if (save_args || PPP_CHECK_CB(on_fchown_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fchown_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 124 int fchmod ['int fd', 'mode_t mode']
	case 124: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fchmod_return_num_cb;
		return_context_count = &ppp_on_fchmod_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fchmod_return)));
		if (save_args || PPP_CHECK_CB(on_fchmod_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_fchmod_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 126 int setreuid ['int ruid', 'int euid']
	case 126: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setreuid_return_num_cb;
		return_context_count = &ppp_on_setreuid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setreuid_return)));
		if (save_args || PPP_CHECK_CB(on_setreuid_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_setreuid_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 127 int setregid ['int rgid', 'int egid']
	case 127: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setregid_return_num_cb;
		return_context_count = &ppp_on_setregid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setregid_return)));
		if (save_args || PPP_CHECK_CB(on_setregid_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_setregid_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 128 int rename ['const char *from', 'const char *to']
	case 128: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rename_return_num_cb;
		return_context_count = &ppp_on_rename_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rename_return)));
		if (save_args || PPP_CHECK_CB(on_rename_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_rename_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 131 int flock ['int fd', 'int how']
	case 131: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_flock_return_num_cb;
		return_context_count = &ppp_on_flock_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_flock_return)));
		if (save_args || PPP_CHECK_CB(on_flock_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_flock_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 132 int mkfifo ['const char *path', 'mode_t mode']
	case 132: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mkfifo_return_num_cb;
		return_context_count = &ppp_on_mkfifo_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mkfifo_return)));
		if (save_args || PPP_CHECK_CB(on_mkfifo_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_mkfifo_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 133 int sendto ['int s', 'const void *buf', 'size_t len', 'int flags', 'const struct sockaddr *to', 'int tolen']
	case 133: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sendto_return_num_cb;
		return_context_count = &ppp_on_sendto_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sendto_return)));
		if (save_args || PPP_CHECK_CB(on_sendto_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		int32_t arg5 = get_s32(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sendto_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 134 int shutdown ['int s', 'int how']
	case 134: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_shutdown_return_num_cb;
		return_context_count = &ppp_on_shutdown_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_shutdown_return)));
		if (save_args || PPP_CHECK_CB(on_shutdown_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_shutdown_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 135 int socketpair ['int domain', 'int type', 'int protocol', 'int *rsv']
	case 135: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_socketpair_return_num_cb;
		return_context_count = &ppp_on_socketpair_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_socketpair_return)));
		if (save_args || PPP_CHECK_CB(on_socketpair_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_socketpair_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 136 int mkdir ['const char *path', 'mode_t mode']
	case 136: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mkdir_return_num_cb;
		return_context_count = &ppp_on_mkdir_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mkdir_return)));
		if (save_args || PPP_CHECK_CB(on_mkdir_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_mkdir_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 137 int rmdir ['const char *path']
	case 137: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rmdir_return_num_cb;
		return_context_count = &ppp_on_rmdir_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rmdir_return)));
		if (save_args || PPP_CHECK_CB(on_rmdir_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_rmdir_enter, cpu, pc, arg0);
		}
	}; break;
	// 138 int utimes ['const char *path', 'struct timeval *tptr']
	case 138: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_utimes_return_num_cb;
		return_context_count = &ppp_on_utimes_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_utimes_return)));
		if (save_args || PPP_CHECK_CB(on_utimes_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_utimes_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 140 int adjtime ['struct timeval *delta', 'struct timeval *olddelta']
	case 140: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_adjtime_return_num_cb;
		return_context_count = &ppp_on_adjtime_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_adjtime_return)));
		if (save_args || PPP_CHECK_CB(on_adjtime_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_adjtime_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 142 long gethostid ['void']
	case 142: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_gethostid_return_num_cb;
		return_context_count = &ppp_on_gethostid_return_num_cb_with_context;
		PPP_RUN_CB(on_gethostid_enter, cpu, pc);
	}; break;
	// 143 int sethostid ['long hostid']
	case 143: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sethostid_return_num_cb;
		return_context_count = &ppp_on_sethostid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sethostid_return)));
		if (save_args || PPP_CHECK_CB(on_sethostid_enter)) {
			int64_t arg0 = get_s64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int64_t));
			}
			PPP_RUN_CB(on_sethostid_enter, cpu, pc, arg0);
		}
	}; break;
	// 144, 194 int getrlimit ['unsigned which', 'struct orlimit *rlp'] OR ['unsigned which', 'struct rlimit *rlp']
	case 144: case 194: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getrlimit_return_num_cb;
		return_context_count = &ppp_on_getrlimit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getrlimit_return)));
		if (save_args || PPP_CHECK_CB(on_getrlimit_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getrlimit_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 145, 195 int setrlimit ['unsigned which', 'struct orlimit *rlp'] OR ['unsigned which', 'struct rlimit *rlp']
	case 145: case 195: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setrlimit_return_num_cb;
		return_context_count = &ppp_on_setrlimit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setrlimit_return)));
		if (save_args || PPP_CHECK_CB(on_setrlimit_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_setrlimit_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 146 int killpg ['int pgid', 'int signum']
	case 146: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_killpg_return_num_cb;
		return_context_count = &ppp_on_killpg_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_killpg_return)));
		if (save_args || PPP_CHECK_CB(on_killpg_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_killpg_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 147 int setsid ['void']
	case 147: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setsid_return_num_cb;
		return_context_count = &ppp_on_setsid_return_num_cb_with_context;
		PPP_RUN_CB(on_setsid_enter, cpu, pc);
	}; break;
	// 148 int quotactl ['const char *path', 'int cmd', 'int uid', 'void *arg']
	case 148: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_quotactl_return_num_cb;
		return_context_count = &ppp_on_quotactl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_quotactl_return)));
		if (save_args || PPP_CHECK_CB(on_quotactl_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_quotactl_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 149 int quota ['void']
	case 149: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_quota_return_num_cb;
		return_context_count = &ppp_on_quota_return_num_cb_with_context;
		PPP_RUN_CB(on_quota_enter, cpu, pc);
	}; break;
	// 150 int getsockname ['int fdec', 'struct sockaddr *asa', 'int *alen']
	case 150: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getsockname_return_num_cb;
		return_context_count = &ppp_on_getsockname_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getsockname_return)));
		if (save_args || PPP_CHECK_CB(on_getsockname_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getsockname_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 154 int nlm_syscall ['int debug_level', 'int grace_period', 'int addr_count', 'char **addrs']
	case 154: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nlm_syscall_return_num_cb;
		return_context_count = &ppp_on_nlm_syscall_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_nlm_syscall_return)));
		if (save_args || PPP_CHECK_CB(on_nlm_syscall_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_nlm_syscall_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 155 int nfssvc ['int flag', 'void *argp']
	case 155: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nfssvc_return_num_cb;
		return_context_count = &ppp_on_nfssvc_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_nfssvc_return)));
		if (save_args || PPP_CHECK_CB(on_nfssvc_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_nfssvc_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 156, 196, 554 int getdirentries ['int fd', 'char *buf', 'unsigned count', 'long *basep'] OR ['int fd', 'char *buf', 'size_t count', 'off_t *basep']
	case 156: case 196: case 554: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getdirentries_return_num_cb;
		return_context_count = &ppp_on_getdirentries_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getdirentries_return)));
		if (save_args || PPP_CHECK_CB(on_getdirentries_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getdirentries_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 157, 396, 555 int statfs ['const char *path', 'struct ostatfs *buf'] OR ['const char *path', 'struct freebsd11_statfs *buf'] OR ['const char *path', 'struct statfs *buf']
	case 157: case 396: case 555: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_statfs_return_num_cb;
		return_context_count = &ppp_on_statfs_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_statfs_return)));
		if (save_args || PPP_CHECK_CB(on_statfs_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_statfs_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 158, 397, 556 int fstatfs ['int fd', 'struct ostatfs *buf'] OR ['int fd', 'struct freebsd11_statfs *buf'] OR ['int fd', 'struct statfs *buf']
	case 158: case 397: case 556: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fstatfs_return_num_cb;
		return_context_count = &ppp_on_fstatfs_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fstatfs_return)));
		if (save_args || PPP_CHECK_CB(on_fstatfs_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_fstatfs_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 160 int lgetfh ['const char *fname', 'struct fhandle *fhp']
	case 160: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lgetfh_return_num_cb;
		return_context_count = &ppp_on_lgetfh_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lgetfh_return)));
		if (save_args || PPP_CHECK_CB(on_lgetfh_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_lgetfh_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 161 int getfh ['const char *fname', 'struct fhandle *fhp']
	case 161: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getfh_return_num_cb;
		return_context_count = &ppp_on_getfh_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getfh_return)));
		if (save_args || PPP_CHECK_CB(on_getfh_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getfh_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 162 int getdomainname ['char *domainname', 'int len']
	case 162: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getdomainname_return_num_cb;
		return_context_count = &ppp_on_getdomainname_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getdomainname_return)));
		if (save_args || PPP_CHECK_CB(on_getdomainname_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_getdomainname_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 163 int setdomainname ['char *domainname', 'int len']
	case 163: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setdomainname_return_num_cb;
		return_context_count = &ppp_on_setdomainname_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setdomainname_return)));
		if (save_args || PPP_CHECK_CB(on_setdomainname_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_setdomainname_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 164 int uname ['struct utsname *name']
	case 164: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_uname_return_num_cb;
		return_context_count = &ppp_on_uname_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_uname_return)));
		if (save_args || PPP_CHECK_CB(on_uname_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_uname_enter, cpu, pc, arg0);
		}
	}; break;
	// 165 int sysarch ['int op', 'char *parms']
	case 165: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sysarch_return_num_cb;
		return_context_count = &ppp_on_sysarch_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sysarch_return)));
		if (save_args || PPP_CHECK_CB(on_sysarch_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sysarch_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 166 int rtprio ['int function', 'pid_t pid', 'struct rtprio *rtp']
	case 166: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rtprio_return_num_cb;
		return_context_count = &ppp_on_rtprio_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rtprio_return)));
		if (save_args || PPP_CHECK_CB(on_rtprio_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_rtprio_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 169 int semsys ['int which', 'int a2', 'int a3', 'int a4', 'int a5']
	case 169: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_semsys_return_num_cb;
		return_context_count = &ppp_on_semsys_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_semsys_return)));
		if (save_args || PPP_CHECK_CB(on_semsys_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
		int32_t arg4 = get_s32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
				memcpy(ctx.args[4], &arg4, sizeof(int32_t));
			}
			PPP_RUN_CB(on_semsys_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 175 int setfib ['int fibnum']
	case 175: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setfib_return_num_cb;
		return_context_count = &ppp_on_setfib_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setfib_return)));
		if (save_args || PPP_CHECK_CB(on_setfib_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_setfib_enter, cpu, pc, arg0);
		}
	}; break;
	// 176 int ntp_adjtime ['struct timex *tp']
	case 176: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ntp_adjtime_return_num_cb;
		return_context_count = &ppp_on_ntp_adjtime_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ntp_adjtime_return)));
		if (save_args || PPP_CHECK_CB(on_ntp_adjtime_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ntp_adjtime_enter, cpu, pc, arg0);
		}
	}; break;
	// 181 int setgid ['gid_t gid']
	case 181: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setgid_return_num_cb;
		return_context_count = &ppp_on_setgid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setgid_return)));
		if (save_args || PPP_CHECK_CB(on_setgid_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_setgid_enter, cpu, pc, arg0);
		}
	}; break;
	// 182 int setegid ['gid_t egid']
	case 182: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setegid_return_num_cb;
		return_context_count = &ppp_on_setegid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setegid_return)));
		if (save_args || PPP_CHECK_CB(on_setegid_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_setegid_enter, cpu, pc, arg0);
		}
	}; break;
	// 183 int seteuid ['uid_t euid']
	case 183: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_seteuid_return_num_cb;
		return_context_count = &ppp_on_seteuid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_seteuid_return)));
		if (save_args || PPP_CHECK_CB(on_seteuid_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_seteuid_enter, cpu, pc, arg0);
		}
	}; break;
	// 191 int pathconf ['const char *path', 'int name']
	case 191: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pathconf_return_num_cb;
		return_context_count = &ppp_on_pathconf_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pathconf_return)));
		if (save_args || PPP_CHECK_CB(on_pathconf_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_pathconf_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 192 int fpathconf ['int fd', 'int name']
	case 192: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fpathconf_return_num_cb;
		return_context_count = &ppp_on_fpathconf_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fpathconf_return)));
		if (save_args || PPP_CHECK_CB(on_fpathconf_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fpathconf_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 202 int __sysctl ['int *name', 'unsigned namelen', 'void *old', 'size_t *oldlenp', 'const void *new', 'size_t newlen']
	case 202: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___sysctl_return_num_cb;
		return_context_count = &ppp_on___sysctl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___sysctl_return)));
		if (save_args || PPP_CHECK_CB(on___sysctl_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		uint32_t arg5 = get_32(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint32_t));
			}
			PPP_RUN_CB(on___sysctl_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 203 int mlock ['const void *addr', 'size_t len']
	case 203: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mlock_return_num_cb;
		return_context_count = &ppp_on_mlock_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mlock_return)));
		if (save_args || PPP_CHECK_CB(on_mlock_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_mlock_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 204 int munlock ['const void *addr', 'size_t len']
	case 204: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_munlock_return_num_cb;
		return_context_count = &ppp_on_munlock_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_munlock_return)));
		if (save_args || PPP_CHECK_CB(on_munlock_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_munlock_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 205 int undelete ['const char *path']
	case 205: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_undelete_return_num_cb;
		return_context_count = &ppp_on_undelete_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_undelete_return)));
		if (save_args || PPP_CHECK_CB(on_undelete_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_undelete_enter, cpu, pc, arg0);
		}
	}; break;
	// 206 int futimes ['int fd', 'struct timeval *tptr']
	case 206: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_futimes_return_num_cb;
		return_context_count = &ppp_on_futimes_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_futimes_return)));
		if (save_args || PPP_CHECK_CB(on_futimes_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_futimes_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 207 int getpgid ['pid_t pid']
	case 207: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getpgid_return_num_cb;
		return_context_count = &ppp_on_getpgid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getpgid_return)));
		if (save_args || PPP_CHECK_CB(on_getpgid_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_getpgid_enter, cpu, pc, arg0);
		}
	}; break;
	// 209 int poll ['struct pollfd *fds', 'unsigned nfds', 'int timeout']
	case 209: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_poll_return_num_cb;
		return_context_count = &ppp_on_poll_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_poll_return)));
		if (save_args || PPP_CHECK_CB(on_poll_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_poll_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 220, 510 int __semctl ['int semid', 'int semnum', 'int cmd', 'union semun_old *arg'] OR ['int semid', 'int semnum', 'int cmd', 'union semun *arg']
	case 220: case 510: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___semctl_return_num_cb;
		return_context_count = &ppp_on___semctl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___semctl_return)));
		if (save_args || PPP_CHECK_CB(on___semctl_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___semctl_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 221 int semget ['key_t key', 'int nsems', 'int semflg']
	case 221: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_semget_return_num_cb;
		return_context_count = &ppp_on_semget_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_semget_return)));
		if (save_args || PPP_CHECK_CB(on_semget_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_semget_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 222 int semop ['int semid', 'struct sembuf *sops', 'size_t nsops']
	case 222: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_semop_return_num_cb;
		return_context_count = &ppp_on_semop_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_semop_return)));
		if (save_args || PPP_CHECK_CB(on_semop_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_semop_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 224, 511 int msgctl ['int msqid', 'int cmd', 'struct msqid_ds_old *buf'] OR ['int msqid', 'int cmd', 'struct msqid_ds *buf']
	case 224: case 511: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_msgctl_return_num_cb;
		return_context_count = &ppp_on_msgctl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_msgctl_return)));
		if (save_args || PPP_CHECK_CB(on_msgctl_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_msgctl_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 225 int msgget ['key_t key', 'int msgflg']
	case 225: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_msgget_return_num_cb;
		return_context_count = &ppp_on_msgget_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_msgget_return)));
		if (save_args || PPP_CHECK_CB(on_msgget_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_msgget_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 226 int msgsnd ['int msqid', 'const void *msgp', 'size_t msgsz', 'int msgflg']
	case 226: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_msgsnd_return_num_cb;
		return_context_count = &ppp_on_msgsnd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_msgsnd_return)));
		if (save_args || PPP_CHECK_CB(on_msgsnd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_msgsnd_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 227 ssize_t msgrcv ['int msqid', 'void *msgp', 'size_t msgsz', 'long msgtyp', 'int msgflg']
	case 227: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_msgrcv_return_num_cb;
		return_context_count = &ppp_on_msgrcv_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_msgrcv_return)));
		if (save_args || PPP_CHECK_CB(on_msgrcv_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		int64_t arg3 = get_s64(cpu, &ctx, 3);
		int32_t arg4 = get_s32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int64_t));
				memcpy(ctx.args[4], &arg4, sizeof(int32_t));
			}
			PPP_RUN_CB(on_msgrcv_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 229, 512 int shmctl ['int shmid', 'int cmd', 'struct shmid_ds_old *buf'] OR ['int shmid', 'int cmd', 'struct shmid_ds *buf']
	case 229: case 512: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_shmctl_return_num_cb;
		return_context_count = &ppp_on_shmctl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_shmctl_return)));
		if (save_args || PPP_CHECK_CB(on_shmctl_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_shmctl_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 230 int shmdt ['const void *shmaddr']
	case 230: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_shmdt_return_num_cb;
		return_context_count = &ppp_on_shmdt_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_shmdt_return)));
		if (save_args || PPP_CHECK_CB(on_shmdt_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_shmdt_enter, cpu, pc, arg0);
		}
	}; break;
	// 231 int shmget ['key_t key', 'size_t size', 'int shmflg']
	case 231: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_shmget_return_num_cb;
		return_context_count = &ppp_on_shmget_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_shmget_return)));
		if (save_args || PPP_CHECK_CB(on_shmget_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_shmget_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 232 int clock_gettime ['clockid_t clock_id', 'struct timespec *tp']
	case 232: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_clock_gettime_return_num_cb;
		return_context_count = &ppp_on_clock_gettime_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_clock_gettime_return)));
		if (save_args || PPP_CHECK_CB(on_clock_gettime_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_clock_gettime_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 233 int clock_settime ['clockid_t clock_id', 'const struct timespec *tp']
	case 233: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_clock_settime_return_num_cb;
		return_context_count = &ppp_on_clock_settime_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_clock_settime_return)));
		if (save_args || PPP_CHECK_CB(on_clock_settime_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_clock_settime_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 234 int clock_getres ['clockid_t clock_id', 'struct timespec *tp']
	case 234: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_clock_getres_return_num_cb;
		return_context_count = &ppp_on_clock_getres_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_clock_getres_return)));
		if (save_args || PPP_CHECK_CB(on_clock_getres_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_clock_getres_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 235 int ktimer_create ['clockid_t clock_id', 'struct sigevent *evp', 'int *timerid']
	case 235: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ktimer_create_return_num_cb;
		return_context_count = &ppp_on_ktimer_create_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ktimer_create_return)));
		if (save_args || PPP_CHECK_CB(on_ktimer_create_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ktimer_create_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 236 int ktimer_delete ['int timerid']
	case 236: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ktimer_delete_return_num_cb;
		return_context_count = &ppp_on_ktimer_delete_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ktimer_delete_return)));
		if (save_args || PPP_CHECK_CB(on_ktimer_delete_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_ktimer_delete_enter, cpu, pc, arg0);
		}
	}; break;
	// 237 int ktimer_settime ['int timerid', 'int flags', 'const struct itimerspec *value', 'struct itimerspec *ovalue']
	case 237: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ktimer_settime_return_num_cb;
		return_context_count = &ppp_on_ktimer_settime_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ktimer_settime_return)));
		if (save_args || PPP_CHECK_CB(on_ktimer_settime_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ktimer_settime_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 238 int ktimer_gettime ['int timerid', 'struct itimerspec *value']
	case 238: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ktimer_gettime_return_num_cb;
		return_context_count = &ppp_on_ktimer_gettime_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ktimer_gettime_return)));
		if (save_args || PPP_CHECK_CB(on_ktimer_gettime_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ktimer_gettime_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 239 int ktimer_getoverrun ['int timerid']
	case 239: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ktimer_getoverrun_return_num_cb;
		return_context_count = &ppp_on_ktimer_getoverrun_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ktimer_getoverrun_return)));
		if (save_args || PPP_CHECK_CB(on_ktimer_getoverrun_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_ktimer_getoverrun_enter, cpu, pc, arg0);
		}
	}; break;
	// 240 int nanosleep ['const struct timespec *rqtp', 'struct timespec *rmtp']
	case 240: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nanosleep_return_num_cb;
		return_context_count = &ppp_on_nanosleep_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_nanosleep_return)));
		if (save_args || PPP_CHECK_CB(on_nanosleep_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_nanosleep_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 241 int ffclock_getcounter ['ffcounter *ffcount']
	case 241: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ffclock_getcounter_return_num_cb;
		return_context_count = &ppp_on_ffclock_getcounter_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ffclock_getcounter_return)));
		if (save_args || PPP_CHECK_CB(on_ffclock_getcounter_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ffclock_getcounter_enter, cpu, pc, arg0);
		}
	}; break;
	// 242 int ffclock_setestimate ['struct ffclock_estimate *cest']
	case 242: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ffclock_setestimate_return_num_cb;
		return_context_count = &ppp_on_ffclock_setestimate_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ffclock_setestimate_return)));
		if (save_args || PPP_CHECK_CB(on_ffclock_setestimate_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ffclock_setestimate_enter, cpu, pc, arg0);
		}
	}; break;
	// 243 int ffclock_getestimate ['struct ffclock_estimate *cest']
	case 243: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ffclock_getestimate_return_num_cb;
		return_context_count = &ppp_on_ffclock_getestimate_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ffclock_getestimate_return)));
		if (save_args || PPP_CHECK_CB(on_ffclock_getestimate_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ffclock_getestimate_enter, cpu, pc, arg0);
		}
	}; break;
	// 244 int clock_nanosleep ['clockid_t clock_id', 'int flags', 'const struct timespec *rqtp', 'struct timespec *rmtp']
	case 244: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_clock_nanosleep_return_num_cb;
		return_context_count = &ppp_on_clock_nanosleep_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_clock_nanosleep_return)));
		if (save_args || PPP_CHECK_CB(on_clock_nanosleep_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_clock_nanosleep_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 247 int clock_getcpuclockid2 ['id_t id', 'int which', 'clockid_t *clock_id']
	case 247: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_clock_getcpuclockid2_return_num_cb;
		return_context_count = &ppp_on_clock_getcpuclockid2_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_clock_getcpuclockid2_return)));
		if (save_args || PPP_CHECK_CB(on_clock_getcpuclockid2_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_clock_getcpuclockid2_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 248 int ntp_gettime ['struct ntptimeval *ntvp']
	case 248: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ntp_gettime_return_num_cb;
		return_context_count = &ppp_on_ntp_gettime_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ntp_gettime_return)));
		if (save_args || PPP_CHECK_CB(on_ntp_gettime_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ntp_gettime_enter, cpu, pc, arg0);
		}
	}; break;
	// 250 int minherit ['void *addr', 'size_t len', 'int inherit']
	case 250: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_minherit_return_num_cb;
		return_context_count = &ppp_on_minherit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_minherit_return)));
		if (save_args || PPP_CHECK_CB(on_minherit_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_minherit_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 251 int rfork ['int flags']
	case 251: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rfork_return_num_cb;
		return_context_count = &ppp_on_rfork_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rfork_return)));
		if (save_args || PPP_CHECK_CB(on_rfork_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_rfork_enter, cpu, pc, arg0);
		}
	}; break;
	// 253 int issetugid ['void']
	case 253: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_issetugid_return_num_cb;
		return_context_count = &ppp_on_issetugid_return_num_cb_with_context;
		PPP_RUN_CB(on_issetugid_enter, cpu, pc);
	}; break;
	// 254 int lchown ['const char *path', 'int uid', 'int gid']
	case 254: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lchown_return_num_cb;
		return_context_count = &ppp_on_lchown_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lchown_return)));
		if (save_args || PPP_CHECK_CB(on_lchown_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_lchown_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 255, 318 int aio_read ['struct aiocb *aiocbp'] OR ['struct oaiocb *aiocbp']
	case 255: case 318: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_read_return_num_cb;
		return_context_count = &ppp_on_aio_read_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_read_return)));
		if (save_args || PPP_CHECK_CB(on_aio_read_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_read_enter, cpu, pc, arg0);
		}
	}; break;
	// 256, 319 int aio_write ['struct aiocb *aiocbp'] OR ['struct oaiocb *aiocbp']
	case 256: case 319: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_write_return_num_cb;
		return_context_count = &ppp_on_aio_write_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_write_return)));
		if (save_args || PPP_CHECK_CB(on_aio_write_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_write_enter, cpu, pc, arg0);
		}
	}; break;
	// 257, 320 int lio_listio ['int mode', 'struct aiocb * const *acb_list', 'int nent', 'struct sigevent *sig'] OR ['int mode', 'struct oaiocb * const *acb_list', 'int nent', 'struct osigevent *sig']
	case 257: case 320: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lio_listio_return_num_cb;
		return_context_count = &ppp_on_lio_listio_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lio_listio_return)));
		if (save_args || PPP_CHECK_CB(on_lio_listio_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_lio_listio_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 272 int getdents ['int fd', 'char *buf', 'size_t count']
	case 272: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getdents_return_num_cb;
		return_context_count = &ppp_on_getdents_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getdents_return)));
		if (save_args || PPP_CHECK_CB(on_getdents_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_getdents_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 274 int lchmod ['const char *path', 'mode_t mode']
	case 274: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lchmod_return_num_cb;
		return_context_count = &ppp_on_lchmod_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lchmod_return)));
		if (save_args || PPP_CHECK_CB(on_lchmod_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_lchmod_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 276 int lutimes ['const char *path', 'struct timeval *tptr']
	case 276: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lutimes_return_num_cb;
		return_context_count = &ppp_on_lutimes_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lutimes_return)));
		if (save_args || PPP_CHECK_CB(on_lutimes_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_lutimes_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 278 int nstat ['const char *path', 'struct nstat *ub']
	case 278: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nstat_return_num_cb;
		return_context_count = &ppp_on_nstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_nstat_return)));
		if (save_args || PPP_CHECK_CB(on_nstat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_nstat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 279 int nfstat ['int fd', 'struct nstat *sb']
	case 279: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nfstat_return_num_cb;
		return_context_count = &ppp_on_nfstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_nfstat_return)));
		if (save_args || PPP_CHECK_CB(on_nfstat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_nfstat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 280 int nlstat ['const char *path', 'struct nstat *ub']
	case 280: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nlstat_return_num_cb;
		return_context_count = &ppp_on_nlstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_nlstat_return)));
		if (save_args || PPP_CHECK_CB(on_nlstat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_nlstat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 289 ssize_t preadv ['int fd', 'struct iovec *iovp', 'unsigned iovcnt', 'off_t offset']
	case 289: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_preadv_return_num_cb;
		return_context_count = &ppp_on_preadv_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_preadv_return)));
		if (save_args || PPP_CHECK_CB(on_preadv_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_preadv_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 290 ssize_t pwritev ['int fd', 'struct iovec *iovp', 'unsigned iovcnt', 'off_t offset']
	case 290: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pwritev_return_num_cb;
		return_context_count = &ppp_on_pwritev_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pwritev_return)));
		if (save_args || PPP_CHECK_CB(on_pwritev_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_pwritev_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 297, 398, 558 int fhstatfs ['const struct fhandle *u_fhp', 'struct ostatfs *buf'] OR ['const struct fhandle *u_fhp', 'struct freebsd11_statfs *buf'] OR ['const struct fhandle *u_fhp', 'struct statfs *buf']
	case 297: case 398: case 558: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fhstatfs_return_num_cb;
		return_context_count = &ppp_on_fhstatfs_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fhstatfs_return)));
		if (save_args || PPP_CHECK_CB(on_fhstatfs_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_fhstatfs_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 298 int fhopen ['const struct fhandle *u_fhp', 'int flags']
	case 298: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fhopen_return_num_cb;
		return_context_count = &ppp_on_fhopen_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fhopen_return)));
		if (save_args || PPP_CHECK_CB(on_fhopen_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fhopen_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 299, 553 int fhstat ['const struct fhandle *u_fhp', 'struct freebsd11_stat *sb'] OR ['const struct fhandle *u_fhp', 'struct stat *sb']
	case 299: case 553: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fhstat_return_num_cb;
		return_context_count = &ppp_on_fhstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fhstat_return)));
		if (save_args || PPP_CHECK_CB(on_fhstat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_fhstat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 300 int modnext ['int modid']
	case 300: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_modnext_return_num_cb;
		return_context_count = &ppp_on_modnext_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_modnext_return)));
		if (save_args || PPP_CHECK_CB(on_modnext_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_modnext_enter, cpu, pc, arg0);
		}
	}; break;
	// 301 int modstat ['int modid', 'struct module_stat *stat']
	case 301: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_modstat_return_num_cb;
		return_context_count = &ppp_on_modstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_modstat_return)));
		if (save_args || PPP_CHECK_CB(on_modstat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_modstat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 302 int modfnext ['int modid']
	case 302: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_modfnext_return_num_cb;
		return_context_count = &ppp_on_modfnext_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_modfnext_return)));
		if (save_args || PPP_CHECK_CB(on_modfnext_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_modfnext_enter, cpu, pc, arg0);
		}
	}; break;
	// 303 int modfind ['const char *name']
	case 303: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_modfind_return_num_cb;
		return_context_count = &ppp_on_modfind_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_modfind_return)));
		if (save_args || PPP_CHECK_CB(on_modfind_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_modfind_enter, cpu, pc, arg0);
		}
	}; break;
	// 304 int kldload ['const char *file']
	case 304: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kldload_return_num_cb;
		return_context_count = &ppp_on_kldload_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kldload_return)));
		if (save_args || PPP_CHECK_CB(on_kldload_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kldload_enter, cpu, pc, arg0);
		}
	}; break;
	// 305 int kldunload ['int fileid']
	case 305: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kldunload_return_num_cb;
		return_context_count = &ppp_on_kldunload_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kldunload_return)));
		if (save_args || PPP_CHECK_CB(on_kldunload_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_kldunload_enter, cpu, pc, arg0);
		}
	}; break;
	// 306 int kldfind ['const char *file']
	case 306: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kldfind_return_num_cb;
		return_context_count = &ppp_on_kldfind_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kldfind_return)));
		if (save_args || PPP_CHECK_CB(on_kldfind_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kldfind_enter, cpu, pc, arg0);
		}
	}; break;
	// 307 int kldnext ['int fileid']
	case 307: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kldnext_return_num_cb;
		return_context_count = &ppp_on_kldnext_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kldnext_return)));
		if (save_args || PPP_CHECK_CB(on_kldnext_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_kldnext_enter, cpu, pc, arg0);
		}
	}; break;
	// 308 int kldstat ['int fileid', 'struct kld_file_stat *stat']
	case 308: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kldstat_return_num_cb;
		return_context_count = &ppp_on_kldstat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kldstat_return)));
		if (save_args || PPP_CHECK_CB(on_kldstat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kldstat_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 309 int kldfirstmod ['int fileid']
	case 309: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kldfirstmod_return_num_cb;
		return_context_count = &ppp_on_kldfirstmod_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kldfirstmod_return)));
		if (save_args || PPP_CHECK_CB(on_kldfirstmod_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_kldfirstmod_enter, cpu, pc, arg0);
		}
	}; break;
	// 310 int getsid ['pid_t pid']
	case 310: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getsid_return_num_cb;
		return_context_count = &ppp_on_getsid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getsid_return)));
		if (save_args || PPP_CHECK_CB(on_getsid_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_getsid_enter, cpu, pc, arg0);
		}
	}; break;
	// 311 int setresuid ['uid_t ruid', 'uid_t euid', 'uid_t suid']
	case 311: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setresuid_return_num_cb;
		return_context_count = &ppp_on_setresuid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setresuid_return)));
		if (save_args || PPP_CHECK_CB(on_setresuid_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_setresuid_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 312 int setresgid ['gid_t rgid', 'gid_t egid', 'gid_t sgid']
	case 312: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setresgid_return_num_cb;
		return_context_count = &ppp_on_setresgid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setresgid_return)));
		if (save_args || PPP_CHECK_CB(on_setresgid_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_setresgid_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 314 ssize_t aio_return ['struct aiocb *aiocbp']
	case 314: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_return_return_num_cb;
		return_context_count = &ppp_on_aio_return_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_return_return)));
		if (save_args || PPP_CHECK_CB(on_aio_return_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_return_enter, cpu, pc, arg0);
		}
	}; break;
	// 315 int aio_suspend ['struct aiocb * const * aiocbp', 'int nent', 'const struct timespec *timeout']
	case 315: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_suspend_return_num_cb;
		return_context_count = &ppp_on_aio_suspend_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_suspend_return)));
		if (save_args || PPP_CHECK_CB(on_aio_suspend_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_suspend_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 316 int aio_cancel ['int fd', 'struct aiocb *aiocbp']
	case 316: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_cancel_return_num_cb;
		return_context_count = &ppp_on_aio_cancel_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_cancel_return)));
		if (save_args || PPP_CHECK_CB(on_aio_cancel_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_cancel_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 317 int aio_error ['struct aiocb *aiocbp']
	case 317: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_error_return_num_cb;
		return_context_count = &ppp_on_aio_error_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_error_return)));
		if (save_args || PPP_CHECK_CB(on_aio_error_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_error_enter, cpu, pc, arg0);
		}
	}; break;
	// 321 int yield ['void']
	case 321: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_yield_return_num_cb;
		return_context_count = &ppp_on_yield_return_num_cb_with_context;
		PPP_RUN_CB(on_yield_enter, cpu, pc);
	}; break;
	// 324 int mlockall ['int how']
	case 324: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mlockall_return_num_cb;
		return_context_count = &ppp_on_mlockall_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mlockall_return)));
		if (save_args || PPP_CHECK_CB(on_mlockall_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_mlockall_enter, cpu, pc, arg0);
		}
	}; break;
	// 325 int munlockall(void); 326 int __getcwd ['char *buf', 'size_t buflen']
	case 325: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___getcwd_return_num_cb;
		return_context_count = &ppp_on___getcwd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___getcwd_return)));
		if (save_args || PPP_CHECK_CB(on___getcwd_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on___getcwd_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 327 int sched_setparam ['pid_t pid', 'const struct sched_param *param']
	case 327: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sched_setparam_return_num_cb;
		return_context_count = &ppp_on_sched_setparam_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sched_setparam_return)));
		if (save_args || PPP_CHECK_CB(on_sched_setparam_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sched_setparam_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 328 int sched_getparam ['pid_t pid', 'struct sched_param *param']
	case 328: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sched_getparam_return_num_cb;
		return_context_count = &ppp_on_sched_getparam_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sched_getparam_return)));
		if (save_args || PPP_CHECK_CB(on_sched_getparam_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sched_getparam_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 329 int sched_setscheduler ['pid_t pid', 'int policy', 'const struct sched_param *param']
	case 329: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sched_setscheduler_return_num_cb;
		return_context_count = &ppp_on_sched_setscheduler_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sched_setscheduler_return)));
		if (save_args || PPP_CHECK_CB(on_sched_setscheduler_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sched_setscheduler_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 330 int sched_getscheduler ['pid_t pid']
	case 330: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sched_getscheduler_return_num_cb;
		return_context_count = &ppp_on_sched_getscheduler_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sched_getscheduler_return)));
		if (save_args || PPP_CHECK_CB(on_sched_getscheduler_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sched_getscheduler_enter, cpu, pc, arg0);
		}
	}; break;
	// 331 int sched_yield ['void']
	case 331: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sched_yield_return_num_cb;
		return_context_count = &ppp_on_sched_yield_return_num_cb_with_context;
		PPP_RUN_CB(on_sched_yield_enter, cpu, pc);
	}; break;
	// 332 int sched_get_priority_max ['int policy']
	case 332: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sched_get_priority_max_return_num_cb;
		return_context_count = &ppp_on_sched_get_priority_max_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sched_get_priority_max_return)));
		if (save_args || PPP_CHECK_CB(on_sched_get_priority_max_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sched_get_priority_max_enter, cpu, pc, arg0);
		}
	}; break;
	// 333 int sched_get_priority_min ['int policy']
	case 333: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sched_get_priority_min_return_num_cb;
		return_context_count = &ppp_on_sched_get_priority_min_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sched_get_priority_min_return)));
		if (save_args || PPP_CHECK_CB(on_sched_get_priority_min_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sched_get_priority_min_enter, cpu, pc, arg0);
		}
	}; break;
	// 334 int sched_rr_get_interval ['pid_t pid', 'struct timespec *interval']
	case 334: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sched_rr_get_interval_return_num_cb;
		return_context_count = &ppp_on_sched_rr_get_interval_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sched_rr_get_interval_return)));
		if (save_args || PPP_CHECK_CB(on_sched_rr_get_interval_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sched_rr_get_interval_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 335 int utrace ['const void *addr', 'size_t len']
	case 335: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_utrace_return_num_cb;
		return_context_count = &ppp_on_utrace_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_utrace_return)));
		if (save_args || PPP_CHECK_CB(on_utrace_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_utrace_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 336, 393 int sendfile ['int fd', 'int s', 'off_t offset', 'size_t nbytes', 'struct sf_hdtr *hdtr', 'off_t *sbytes', 'int flags']
	case 336: case 393: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sendfile_return_num_cb;
		return_context_count = &ppp_on_sendfile_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sendfile_return)));
		if (save_args || PPP_CHECK_CB(on_sendfile_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
		int32_t arg6 = get_s32(cpu, &ctx, 6);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
				memcpy(ctx.args[6], &arg6, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sendfile_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
		}
	}; break;
	// 337 int kldsym ['int fileid', 'int cmd', 'void *data']
	case 337: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kldsym_return_num_cb;
		return_context_count = &ppp_on_kldsym_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kldsym_return)));
		if (save_args || PPP_CHECK_CB(on_kldsym_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kldsym_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 338 int jail ['struct jail *jail']
	case 338: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_jail_return_num_cb;
		return_context_count = &ppp_on_jail_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_jail_return)));
		if (save_args || PPP_CHECK_CB(on_jail_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_jail_enter, cpu, pc, arg0);
		}
	}; break;
	// 339 int nnpfs_syscall ['int operation', 'char *a_pathP', 'int a_opcode', 'void *a_paramsP', 'int a_followSymlinks']
	case 339: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nnpfs_syscall_return_num_cb;
		return_context_count = &ppp_on_nnpfs_syscall_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_nnpfs_syscall_return)));
		if (save_args || PPP_CHECK_CB(on_nnpfs_syscall_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		int32_t arg4 = get_s32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(int32_t));
			}
			PPP_RUN_CB(on_nnpfs_syscall_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 340 int sigprocmask ['int how', 'const sigset_t *set', 'sigset_t *oset']
	case 340: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigprocmask_return_num_cb;
		return_context_count = &ppp_on_sigprocmask_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigprocmask_return)));
		if (save_args || PPP_CHECK_CB(on_sigprocmask_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigprocmask_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 341 int sigsuspend ['const sigset_t *sigmask']
	case 341: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigsuspend_return_num_cb;
		return_context_count = &ppp_on_sigsuspend_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigsuspend_return)));
		if (save_args || PPP_CHECK_CB(on_sigsuspend_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigsuspend_enter, cpu, pc, arg0);
		}
	}; break;
	// 342, 416 int sigaction ['int sig', 'const struct sigaction *act', 'struct sigaction *oact']
	case 342: case 416: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigaction_return_num_cb;
		return_context_count = &ppp_on_sigaction_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigaction_return)));
		if (save_args || PPP_CHECK_CB(on_sigaction_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigaction_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 343 int sigpending ['sigset_t *set']
	case 343: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigpending_return_num_cb;
		return_context_count = &ppp_on_sigpending_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigpending_return)));
		if (save_args || PPP_CHECK_CB(on_sigpending_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigpending_enter, cpu, pc, arg0);
		}
	}; break;
	// 345 int sigtimedwait ['const sigset_t *set', 'siginfo_t *info', 'const struct timespec *timeout']
	case 345: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigtimedwait_return_num_cb;
		return_context_count = &ppp_on_sigtimedwait_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigtimedwait_return)));
		if (save_args || PPP_CHECK_CB(on_sigtimedwait_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigtimedwait_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 346 int sigwaitinfo ['const sigset_t *set', 'siginfo_t *info']
	case 346: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigwaitinfo_return_num_cb;
		return_context_count = &ppp_on_sigwaitinfo_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigwaitinfo_return)));
		if (save_args || PPP_CHECK_CB(on_sigwaitinfo_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigwaitinfo_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 347 int __acl_get_file ['const char *path', 'acl_type_t type', 'struct acl *aclp']
	case 347: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_get_file_return_num_cb;
		return_context_count = &ppp_on___acl_get_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_get_file_return)));
		if (save_args || PPP_CHECK_CB(on___acl_get_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_get_file_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 348 int __acl_set_file ['const char *path', 'acl_type_t type', 'struct acl *aclp']
	case 348: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_set_file_return_num_cb;
		return_context_count = &ppp_on___acl_set_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_set_file_return)));
		if (save_args || PPP_CHECK_CB(on___acl_set_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_set_file_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 349 int __acl_get_fd ['int filedes', 'acl_type_t type', 'struct acl *aclp']
	case 349: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_get_fd_return_num_cb;
		return_context_count = &ppp_on___acl_get_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_get_fd_return)));
		if (save_args || PPP_CHECK_CB(on___acl_get_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_get_fd_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 350 int __acl_set_fd ['int filedes', 'acl_type_t type', 'struct acl *aclp']
	case 350: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_set_fd_return_num_cb;
		return_context_count = &ppp_on___acl_set_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_set_fd_return)));
		if (save_args || PPP_CHECK_CB(on___acl_set_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_set_fd_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 351 int __acl_delete_file ['const char *path', 'acl_type_t type']
	case 351: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_delete_file_return_num_cb;
		return_context_count = &ppp_on___acl_delete_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_delete_file_return)));
		if (save_args || PPP_CHECK_CB(on___acl_delete_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on___acl_delete_file_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 352 int __acl_delete_fd ['int filedes', 'acl_type_t type']
	case 352: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_delete_fd_return_num_cb;
		return_context_count = &ppp_on___acl_delete_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_delete_fd_return)));
		if (save_args || PPP_CHECK_CB(on___acl_delete_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on___acl_delete_fd_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 353 int __acl_aclcheck_file ['const char *path', 'acl_type_t type', 'struct acl *aclp']
	case 353: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_aclcheck_file_return_num_cb;
		return_context_count = &ppp_on___acl_aclcheck_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_aclcheck_file_return)));
		if (save_args || PPP_CHECK_CB(on___acl_aclcheck_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_aclcheck_file_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 354 int __acl_aclcheck_fd ['int filedes', 'acl_type_t type', 'struct acl *aclp']
	case 354: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_aclcheck_fd_return_num_cb;
		return_context_count = &ppp_on___acl_aclcheck_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_aclcheck_fd_return)));
		if (save_args || PPP_CHECK_CB(on___acl_aclcheck_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_aclcheck_fd_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 355 int extattrctl ['const char *path', 'int cmd', 'const char *filename', 'int attrnamespace', 'const char *attrname']
	case 355: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattrctl_return_num_cb;
		return_context_count = &ppp_on_extattrctl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattrctl_return)));
		if (save_args || PPP_CHECK_CB(on_extattrctl_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_extattrctl_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 356 ssize_t extattr_set_file ['const char *path', 'int attrnamespace', 'const char *attrname', 'void *data', 'size_t nbytes']
	case 356: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_set_file_return_num_cb;
		return_context_count = &ppp_on_extattr_set_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_set_file_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_set_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_set_file_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 357 ssize_t extattr_get_file ['const char *path', 'int attrnamespace', 'const char *attrname', 'void *data', 'size_t nbytes']
	case 357: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_get_file_return_num_cb;
		return_context_count = &ppp_on_extattr_get_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_get_file_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_get_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_get_file_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 358 int extattr_delete_file ['const char *path', 'int attrnamespace', 'const char *attrname']
	case 358: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_delete_file_return_num_cb;
		return_context_count = &ppp_on_extattr_delete_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_delete_file_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_delete_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_extattr_delete_file_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 359 ssize_t aio_waitcomplete ['struct aiocb **aiocbp', 'struct timespec *timeout']
	case 359: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_waitcomplete_return_num_cb;
		return_context_count = &ppp_on_aio_waitcomplete_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_waitcomplete_return)));
		if (save_args || PPP_CHECK_CB(on_aio_waitcomplete_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_waitcomplete_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 360 int getresuid ['uid_t *ruid', 'uid_t *euid', 'uid_t *suid']
	case 360: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getresuid_return_num_cb;
		return_context_count = &ppp_on_getresuid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getresuid_return)));
		if (save_args || PPP_CHECK_CB(on_getresuid_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getresuid_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 361 int getresgid ['gid_t *rgid', 'gid_t *egid', 'gid_t *sgid']
	case 361: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getresgid_return_num_cb;
		return_context_count = &ppp_on_getresgid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getresgid_return)));
		if (save_args || PPP_CHECK_CB(on_getresgid_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getresgid_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 362 int kqueue ['void']
	case 362: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kqueue_return_num_cb;
		return_context_count = &ppp_on_kqueue_return_num_cb_with_context;
		PPP_RUN_CB(on_kqueue_enter, cpu, pc);
	}; break;
	// 363, 560 int kevent ['int fd', 'struct kevent_freebsd11 *changelist', 'int nchanges', 'struct kevent_freebsd11 *eventlist', 'int nevents', 'const struct timespec *timeout'] OR ['int fd', 'struct kevent *changelist', 'int nchanges', 'struct kevent *eventlist', 'int nevents', 'const struct timespec *timeout']
	case 363: case 560: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kevent_return_num_cb;
		return_context_count = &ppp_on_kevent_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kevent_return)));
		if (save_args || PPP_CHECK_CB(on_kevent_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		int32_t arg4 = get_s32(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(int32_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kevent_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 371 ssize_t extattr_set_fd ['int fd', 'int attrnamespace', 'const char *attrname', 'void *data', 'size_t nbytes']
	case 371: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_set_fd_return_num_cb;
		return_context_count = &ppp_on_extattr_set_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_set_fd_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_set_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_set_fd_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 372 ssize_t extattr_get_fd ['int fd', 'int attrnamespace', 'const char *attrname', 'void *data', 'size_t nbytes']
	case 372: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_get_fd_return_num_cb;
		return_context_count = &ppp_on_extattr_get_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_get_fd_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_get_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_get_fd_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 373 int extattr_delete_fd ['int fd', 'int attrnamespace', 'const char *attrname']
	case 373: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_delete_fd_return_num_cb;
		return_context_count = &ppp_on_extattr_delete_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_delete_fd_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_delete_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_extattr_delete_fd_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 374 int __setugid ['int flag']
	case 374: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___setugid_return_num_cb;
		return_context_count = &ppp_on___setugid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___setugid_return)));
		if (save_args || PPP_CHECK_CB(on___setugid_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on___setugid_enter, cpu, pc, arg0);
		}
	}; break;
	// 376 int eaccess ['const char *path', 'int amode']
	case 376: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_eaccess_return_num_cb;
		return_context_count = &ppp_on_eaccess_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_eaccess_return)));
		if (save_args || PPP_CHECK_CB(on_eaccess_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_eaccess_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 377 int afs3_syscall ['long syscall', 'long parm1', 'long parm2', 'long parm3', 'long parm4', 'long parm5', 'long parm6']
	case 377: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_afs3_syscall_return_num_cb;
		return_context_count = &ppp_on_afs3_syscall_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_afs3_syscall_return)));
		if (save_args || PPP_CHECK_CB(on_afs3_syscall_enter)) {
			int64_t arg0 = get_s64(cpu, &ctx, 0);
		int64_t arg1 = get_s64(cpu, &ctx, 1);
		int64_t arg2 = get_s64(cpu, &ctx, 2);
		int64_t arg3 = get_s64(cpu, &ctx, 3);
		int64_t arg4 = get_s64(cpu, &ctx, 4);
		int64_t arg5 = get_s64(cpu, &ctx, 5);
		int64_t arg6 = get_s64(cpu, &ctx, 6);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int64_t));
				memcpy(ctx.args[4], &arg4, sizeof(int64_t));
				memcpy(ctx.args[5], &arg5, sizeof(int64_t));
				memcpy(ctx.args[6], &arg6, sizeof(int64_t));
			}
			PPP_RUN_CB(on_afs3_syscall_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
		}
	}; break;
	// 378 int nmount ['struct iovec *iovp', 'unsigned int iovcnt', 'int flags']
	case 378: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_nmount_return_num_cb;
		return_context_count = &ppp_on_nmount_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_nmount_return)));
		if (save_args || PPP_CHECK_CB(on_nmount_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_nmount_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 384 int __mac_get_proc ['struct mac *mac_p']
	case 384: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_get_proc_return_num_cb;
		return_context_count = &ppp_on___mac_get_proc_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_get_proc_return)));
		if (save_args || PPP_CHECK_CB(on___mac_get_proc_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_get_proc_enter, cpu, pc, arg0);
		}
	}; break;
	// 385 int __mac_set_proc ['struct mac *mac_p']
	case 385: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_set_proc_return_num_cb;
		return_context_count = &ppp_on___mac_set_proc_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_set_proc_return)));
		if (save_args || PPP_CHECK_CB(on___mac_set_proc_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_set_proc_enter, cpu, pc, arg0);
		}
	}; break;
	// 386 int __mac_get_fd ['int fd', 'struct mac *mac_p']
	case 386: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_get_fd_return_num_cb;
		return_context_count = &ppp_on___mac_get_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_get_fd_return)));
		if (save_args || PPP_CHECK_CB(on___mac_get_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_get_fd_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 387 int __mac_get_file ['const char *path_p', 'struct mac *mac_p']
	case 387: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_get_file_return_num_cb;
		return_context_count = &ppp_on___mac_get_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_get_file_return)));
		if (save_args || PPP_CHECK_CB(on___mac_get_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_get_file_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 388 int __mac_set_fd ['int fd', 'struct mac *mac_p']
	case 388: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_set_fd_return_num_cb;
		return_context_count = &ppp_on___mac_set_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_set_fd_return)));
		if (save_args || PPP_CHECK_CB(on___mac_set_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_set_fd_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 389 int __mac_set_file ['const char *path_p', 'struct mac *mac_p']
	case 389: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_set_file_return_num_cb;
		return_context_count = &ppp_on___mac_set_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_set_file_return)));
		if (save_args || PPP_CHECK_CB(on___mac_set_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_set_file_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 390 int kenv ['int what', 'const char *name', 'char *value', 'int len']
	case 390: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kenv_return_num_cb;
		return_context_count = &ppp_on_kenv_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kenv_return)));
		if (save_args || PPP_CHECK_CB(on_kenv_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_kenv_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 391 int lchflags ['const char *path', 'u_long flags']
	case 391: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lchflags_return_num_cb;
		return_context_count = &ppp_on_lchflags_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lchflags_return)));
		if (save_args || PPP_CHECK_CB(on_lchflags_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int64_t arg1 = get_s64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int64_t));
			}
			PPP_RUN_CB(on_lchflags_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 392 int uuidgen ['struct uuid *store', 'int count']
	case 392: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_uuidgen_return_num_cb;
		return_context_count = &ppp_on_uuidgen_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_uuidgen_return)));
		if (save_args || PPP_CHECK_CB(on_uuidgen_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_uuidgen_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 394 int mac_syscall ['const char *policy', 'int call', 'void *arg']
	case 394: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mac_syscall_return_num_cb;
		return_context_count = &ppp_on_mac_syscall_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mac_syscall_return)));
		if (save_args || PPP_CHECK_CB(on_mac_syscall_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_mac_syscall_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 400 int ksem_close ['semid_t id']
	case 400: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_close_return_num_cb;
		return_context_count = &ppp_on_ksem_close_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_close_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_close_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_ksem_close_enter, cpu, pc, arg0);
		}
	}; break;
	// 401 int ksem_post ['semid_t id']
	case 401: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_post_return_num_cb;
		return_context_count = &ppp_on_ksem_post_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_post_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_post_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_ksem_post_enter, cpu, pc, arg0);
		}
	}; break;
	// 402 int ksem_wait ['semid_t id']
	case 402: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_wait_return_num_cb;
		return_context_count = &ppp_on_ksem_wait_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_wait_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_wait_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_ksem_wait_enter, cpu, pc, arg0);
		}
	}; break;
	// 403 int ksem_trywait ['semid_t id']
	case 403: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_trywait_return_num_cb;
		return_context_count = &ppp_on_ksem_trywait_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_trywait_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_trywait_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_ksem_trywait_enter, cpu, pc, arg0);
		}
	}; break;
	// 404 int ksem_init ['semid_t *idp', 'unsigned int value']
	case 404: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_init_return_num_cb;
		return_context_count = &ppp_on_ksem_init_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_init_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_init_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_ksem_init_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 405 int ksem_open ['semid_t *idp', 'const char *name', 'int oflag', 'mode_t mode', 'unsigned int value']
	case 405: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_open_return_num_cb;
		return_context_count = &ppp_on_ksem_open_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_open_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_open_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_ksem_open_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 406 int ksem_unlink ['const char *name']
	case 406: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_unlink_return_num_cb;
		return_context_count = &ppp_on_ksem_unlink_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_unlink_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_unlink_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ksem_unlink_enter, cpu, pc, arg0);
		}
	}; break;
	// 407 int ksem_getvalue ['semid_t id', 'int *val']
	case 407: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_getvalue_return_num_cb;
		return_context_count = &ppp_on_ksem_getvalue_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_getvalue_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_getvalue_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ksem_getvalue_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 408 int ksem_destroy ['semid_t id']
	case 408: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_destroy_return_num_cb;
		return_context_count = &ppp_on_ksem_destroy_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_destroy_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_destroy_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_ksem_destroy_enter, cpu, pc, arg0);
		}
	}; break;
	// 409 int __mac_get_pid ['pid_t pid', 'struct mac *mac_p']
	case 409: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_get_pid_return_num_cb;
		return_context_count = &ppp_on___mac_get_pid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_get_pid_return)));
		if (save_args || PPP_CHECK_CB(on___mac_get_pid_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_get_pid_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 410 int __mac_get_link ['const char *path_p', 'struct mac *mac_p']
	case 410: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_get_link_return_num_cb;
		return_context_count = &ppp_on___mac_get_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_get_link_return)));
		if (save_args || PPP_CHECK_CB(on___mac_get_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_get_link_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 411 int __mac_set_link ['const char *path_p', 'struct mac *mac_p']
	case 411: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___mac_set_link_return_num_cb;
		return_context_count = &ppp_on___mac_set_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_set_link_return)));
		if (save_args || PPP_CHECK_CB(on___mac_set_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_set_link_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 412 ssize_t extattr_set_link ['const char *path', 'int attrnamespace', 'const char *attrname', 'void *data', 'size_t nbytes']
	case 412: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_set_link_return_num_cb;
		return_context_count = &ppp_on_extattr_set_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_set_link_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_set_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_set_link_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 413 ssize_t extattr_get_link ['const char *path', 'int attrnamespace', 'const char *attrname', 'void *data', 'size_t nbytes']
	case 413: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_get_link_return_num_cb;
		return_context_count = &ppp_on_extattr_get_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_get_link_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_get_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_get_link_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 414 int extattr_delete_link ['const char *path', 'int attrnamespace', 'const char *attrname']
	case 414: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_delete_link_return_num_cb;
		return_context_count = &ppp_on_extattr_delete_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_delete_link_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_delete_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_extattr_delete_link_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 415 int __mac_execve ['const char *fname', 'char **argv', 'char **envv', 'struct mac *mac_p']
	case 415: {
		panda_noreturn = true;
		ctx.double_return = false;
		return_count = &ppp_on___mac_execve_return_num_cb;
		return_context_count = &ppp_on___mac_execve_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___mac_execve_return)));
		if (save_args || PPP_CHECK_CB(on___mac_execve_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___mac_execve_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 421 int getcontext ['struct __ucontext *ucp']
	case 421: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getcontext_return_num_cb;
		return_context_count = &ppp_on_getcontext_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getcontext_return)));
		if (save_args || PPP_CHECK_CB(on_getcontext_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getcontext_enter, cpu, pc, arg0);
		}
	}; break;
	// 422 int setcontext ['const struct __ucontext *ucp']
	case 422: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setcontext_return_num_cb;
		return_context_count = &ppp_on_setcontext_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setcontext_return)));
		if (save_args || PPP_CHECK_CB(on_setcontext_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_setcontext_enter, cpu, pc, arg0);
		}
	}; break;
	// 423 int swapcontext ['struct __ucontext *oucp', 'const struct __ucontext *ucp']
	case 423: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_swapcontext_return_num_cb;
		return_context_count = &ppp_on_swapcontext_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_swapcontext_return)));
		if (save_args || PPP_CHECK_CB(on_swapcontext_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_swapcontext_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 424 int swapoff ['const char *name']
	case 424: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_swapoff_return_num_cb;
		return_context_count = &ppp_on_swapoff_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_swapoff_return)));
		if (save_args || PPP_CHECK_CB(on_swapoff_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_swapoff_enter, cpu, pc, arg0);
		}
	}; break;
	// 425 int __acl_get_link ['const char *path', 'acl_type_t type', 'struct acl *aclp']
	case 425: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_get_link_return_num_cb;
		return_context_count = &ppp_on___acl_get_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_get_link_return)));
		if (save_args || PPP_CHECK_CB(on___acl_get_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_get_link_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 426 int __acl_set_link ['const char *path', 'acl_type_t type', 'struct acl *aclp']
	case 426: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_set_link_return_num_cb;
		return_context_count = &ppp_on___acl_set_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_set_link_return)));
		if (save_args || PPP_CHECK_CB(on___acl_set_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_set_link_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 427 int __acl_delete_link ['const char *path', 'acl_type_t type']
	case 427: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_delete_link_return_num_cb;
		return_context_count = &ppp_on___acl_delete_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_delete_link_return)));
		if (save_args || PPP_CHECK_CB(on___acl_delete_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on___acl_delete_link_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 428 int __acl_aclcheck_link ['const char *path', 'acl_type_t type', 'struct acl *aclp']
	case 428: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___acl_aclcheck_link_return_num_cb;
		return_context_count = &ppp_on___acl_aclcheck_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___acl_aclcheck_link_return)));
		if (save_args || PPP_CHECK_CB(on___acl_aclcheck_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___acl_aclcheck_link_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 429 int sigwait ['const sigset_t *set', 'int *sig']
	case 429: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigwait_return_num_cb;
		return_context_count = &ppp_on_sigwait_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigwait_return)));
		if (save_args || PPP_CHECK_CB(on_sigwait_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigwait_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 430 int thr_create ['ucontext_t *ctx', 'long *id', 'int flags']
	case 430: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_thr_create_return_num_cb;
		return_context_count = &ppp_on_thr_create_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_create_return)));
		if (save_args || PPP_CHECK_CB(on_thr_create_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_thr_create_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 431 void thr_exit ['long *state']
	case 431: {
		panda_noreturn = true;
		ctx.double_return = false;
		return_count = &ppp_on_thr_exit_return_num_cb;
		return_context_count = &ppp_on_thr_exit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_exit_return)));
		if (save_args || PPP_CHECK_CB(on_thr_exit_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_thr_exit_enter, cpu, pc, arg0);
		}
	}; break;
	// 432 int thr_self ['long *id']
	case 432: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_thr_self_return_num_cb;
		return_context_count = &ppp_on_thr_self_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_self_return)));
		if (save_args || PPP_CHECK_CB(on_thr_self_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_thr_self_enter, cpu, pc, arg0);
		}
	}; break;
	// 433 int thr_kill ['long id', 'int sig']
	case 433: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_thr_kill_return_num_cb;
		return_context_count = &ppp_on_thr_kill_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_kill_return)));
		if (save_args || PPP_CHECK_CB(on_thr_kill_enter)) {
			int64_t arg0 = get_s64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_thr_kill_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 436 int jail_attach ['int jid']
	case 436: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_jail_attach_return_num_cb;
		return_context_count = &ppp_on_jail_attach_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_jail_attach_return)));
		if (save_args || PPP_CHECK_CB(on_jail_attach_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_jail_attach_enter, cpu, pc, arg0);
		}
	}; break;
	// 437 ssize_t extattr_list_fd ['int fd', 'int attrnamespace', 'void *data', 'size_t nbytes']
	case 437: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_list_fd_return_num_cb;
		return_context_count = &ppp_on_extattr_list_fd_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_list_fd_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_list_fd_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_list_fd_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 438 ssize_t extattr_list_file ['const char *path', 'int attrnamespace', 'void *data', 'size_t nbytes']
	case 438: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_list_file_return_num_cb;
		return_context_count = &ppp_on_extattr_list_file_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_list_file_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_list_file_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_list_file_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 439 ssize_t extattr_list_link ['const char *path', 'int attrnamespace', 'void *data', 'size_t nbytes']
	case 439: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_extattr_list_link_return_num_cb;
		return_context_count = &ppp_on_extattr_list_link_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_extattr_list_link_return)));
		if (save_args || PPP_CHECK_CB(on_extattr_list_link_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_extattr_list_link_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 441 int ksem_timedwait ['semid_t id', 'const struct timespec *abstime']
	case 441: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ksem_timedwait_return_num_cb;
		return_context_count = &ppp_on_ksem_timedwait_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ksem_timedwait_return)));
		if (save_args || PPP_CHECK_CB(on_ksem_timedwait_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ksem_timedwait_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 442 int thr_suspend ['const struct timespec *timeout']
	case 442: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_thr_suspend_return_num_cb;
		return_context_count = &ppp_on_thr_suspend_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_suspend_return)));
		if (save_args || PPP_CHECK_CB(on_thr_suspend_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_thr_suspend_enter, cpu, pc, arg0);
		}
	}; break;
	// 443 int thr_wake ['long id']
	case 443: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_thr_wake_return_num_cb;
		return_context_count = &ppp_on_thr_wake_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_wake_return)));
		if (save_args || PPP_CHECK_CB(on_thr_wake_enter)) {
			int64_t arg0 = get_s64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int64_t));
			}
			PPP_RUN_CB(on_thr_wake_enter, cpu, pc, arg0);
		}
	}; break;
	// 444 int kldunloadf ['int fileid', 'int flags']
	case 444: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kldunloadf_return_num_cb;
		return_context_count = &ppp_on_kldunloadf_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kldunloadf_return)));
		if (save_args || PPP_CHECK_CB(on_kldunloadf_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_kldunloadf_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 445 int audit ['const void *record', 'unsigned length']
	case 445: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_audit_return_num_cb;
		return_context_count = &ppp_on_audit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_audit_return)));
		if (save_args || PPP_CHECK_CB(on_audit_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_audit_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 446 int auditon ['int cmd', 'void *data', 'unsigned length']
	case 446: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_auditon_return_num_cb;
		return_context_count = &ppp_on_auditon_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_auditon_return)));
		if (save_args || PPP_CHECK_CB(on_auditon_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_auditon_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 447 int getauid ['uid_t *auid']
	case 447: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getauid_return_num_cb;
		return_context_count = &ppp_on_getauid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getauid_return)));
		if (save_args || PPP_CHECK_CB(on_getauid_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getauid_enter, cpu, pc, arg0);
		}
	}; break;
	// 448 int setauid ['uid_t *auid']
	case 448: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setauid_return_num_cb;
		return_context_count = &ppp_on_setauid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setauid_return)));
		if (save_args || PPP_CHECK_CB(on_setauid_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_setauid_enter, cpu, pc, arg0);
		}
	}; break;
	// 449 int getaudit ['struct auditinfo *auditinfo']
	case 449: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getaudit_return_num_cb;
		return_context_count = &ppp_on_getaudit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getaudit_return)));
		if (save_args || PPP_CHECK_CB(on_getaudit_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_getaudit_enter, cpu, pc, arg0);
		}
	}; break;
	// 450 int setaudit ['struct auditinfo *auditinfo']
	case 450: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setaudit_return_num_cb;
		return_context_count = &ppp_on_setaudit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setaudit_return)));
		if (save_args || PPP_CHECK_CB(on_setaudit_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_setaudit_enter, cpu, pc, arg0);
		}
	}; break;
	// 451 int getaudit_addr ['struct auditinfo_addr *auditinfo_addr', 'unsigned length']
	case 451: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getaudit_addr_return_num_cb;
		return_context_count = &ppp_on_getaudit_addr_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getaudit_addr_return)));
		if (save_args || PPP_CHECK_CB(on_getaudit_addr_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_getaudit_addr_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 452 int setaudit_addr ['struct auditinfo_addr *auditinfo_addr', 'unsigned length']
	case 452: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setaudit_addr_return_num_cb;
		return_context_count = &ppp_on_setaudit_addr_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setaudit_addr_return)));
		if (save_args || PPP_CHECK_CB(on_setaudit_addr_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_setaudit_addr_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 453 int auditctl ['const char *path']
	case 453: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_auditctl_return_num_cb;
		return_context_count = &ppp_on_auditctl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_auditctl_return)));
		if (save_args || PPP_CHECK_CB(on_auditctl_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_auditctl_enter, cpu, pc, arg0);
		}
	}; break;
	// 454 int _umtx_op ['void *obj', 'int op', 'u_long val', 'void *uaddr1', 'void *uaddr2']
	case 454: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on__umtx_op_return_num_cb;
		return_context_count = &ppp_on__umtx_op_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on__umtx_op_return)));
		if (save_args || PPP_CHECK_CB(on__umtx_op_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		int64_t arg2 = get_s64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on__umtx_op_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 455 int thr_new ['struct thr_param *param', 'int param_size']
	case 455: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_thr_new_return_num_cb;
		return_context_count = &ppp_on_thr_new_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_new_return)));
		if (save_args || PPP_CHECK_CB(on_thr_new_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_thr_new_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 456 int sigqueue ['pid_t pid', 'int signum', 'void *value']
	case 456: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigqueue_return_num_cb;
		return_context_count = &ppp_on_sigqueue_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigqueue_return)));
		if (save_args || PPP_CHECK_CB(on_sigqueue_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigqueue_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 457 int kmq_open ['const char *path', 'int flags', 'mode_t mode', 'const struct mq_attr *attr']
	case 457: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kmq_open_return_num_cb;
		return_context_count = &ppp_on_kmq_open_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kmq_open_return)));
		if (save_args || PPP_CHECK_CB(on_kmq_open_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kmq_open_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 458 int kmq_setattr ['int mqd', 'const struct mq_attr *attr', 'struct mq_attr *oattr']
	case 458: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kmq_setattr_return_num_cb;
		return_context_count = &ppp_on_kmq_setattr_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kmq_setattr_return)));
		if (save_args || PPP_CHECK_CB(on_kmq_setattr_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kmq_setattr_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 459 int kmq_timedreceive ['int mqd', 'char *msg_ptr', 'size_t msg_len', 'unsigned *msg_prio', 'const struct timespec *abs_timeout']
	case 459: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kmq_timedreceive_return_num_cb;
		return_context_count = &ppp_on_kmq_timedreceive_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kmq_timedreceive_return)));
		if (save_args || PPP_CHECK_CB(on_kmq_timedreceive_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kmq_timedreceive_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 460 int kmq_timedsend ['int mqd', 'const char *msg_ptr', 'size_t msg_len', 'unsigned msg_prio', 'const struct timespec *abs_timeout']
	case 460: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kmq_timedsend_return_num_cb;
		return_context_count = &ppp_on_kmq_timedsend_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kmq_timedsend_return)));
		if (save_args || PPP_CHECK_CB(on_kmq_timedsend_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kmq_timedsend_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 461 int kmq_notify ['int mqd', 'const struct sigevent *sigev']
	case 461: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kmq_notify_return_num_cb;
		return_context_count = &ppp_on_kmq_notify_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kmq_notify_return)));
		if (save_args || PPP_CHECK_CB(on_kmq_notify_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kmq_notify_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 462 int kmq_unlink ['const char *path']
	case 462: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_kmq_unlink_return_num_cb;
		return_context_count = &ppp_on_kmq_unlink_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_kmq_unlink_return)));
		if (save_args || PPP_CHECK_CB(on_kmq_unlink_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_kmq_unlink_enter, cpu, pc, arg0);
		}
	}; break;
	// 463 int abort2 ['const char *why', 'int nargs', 'void **args']
	case 463: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_abort2_return_num_cb;
		return_context_count = &ppp_on_abort2_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_abort2_return)));
		if (save_args || PPP_CHECK_CB(on_abort2_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_abort2_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 464 int thr_set_name ['long id', 'const char *name']
	case 464: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_thr_set_name_return_num_cb;
		return_context_count = &ppp_on_thr_set_name_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_set_name_return)));
		if (save_args || PPP_CHECK_CB(on_thr_set_name_enter)) {
			int64_t arg0 = get_s64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_thr_set_name_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 465 int aio_fsync ['int op', 'struct aiocb *aiocbp']
	case 465: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_fsync_return_num_cb;
		return_context_count = &ppp_on_aio_fsync_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_fsync_return)));
		if (save_args || PPP_CHECK_CB(on_aio_fsync_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_fsync_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 466 int rtprio_thread ['int function', 'lwpid_t lwpid', 'struct rtprio *rtp']
	case 466: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rtprio_thread_return_num_cb;
		return_context_count = &ppp_on_rtprio_thread_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rtprio_thread_return)));
		if (save_args || PPP_CHECK_CB(on_rtprio_thread_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_rtprio_thread_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 471 int sctp_peeloff ['int sd', 'uint32_t name']
	case 471: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sctp_peeloff_return_num_cb;
		return_context_count = &ppp_on_sctp_peeloff_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sctp_peeloff_return)));
		if (save_args || PPP_CHECK_CB(on_sctp_peeloff_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sctp_peeloff_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 472 int sctp_generic_sendmsg ['int sd', 'void *msg', 'int mlen', 'struct sockaddr *to', '__socklen_t tolen', 'struct sctp_sndrcvinfo *sinfo', 'int flags']
	case 472: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sctp_generic_sendmsg_return_num_cb;
		return_context_count = &ppp_on_sctp_generic_sendmsg_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sctp_generic_sendmsg_return)));
		if (save_args || PPP_CHECK_CB(on_sctp_generic_sendmsg_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
		int32_t arg6 = get_s32(cpu, &ctx, 6);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
				memcpy(ctx.args[6], &arg6, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sctp_generic_sendmsg_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
		}
	}; break;
	// 473 int sctp_generic_sendmsg_iov ['int sd', 'struct iovec *iov', 'int iovlen', 'struct sockaddr *to', '__socklen_t tolen', 'struct sctp_sndrcvinfo *sinfo', 'int flags']
	case 473: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sctp_generic_sendmsg_iov_return_num_cb;
		return_context_count = &ppp_on_sctp_generic_sendmsg_iov_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sctp_generic_sendmsg_iov_return)));
		if (save_args || PPP_CHECK_CB(on_sctp_generic_sendmsg_iov_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
		int32_t arg6 = get_s32(cpu, &ctx, 6);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
				memcpy(ctx.args[6], &arg6, sizeof(int32_t));
			}
			PPP_RUN_CB(on_sctp_generic_sendmsg_iov_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
		}
	}; break;
	// 474 int sctp_generic_recvmsg ['int sd', 'struct iovec *iov', 'int iovlen', 'struct sockaddr *from', '__socklen_t *fromlenaddr', 'struct sctp_sndrcvinfo *sinfo', 'int *msg_flags']
	case 474: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sctp_generic_recvmsg_return_num_cb;
		return_context_count = &ppp_on_sctp_generic_recvmsg_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sctp_generic_recvmsg_return)));
		if (save_args || PPP_CHECK_CB(on_sctp_generic_recvmsg_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
		uint64_t arg6 = get_64(cpu, &ctx, 6);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
				memcpy(ctx.args[6], &arg6, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sctp_generic_recvmsg_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5, arg6);
		}
	}; break;
	// 475 ssize_t pread ['int fd', 'void *buf', 'size_t nbyte', 'off_t offset']
	case 475: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pread_return_num_cb;
		return_context_count = &ppp_on_pread_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pread_return)));
		if (save_args || PPP_CHECK_CB(on_pread_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_pread_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 476 ssize_t pwrite ['int fd', 'const void *buf', 'size_t nbyte', 'off_t offset']
	case 476: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pwrite_return_num_cb;
		return_context_count = &ppp_on_pwrite_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pwrite_return)));
		if (save_args || PPP_CHECK_CB(on_pwrite_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_pwrite_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 478 off_t lseek ['int fd', 'off_t offset', 'int whence']
	case 478: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lseek_return_num_cb;
		return_context_count = &ppp_on_lseek_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lseek_return)));
		if (save_args || PPP_CHECK_CB(on_lseek_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_lseek_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 479 int truncate ['const char *path', 'off_t length']
	case 479: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_truncate_return_num_cb;
		return_context_count = &ppp_on_truncate_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_truncate_return)));
		if (save_args || PPP_CHECK_CB(on_truncate_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_truncate_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 480 int ftruncate ['int fd', 'off_t length']
	case 480: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ftruncate_return_num_cb;
		return_context_count = &ppp_on_ftruncate_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ftruncate_return)));
		if (save_args || PPP_CHECK_CB(on_ftruncate_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ftruncate_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 481 int thr_kill2 ['pid_t pid', 'long id', 'int sig']
	case 481: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_thr_kill2_return_num_cb;
		return_context_count = &ppp_on_thr_kill2_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_thr_kill2_return)));
		if (save_args || PPP_CHECK_CB(on_thr_kill2_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int64_t arg1 = get_s64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_thr_kill2_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 482 int shm_open ['const char *path', 'int flags', 'mode_t mode']
	case 482: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_shm_open_return_num_cb;
		return_context_count = &ppp_on_shm_open_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_shm_open_return)));
		if (save_args || PPP_CHECK_CB(on_shm_open_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_shm_open_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 483 int shm_unlink ['const char *path']
	case 483: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_shm_unlink_return_num_cb;
		return_context_count = &ppp_on_shm_unlink_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_shm_unlink_return)));
		if (save_args || PPP_CHECK_CB(on_shm_unlink_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_shm_unlink_enter, cpu, pc, arg0);
		}
	}; break;
	// 484 int cpuset ['cpusetid_t *setid']
	case 484: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cpuset_return_num_cb;
		return_context_count = &ppp_on_cpuset_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cpuset_return)));
		if (save_args || PPP_CHECK_CB(on_cpuset_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_cpuset_enter, cpu, pc, arg0);
		}
	}; break;
	// 485 int cpuset_setid ['cpuwhich_t which', 'id_t id', 'cpusetid_t setid']
	case 485: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cpuset_setid_return_num_cb;
		return_context_count = &ppp_on_cpuset_setid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cpuset_setid_return)));
		if (save_args || PPP_CHECK_CB(on_cpuset_setid_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_cpuset_setid_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 486 int cpuset_getid ['cpulevel_t level', 'cpuwhich_t which', 'id_t id', 'cpusetid_t *setid']
	case 486: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cpuset_getid_return_num_cb;
		return_context_count = &ppp_on_cpuset_getid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cpuset_getid_return)));
		if (save_args || PPP_CHECK_CB(on_cpuset_getid_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_cpuset_getid_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 487 int cpuset_getaffinity ['cpulevel_t level', 'cpuwhich_t which', 'id_t id', 'size_t cpusetsize', 'cpuset_t *mask']
	case 487: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cpuset_getaffinity_return_num_cb;
		return_context_count = &ppp_on_cpuset_getaffinity_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cpuset_getaffinity_return)));
		if (save_args || PPP_CHECK_CB(on_cpuset_getaffinity_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_cpuset_getaffinity_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 488 int cpuset_setaffinity ['cpulevel_t level', 'cpuwhich_t which', 'id_t id', 'size_t cpusetsize', 'const cpuset_t *mask']
	case 488: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cpuset_setaffinity_return_num_cb;
		return_context_count = &ppp_on_cpuset_setaffinity_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cpuset_setaffinity_return)));
		if (save_args || PPP_CHECK_CB(on_cpuset_setaffinity_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_cpuset_setaffinity_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 489 int faccessat ['int fd', 'const char *path', 'int amode', 'int flag']
	case 489: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_faccessat_return_num_cb;
		return_context_count = &ppp_on_faccessat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_faccessat_return)));
		if (save_args || PPP_CHECK_CB(on_faccessat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_faccessat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 490 int fchmodat ['int fd', 'const char *path', 'mode_t mode', 'int flag']
	case 490: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fchmodat_return_num_cb;
		return_context_count = &ppp_on_fchmodat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fchmodat_return)));
		if (save_args || PPP_CHECK_CB(on_fchmodat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fchmodat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 491 int fchownat ['int fd', 'const char *path', 'uid_t uid', 'gid_t gid', 'int flag']
	case 491: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fchownat_return_num_cb;
		return_context_count = &ppp_on_fchownat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fchownat_return)));
		if (save_args || PPP_CHECK_CB(on_fchownat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		int32_t arg4 = get_s32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fchownat_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 492 int fexecve ['int fd', 'char **argv', 'char **envv']
	case 492: {
		panda_noreturn = true;
		ctx.double_return = false;
		return_count = &ppp_on_fexecve_return_num_cb;
		return_context_count = &ppp_on_fexecve_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fexecve_return)));
		if (save_args || PPP_CHECK_CB(on_fexecve_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_fexecve_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 493, 552 int fstatat ['int fd', 'const char *path', 'struct freebsd11_stat *buf', 'int flag'] OR ['int fd', 'const char *path', 'struct stat *buf', 'int flag']
	case 493: case 552: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fstatat_return_num_cb;
		return_context_count = &ppp_on_fstatat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fstatat_return)));
		if (save_args || PPP_CHECK_CB(on_fstatat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fstatat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 494 int futimesat ['int fd', 'const char *path', 'struct timeval *times']
	case 494: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_futimesat_return_num_cb;
		return_context_count = &ppp_on_futimesat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_futimesat_return)));
		if (save_args || PPP_CHECK_CB(on_futimesat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_futimesat_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 495 int linkat ['int fd1', 'const char *path1', 'int fd2', 'const char *path2', 'int flag']
	case 495: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_linkat_return_num_cb;
		return_context_count = &ppp_on_linkat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_linkat_return)));
		if (save_args || PPP_CHECK_CB(on_linkat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		int32_t arg4 = get_s32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(int32_t));
			}
			PPP_RUN_CB(on_linkat_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 496 int mkdirat ['int fd', 'const char *path', 'mode_t mode']
	case 496: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mkdirat_return_num_cb;
		return_context_count = &ppp_on_mkdirat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mkdirat_return)));
		if (save_args || PPP_CHECK_CB(on_mkdirat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_mkdirat_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 497 int mkfifoat ['int fd', 'const char *path', 'mode_t mode']
	case 497: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mkfifoat_return_num_cb;
		return_context_count = &ppp_on_mkfifoat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mkfifoat_return)));
		if (save_args || PPP_CHECK_CB(on_mkfifoat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_mkfifoat_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 498 int mknodat ['int fd', 'const char *path', 'mode_t mode', 'uint32_t dev']
	case 498: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mknodat_return_num_cb;
		return_context_count = &ppp_on_mknodat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mknodat_return)));
		if (save_args || PPP_CHECK_CB(on_mknodat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_mknodat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 499 int openat ['int fd', 'const char *path', 'int flag', 'mode_t mode']
	case 499: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_openat_return_num_cb;
		return_context_count = &ppp_on_openat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_openat_return)));
		if (save_args || PPP_CHECK_CB(on_openat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_openat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 500 ssize_t readlinkat ['int fd', 'const char *path', 'char *buf', 'size_t bufsize']
	case 500: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_readlinkat_return_num_cb;
		return_context_count = &ppp_on_readlinkat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_readlinkat_return)));
		if (save_args || PPP_CHECK_CB(on_readlinkat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_readlinkat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 501 int renameat ['int oldfd', 'const char *old', 'int newfd', 'const char *new']
	case 501: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_renameat_return_num_cb;
		return_context_count = &ppp_on_renameat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_renameat_return)));
		if (save_args || PPP_CHECK_CB(on_renameat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_renameat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 502 int symlinkat ['const char *path1', 'int fd', 'const char *path2']
	case 502: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_symlinkat_return_num_cb;
		return_context_count = &ppp_on_symlinkat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_symlinkat_return)));
		if (save_args || PPP_CHECK_CB(on_symlinkat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_symlinkat_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 503 int unlinkat ['int fd', 'const char *path', 'int flag']
	case 503: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_unlinkat_return_num_cb;
		return_context_count = &ppp_on_unlinkat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_unlinkat_return)));
		if (save_args || PPP_CHECK_CB(on_unlinkat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_unlinkat_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 504 int posix_openpt ['int flags']
	case 504: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_posix_openpt_return_num_cb;
		return_context_count = &ppp_on_posix_openpt_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_posix_openpt_return)));
		if (save_args || PPP_CHECK_CB(on_posix_openpt_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_posix_openpt_enter, cpu, pc, arg0);
		}
	}; break;
	// 505 int gssd_syscall ['const char *path']
	case 505: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_gssd_syscall_return_num_cb;
		return_context_count = &ppp_on_gssd_syscall_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_gssd_syscall_return)));
		if (save_args || PPP_CHECK_CB(on_gssd_syscall_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_gssd_syscall_enter, cpu, pc, arg0);
		}
	}; break;
	// 506 int jail_get ['struct iovec *iovp', 'unsigned int iovcnt', 'int flags']
	case 506: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_jail_get_return_num_cb;
		return_context_count = &ppp_on_jail_get_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_jail_get_return)));
		if (save_args || PPP_CHECK_CB(on_jail_get_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_jail_get_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 507 int jail_set ['struct iovec *iovp', 'unsigned int iovcnt', 'int flags']
	case 507: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_jail_set_return_num_cb;
		return_context_count = &ppp_on_jail_set_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_jail_set_return)));
		if (save_args || PPP_CHECK_CB(on_jail_set_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_jail_set_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 508 int jail_remove ['int jid']
	case 508: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_jail_remove_return_num_cb;
		return_context_count = &ppp_on_jail_remove_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_jail_remove_return)));
		if (save_args || PPP_CHECK_CB(on_jail_remove_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_jail_remove_enter, cpu, pc, arg0);
		}
	}; break;
	// 509 int closefrom ['int lowfd']
	case 509: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_closefrom_return_num_cb;
		return_context_count = &ppp_on_closefrom_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_closefrom_return)));
		if (save_args || PPP_CHECK_CB(on_closefrom_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_closefrom_enter, cpu, pc, arg0);
		}
	}; break;
	// 513 int lpathconf ['const char *path', 'int name']
	case 513: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_lpathconf_return_num_cb;
		return_context_count = &ppp_on_lpathconf_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_lpathconf_return)));
		if (save_args || PPP_CHECK_CB(on_lpathconf_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_lpathconf_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 515 int __cap_rights_get ['int version', 'int fd', 'cap_rights_t *rightsp']
	case 515: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___cap_rights_get_return_num_cb;
		return_context_count = &ppp_on___cap_rights_get_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___cap_rights_get_return)));
		if (save_args || PPP_CHECK_CB(on___cap_rights_get_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on___cap_rights_get_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 516 int cap_enter ['void']
	case 516: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cap_enter_return_num_cb;
		return_context_count = &ppp_on_cap_enter_return_num_cb_with_context;
		PPP_RUN_CB(on_cap_enter_enter, cpu, pc);
	}; break;
	// 517 int cap_getmode ['unsigned *modep']
	case 517: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cap_getmode_return_num_cb;
		return_context_count = &ppp_on_cap_getmode_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cap_getmode_return)));
		if (save_args || PPP_CHECK_CB(on_cap_getmode_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_cap_getmode_enter, cpu, pc, arg0);
		}
	}; break;
	// 518 int pdfork ['int *fdp', 'int flags']
	case 518: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pdfork_return_num_cb;
		return_context_count = &ppp_on_pdfork_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pdfork_return)));
		if (save_args || PPP_CHECK_CB(on_pdfork_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_pdfork_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 519 int pdkill ['int fd', 'int signum']
	case 519: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pdkill_return_num_cb;
		return_context_count = &ppp_on_pdkill_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pdkill_return)));
		if (save_args || PPP_CHECK_CB(on_pdkill_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_pdkill_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 520 int pdgetpid ['int fd', 'pid_t *pidp']
	case 520: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pdgetpid_return_num_cb;
		return_context_count = &ppp_on_pdgetpid_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pdgetpid_return)));
		if (save_args || PPP_CHECK_CB(on_pdgetpid_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_pdgetpid_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 522 int pselect ['int nd', 'fd_set *in', 'fd_set *ou', 'fd_set *ex', 'const struct timespec *ts', 'const sigset_t *sm']
	case 522: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pselect_return_num_cb;
		return_context_count = &ppp_on_pselect_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pselect_return)));
		if (save_args || PPP_CHECK_CB(on_pselect_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_pselect_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 523 int getloginclass ['char *namebuf', 'size_t namelen']
	case 523: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getloginclass_return_num_cb;
		return_context_count = &ppp_on_getloginclass_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getloginclass_return)));
		if (save_args || PPP_CHECK_CB(on_getloginclass_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_getloginclass_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 524 int setloginclass ['const char *namebuf']
	case 524: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_setloginclass_return_num_cb;
		return_context_count = &ppp_on_setloginclass_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_setloginclass_return)));
		if (save_args || PPP_CHECK_CB(on_setloginclass_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_setloginclass_enter, cpu, pc, arg0);
		}
	}; break;
	// 525 int rctl_get_racct ['const void *inbufp', 'size_t inbuflen', 'void *outbufp', 'size_t outbuflen']
	case 525: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rctl_get_racct_return_num_cb;
		return_context_count = &ppp_on_rctl_get_racct_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rctl_get_racct_return)));
		if (save_args || PPP_CHECK_CB(on_rctl_get_racct_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_rctl_get_racct_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 526 int rctl_get_rules ['const void *inbufp', 'size_t inbuflen', 'void *outbufp', 'size_t outbuflen']
	case 526: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rctl_get_rules_return_num_cb;
		return_context_count = &ppp_on_rctl_get_rules_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rctl_get_rules_return)));
		if (save_args || PPP_CHECK_CB(on_rctl_get_rules_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_rctl_get_rules_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 527 int rctl_get_limits ['const void *inbufp', 'size_t inbuflen', 'void *outbufp', 'size_t outbuflen']
	case 527: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rctl_get_limits_return_num_cb;
		return_context_count = &ppp_on_rctl_get_limits_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rctl_get_limits_return)));
		if (save_args || PPP_CHECK_CB(on_rctl_get_limits_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_rctl_get_limits_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 528 int rctl_add_rule ['const void *inbufp', 'size_t inbuflen', 'void *outbufp', 'size_t outbuflen']
	case 528: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rctl_add_rule_return_num_cb;
		return_context_count = &ppp_on_rctl_add_rule_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rctl_add_rule_return)));
		if (save_args || PPP_CHECK_CB(on_rctl_add_rule_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_rctl_add_rule_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 529 int rctl_remove_rule ['const void *inbufp', 'size_t inbuflen', 'void *outbufp', 'size_t outbuflen']
	case 529: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rctl_remove_rule_return_num_cb;
		return_context_count = &ppp_on_rctl_remove_rule_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rctl_remove_rule_return)));
		if (save_args || PPP_CHECK_CB(on_rctl_remove_rule_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_rctl_remove_rule_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 530 int posix_fallocate ['int fd', 'off_t offset', 'off_t len']
	case 530: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_posix_fallocate_return_num_cb;
		return_context_count = &ppp_on_posix_fallocate_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_posix_fallocate_return)));
		if (save_args || PPP_CHECK_CB(on_posix_fallocate_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_posix_fallocate_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 531 int posix_fadvise ['int fd', 'off_t offset', 'off_t len', 'int advice']
	case 531: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_posix_fadvise_return_num_cb;
		return_context_count = &ppp_on_posix_fadvise_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_posix_fadvise_return)));
		if (save_args || PPP_CHECK_CB(on_posix_fadvise_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_posix_fadvise_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 532 int wait6 ['idtype_t idtype', 'id_t id', 'int *status', 'int options', 'struct __wrusage *wrusage', 'siginfo_t *info']
	case 532: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_wait6_return_num_cb;
		return_context_count = &ppp_on_wait6_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_wait6_return)));
		if (save_args || PPP_CHECK_CB(on_wait6_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_wait6_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 533 int cap_rights_limit ['int fd', 'cap_rights_t *rightsp']
	case 533: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cap_rights_limit_return_num_cb;
		return_context_count = &ppp_on_cap_rights_limit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cap_rights_limit_return)));
		if (save_args || PPP_CHECK_CB(on_cap_rights_limit_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_cap_rights_limit_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 534 int cap_ioctls_limit ['int fd', 'const u_long *cmds', 'size_t ncmds']
	case 534: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cap_ioctls_limit_return_num_cb;
		return_context_count = &ppp_on_cap_ioctls_limit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cap_ioctls_limit_return)));
		if (save_args || PPP_CHECK_CB(on_cap_ioctls_limit_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_cap_ioctls_limit_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 535 ssize_t cap_ioctls_get ['int fd', 'u_long *cmds', 'size_t maxcmds']
	case 535: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cap_ioctls_get_return_num_cb;
		return_context_count = &ppp_on_cap_ioctls_get_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cap_ioctls_get_return)));
		if (save_args || PPP_CHECK_CB(on_cap_ioctls_get_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_cap_ioctls_get_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 536 int cap_fcntls_limit ['int fd', 'uint32_t fcntlrights']
	case 536: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cap_fcntls_limit_return_num_cb;
		return_context_count = &ppp_on_cap_fcntls_limit_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cap_fcntls_limit_return)));
		if (save_args || PPP_CHECK_CB(on_cap_fcntls_limit_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_cap_fcntls_limit_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 537 int cap_fcntls_get ['int fd', 'uint32_t *fcntlrightsp']
	case 537: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cap_fcntls_get_return_num_cb;
		return_context_count = &ppp_on_cap_fcntls_get_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cap_fcntls_get_return)));
		if (save_args || PPP_CHECK_CB(on_cap_fcntls_get_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_cap_fcntls_get_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 538 int bindat ['int fd', 'int s', 'const struct sockaddr *name', 'int namelen']
	case 538: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_bindat_return_num_cb;
		return_context_count = &ppp_on_bindat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_bindat_return)));
		if (save_args || PPP_CHECK_CB(on_bindat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_bindat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 539 int connectat ['int fd', 'int s', 'const struct sockaddr *name', 'int namelen']
	case 539: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_connectat_return_num_cb;
		return_context_count = &ppp_on_connectat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_connectat_return)));
		if (save_args || PPP_CHECK_CB(on_connectat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_connectat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 540 int chflagsat ['int fd', 'const char *path', 'u_long flags', 'int atflag']
	case 540: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_chflagsat_return_num_cb;
		return_context_count = &ppp_on_chflagsat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_chflagsat_return)));
		if (save_args || PPP_CHECK_CB(on_chflagsat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int64_t arg2 = get_s64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_chflagsat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 541 int accept4 ['int s', 'struct sockaddr *name', '__socklen_t *anamelen', 'int flags']
	case 541: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_accept4_return_num_cb;
		return_context_count = &ppp_on_accept4_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_accept4_return)));
		if (save_args || PPP_CHECK_CB(on_accept4_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_accept4_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 542 int pipe2 ['int *fildes', 'int flags']
	case 542: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_pipe2_return_num_cb;
		return_context_count = &ppp_on_pipe2_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_pipe2_return)));
		if (save_args || PPP_CHECK_CB(on_pipe2_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
			}
			PPP_RUN_CB(on_pipe2_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 543 int aio_mlock ['struct aiocb *aiocbp']
	case 543: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_aio_mlock_return_num_cb;
		return_context_count = &ppp_on_aio_mlock_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_aio_mlock_return)));
		if (save_args || PPP_CHECK_CB(on_aio_mlock_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_aio_mlock_enter, cpu, pc, arg0);
		}
	}; break;
	// 544 int procctl ['idtype_t idtype', 'id_t id', 'int com', 'void *data']
	case 544: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_procctl_return_num_cb;
		return_context_count = &ppp_on_procctl_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_procctl_return)));
		if (save_args || PPP_CHECK_CB(on_procctl_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_procctl_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 545 int ppoll ['struct pollfd *fds', 'unsigned nfds', 'const struct timespec *ts', 'const sigset_t *set']
	case 545: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_ppoll_return_num_cb;
		return_context_count = &ppp_on_ppoll_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_ppoll_return)));
		if (save_args || PPP_CHECK_CB(on_ppoll_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_ppoll_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 546 int futimens ['int fd', 'struct timespec *times']
	case 546: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_futimens_return_num_cb;
		return_context_count = &ppp_on_futimens_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_futimens_return)));
		if (save_args || PPP_CHECK_CB(on_futimens_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_futimens_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 547 int utimensat ['int fd', 'const char *path', 'struct timespec *times', 'int flag']
	case 547: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_utimensat_return_num_cb;
		return_context_count = &ppp_on_utimensat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_utimensat_return)));
		if (save_args || PPP_CHECK_CB(on_utimensat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_utimensat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 550 int fdatasync ['int fd']
	case 550: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fdatasync_return_num_cb;
		return_context_count = &ppp_on_fdatasync_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fdatasync_return)));
		if (save_args || PPP_CHECK_CB(on_fdatasync_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
			}
			PPP_RUN_CB(on_fdatasync_enter, cpu, pc, arg0);
		}
	}; break;
	// 559 int mknodat ['int fd', 'const char *path', 'mode_t mode', 'dev_t dev']
	case 559: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_mknodat_return_num_cb;
		return_context_count = &ppp_on_mknodat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_mknodat_return)));
		if (save_args || PPP_CHECK_CB(on_mknodat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_mknodat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 561 int cpuset_getdomain ['cpulevel_t level', 'cpuwhich_t which', 'id_t id', 'size_t domainsetsize', 'domainset_t *mask', 'int *policy']
	case 561: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cpuset_getdomain_return_num_cb;
		return_context_count = &ppp_on_cpuset_getdomain_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cpuset_getdomain_return)));
		if (save_args || PPP_CHECK_CB(on_cpuset_getdomain_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		uint64_t arg5 = get_64(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_cpuset_getdomain_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 562 int cpuset_setdomain ['cpulevel_t level', 'cpuwhich_t which', 'id_t id', 'size_t domainsetsize', 'domainset_t *mask', 'int policy']
	case 562: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_cpuset_setdomain_return_num_cb;
		return_context_count = &ppp_on_cpuset_setdomain_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_cpuset_setdomain_return)));
		if (save_args || PPP_CHECK_CB(on_cpuset_setdomain_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		int32_t arg5 = get_s32(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(int32_t));
			}
			PPP_RUN_CB(on_cpuset_setdomain_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 563 int getrandom ['void *buf', 'size_t buflen', 'unsigned int flags']
	case 563: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getrandom_return_num_cb;
		return_context_count = &ppp_on_getrandom_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getrandom_return)));
		if (save_args || PPP_CHECK_CB(on_getrandom_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_getrandom_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 564 int getfhat ['int fd', 'char *path', 'struct fhandle *fhp', 'int flags']
	case 564: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_getfhat_return_num_cb;
		return_context_count = &ppp_on_getfhat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_getfhat_return)));
		if (save_args || PPP_CHECK_CB(on_getfhat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_getfhat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 565 int fhlink ['struct fhandle *fhp', 'const char *to']
	case 565: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fhlink_return_num_cb;
		return_context_count = &ppp_on_fhlink_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fhlink_return)));
		if (save_args || PPP_CHECK_CB(on_fhlink_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_fhlink_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 566 int fhlinkat ['struct fhandle *fhp', 'int tofd', 'const char *to', '']
	case 566: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fhlinkat_return_num_cb;
		return_context_count = &ppp_on_fhlinkat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fhlinkat_return)));
		if (save_args || PPP_CHECK_CB(on_fhlinkat_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_fhlinkat_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 567 int fhreadlink ['struct fhandle *fhp', 'char *buf', 'size_t bufsize']
	case 567: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_fhreadlink_return_num_cb;
		return_context_count = &ppp_on_fhreadlink_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_fhreadlink_return)));
		if (save_args || PPP_CHECK_CB(on_fhreadlink_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_fhreadlink_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 568 int funlinkat ['int dfd', 'const char *path', 'int fd', 'int flag']
	case 568: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_funlinkat_return_num_cb;
		return_context_count = &ppp_on_funlinkat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_funlinkat_return)));
		if (save_args || PPP_CHECK_CB(on_funlinkat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
			}
			PPP_RUN_CB(on_funlinkat_enter, cpu, pc, arg0, arg1, arg2, arg3);
		}
	}; break;
	// 569 ssize_t copy_file_range ['int infd', 'off_t *inoffp', 'int outfd', 'off_t *outoffp', 'size_t len', 'unsigned int flags']
	case 569: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_copy_file_range_return_num_cb;
		return_context_count = &ppp_on_copy_file_range_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_copy_file_range_return)));
		if (save_args || PPP_CHECK_CB(on_copy_file_range_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint32_t arg4 = get_32(cpu, &ctx, 4);
		uint32_t arg5 = get_32(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint32_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint32_t));
			}
			PPP_RUN_CB(on_copy_file_range_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 570 int __sysctlbyname ['const char *name', 'size_t namelen', 'void *old', 'size_t *oldlenp', 'void *new', 'size_t newlen']
	case 570: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___sysctlbyname_return_num_cb;
		return_context_count = &ppp_on___sysctlbyname_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___sysctlbyname_return)));
		if (save_args || PPP_CHECK_CB(on___sysctlbyname_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint64_t arg3 = get_64(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
		uint32_t arg5 = get_32(cpu, &ctx, 5);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint64_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
				memcpy(ctx.args[5], &arg5, sizeof(uint32_t));
			}
			PPP_RUN_CB(on___sysctlbyname_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5);
		}
	}; break;
	// 571 int shm_open2 ['const char *path', 'int flags', 'mode_t mode', 'int shmflags', 'const char *name']
	case 571: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_shm_open2_return_num_cb;
		return_context_count = &ppp_on_shm_open2_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_shm_open2_return)));
		if (save_args || PPP_CHECK_CB(on_shm_open2_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		int32_t arg1 = get_s32(cpu, &ctx, 1);
		uint32_t arg2 = get_32(cpu, &ctx, 2);
		int32_t arg3 = get_s32(cpu, &ctx, 3);
		uint64_t arg4 = get_64(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(int32_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint32_t));
				memcpy(ctx.args[3], &arg3, sizeof(int32_t));
				memcpy(ctx.args[4], &arg4, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_shm_open2_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 572 int shm_rename ['const char *path_from', 'const char *path_to', 'int flags']
	case 572: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_shm_rename_return_num_cb;
		return_context_count = &ppp_on_shm_rename_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_shm_rename_return)));
		if (save_args || PPP_CHECK_CB(on_shm_rename_enter)) {
			uint64_t arg0 = get_64(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint64_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_shm_rename_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 573 int sigfastblock ['int cmd', 'uint32_t *ptr']
	case 573: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_sigfastblock_return_num_cb;
		return_context_count = &ppp_on_sigfastblock_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_sigfastblock_return)));
		if (save_args || PPP_CHECK_CB(on_sigfastblock_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sigfastblock_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	// 574 int __realpathat ['int fd', 'const char *path', 'char *buf', 'size_t size', 'int flags']
	case 574: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on___realpathat_return_num_cb;
		return_context_count = &ppp_on___realpathat_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on___realpathat_return)));
		if (save_args || PPP_CHECK_CB(on___realpathat_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
		uint64_t arg2 = get_64(cpu, &ctx, 2);
		uint32_t arg3 = get_32(cpu, &ctx, 3);
		int32_t arg4 = get_s32(cpu, &ctx, 4);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
				memcpy(ctx.args[2], &arg2, sizeof(uint64_t));
				memcpy(ctx.args[3], &arg3, sizeof(uint32_t));
				memcpy(ctx.args[4], &arg4, sizeof(int32_t));
			}
			PPP_RUN_CB(on___realpathat_enter, cpu, pc, arg0, arg1, arg2, arg3, arg4);
		}
	}; break;
	// 575 int close_range ['unsigned lowfd', 'unsigned highfd', 'int flags']
	case 575: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_close_range_return_num_cb;
		return_context_count = &ppp_on_close_range_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_close_range_return)));
		if (save_args || PPP_CHECK_CB(on_close_range_enter)) {
			uint32_t arg0 = get_32(cpu, &ctx, 0);
		uint32_t arg1 = get_32(cpu, &ctx, 1);
		int32_t arg2 = get_s32(cpu, &ctx, 2);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(uint32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint32_t));
				memcpy(ctx.args[2], &arg2, sizeof(int32_t));
			}
			PPP_RUN_CB(on_close_range_enter, cpu, pc, arg0, arg1, arg2);
		}
	}; break;
	// 576 int rpctls_syscall ['int op', 'const char *path']
	case 576: {
		panda_noreturn = false;
		ctx.double_return = false;
		return_count = &ppp_on_rpctls_syscall_return_num_cb;
		return_context_count = &ppp_on_rpctls_syscall_return_num_cb_with_context;
		const bool save_args = PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_rpctls_syscall_return)));
		if (save_args || PPP_CHECK_CB(on_rpctls_syscall_enter)) {
			int32_t arg0 = get_s32(cpu, &ctx, 0);
		uint64_t arg1 = get_64(cpu, &ctx, 1);
			if (save_args) {
				memcpy(ctx.args[0], &arg0, sizeof(int32_t));
				memcpy(ctx.args[1], &arg1, sizeof(uint64_t));
			}
			PPP_RUN_CB(on_rpctls_syscall_enter, cpu, pc, arg0, arg1);
		}
	}; break;
	default:
		panda_noreturn = false;
		return_count = &ppp_on_unknown_sys_return_num_cb;
		return_context_count = &ppp_on_unknown_sys_return_num_cb_with_context;
		PPP_RUN_CB(on_unknown_sys_enter, cpu, pc, ctx.no);
	} // switch (ctx.no)

	PPP_RUN_CB(on_all_sys_enter, cpu, pc, ctx.no);
	PPP_RUN_CB(on_all_sys_enter2, cpu, pc, call, &ctx);
	if (!panda_noreturn && (*return_count > 0 || *return_context_count > 0 ||
			PPP_CHECK_CB(on_all_sys_return) || PPP_CHECK_CB(on_all_sys_return2))) {
		struct hook h;
		h.addr = ctx.retaddr;
		h.asid = ctx.asid;
		h.cb.before_tcg_codegen = hook_syscall_return;
		h.type = PANDA_CB_BEFORE_TCG_CODEGEN;
		h.enabled = true;
		h.km = MODE_ANY; //you'd expect this to be user only
		hooks_add_hook(&h);

		// Construct a new context directly; operator[] would zero it before copying ctx.
		auto stored = running_syscalls.emplace(std::make_pair(ctx.retaddr, ctx.asid), ctx);
		if (!stored.second) {
			stored.first->second = ctx;
		}
	}
#endif
}

/* vim: set tabstop=4 softtabstop=4 noexpandtab ft=cpp: */