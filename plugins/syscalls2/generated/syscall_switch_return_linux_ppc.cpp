#include "panda.h"

#include "syscalls2.h"
#include "syscalls2_info.h"

extern bool load_info;
#if defined(TARGET_PPC)
static bool first_load = true;
static syscall_info_t *info;
static syscall_meta_t *meta;
#endif

extern "C" {
#include "syscalls_ext_typedefs.h"
#include "syscall_ppp_extern_return.h"
}

void syscall_return_switch_linux_ppc(CPUState *cpu, target_ptr_t pc, const syscall_ctx_t *ctx) {
#if defined(TARGET_PPC)
	const syscall_info_t *call = NULL;
	syscall_info_t zero = {0};
	// only try this once
	if (first_load){
		first_load = false;
		if (load_info){
			sysinfo_load_profile(ctx->profile, &info, &meta);
		}
	}
	if (meta != NULL && ctx->no <= meta->max_generic) {
	  // If the syscall_info object from dso_info_....c doesn't have an entry
	  // for this syscall, we want to leave it as a NULL pointer
	  if (memcmp(&info[ctx->no], &zero, sizeof(syscall_info_t)) != 0) {
		call = &info[ctx->no];
	  }
	}
	switch (ctx->no) {
		// 0 long sys_restart_syscall ['void']
		case 0: {
			if (PPP_CHECK_CB(on_sys_restart_syscall_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_restart_syscall_return, cpu, pc) ;
		}; break;
		// 1 long sys_exit ['int error_code']
		case 1: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_exit_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_exit_return, cpu, pc, arg0) ;
		}; break;
		// 2 long sys_fork ['void']
		case 2: {
			if (PPP_CHECK_CB(on_sys_fork_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_fork_return, cpu, pc) ;
		}; break;
		// 3 long sys_read ['unsigned int fd', 'char __user *buf', 'size_t count']
		case 3: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_read_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_read_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 4 long sys_write ['unsigned int fd', 'const char __user *buf', 'size_t count']
		case 4: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_write_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_write_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 5 long sys_open ['const char __user *filename', 'int flags', 'umode_t mode']
		case 5: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_open_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_open_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 6 long sys_close ['unsigned int fd']
		case 6: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_close_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_close_return, cpu, pc, arg0) ;
		}; break;
		// 7 long sys_waitpid ['pid_t pid', 'int __user *stat_addr', 'int options']
		case 7: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_waitpid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_waitpid_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 8 long sys_creat ['const char __user *pathname', 'umode_t mode']
		case 8: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_creat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_creat_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 9 long sys_link ['const char __user *oldname', 'const char __user *newname']
		case 9: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_link_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_link_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 10 long sys_unlink ['const char __user *pathname']
		case 10: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_unlink_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_unlink_return, cpu, pc, arg0) ;
		}; break;
		// 11 long sys_execve ['const char __user *filename', 'const char __user *const __user *argv', 'const char __user *const __user *envp']
		case 11: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_execve_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_execve_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 12 long sys_chdir ['const char __user *filename']
		case 12: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_chdir_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_chdir_return, cpu, pc, arg0) ;
		}; break;
		// 13 long sys_time ['time_t __user *tloc']
		case 13: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_time_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_time_return, cpu, pc, arg0) ;
		}; break;
		// 14 long sys_mknod ['const char __user *filename', 'umode_t mode', 'unsigned dev']
		case 14: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_mknod_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mknod_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 15 long sys_chmod ['const char __user *filename', 'umode_t mode']
		case 15: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_chmod_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_chmod_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 16 long sys_lchown ['const char __user *filename', 'uid_t user', 'gid_t group']
		case 16: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_lchown_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lchown_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 18 long sys_oldstat ['const char __user *filename', 'struct __old_kernel_stat __user *statbuf']
		case 18: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_oldstat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_oldstat_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 19 long sys_lseek ['unsigned int fd', 'off_t offset', 'unsigned int whence']
		case 19: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_lseek_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lseek_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 20 long sys_getpid ['void']
		case 20: {
			if (PPP_CHECK_CB(on_sys_getpid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_getpid_return, cpu, pc) ;
		}; break;
		// 21 long sys_mount ['char __user *dev_name', 'char __user *dir_name', 'char __user *type', 'unsigned long flags', 'void __user *data']
		case 21: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_mount_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mount_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 22 long sys_umount ['char __user *name', 'int flags']
		case 22: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_umount_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_umount_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 23 long sys_setuid ['uid_t uid']
		case 23: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_setuid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setuid_return, cpu, pc, arg0) ;
		}; break;
		// 24 long sys_getuid ['void']
		case 24: {
			if (PPP_CHECK_CB(on_sys_getuid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_getuid_return, cpu, pc) ;
		}; break;
		// 25 long sys_stime ['time_t __user *tptr']
		case 25: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_stime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_stime_return, cpu, pc, arg0) ;
		}; break;
		// 26 long sys_ptrace ['long request', 'long pid', 'unsigned long addr', 'unsigned long data']
		case 26: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_ptrace_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_ptrace_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 27 long sys_alarm ['unsigned int seconds']
		case 27: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_alarm_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_alarm_return, cpu, pc, arg0) ;
		}; break;
		// 28 long sys_oldfstat ['unsigned int fd', 'struct __old_kernel_stat __user *statbuf']
		case 28: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_oldfstat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_oldfstat_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 29 long sys_pause ['void']
		case 29: {
			if (PPP_CHECK_CB(on_sys_pause_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_pause_return, cpu, pc) ;
		}; break;
		// 30 long sys_utime ['char __user *filename', 'struct utimbuf __user *times']
		case 30: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_utime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_utime_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 33 long sys_access ['const char __user *filename', 'int mode']
		case 33: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_access_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_access_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 34 long sys_nice ['int increment']
		case 34: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_nice_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_nice_return, cpu, pc, arg0) ;
		}; break;
		// 36 long sys_sync ['void']
		case 36: {
			if (PPP_CHECK_CB(on_sys_sync_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_sync_return, cpu, pc) ;
		}; break;
		// 37 long sys_kill ['pid_t pid', 'int sig']
		case 37: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_kill_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_kill_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 38 long sys_rename ['const char __user *oldname', 'const char __user *newname']
		case 38: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_rename_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rename_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 39 long sys_mkdir ['const char __user *pathname', 'umode_t mode']
		case 39: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_mkdir_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mkdir_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 40 long sys_rmdir ['const char __user *pathname']
		case 40: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_rmdir_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rmdir_return, cpu, pc, arg0) ;
		}; break;
		// 41 long sys_dup ['unsigned int fildes']
		case 41: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_dup_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_dup_return, cpu, pc, arg0) ;
		}; break;
		// 42 long sys_pipe ['int __user *fildes']
		case 42: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_pipe_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pipe_return, cpu, pc, arg0) ;
		}; break;
		// 43 long sys_times ['struct tms __user *tbuf']
		case 43: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_times_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_times_return, cpu, pc, arg0) ;
		}; break;
		// 45 long sys_brk ['unsigned long brk']
		case 45: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_brk_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_brk_return, cpu, pc, arg0) ;
		}; break;
		// 46 long sys_setgid ['gid_t gid']
		case 46: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_setgid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setgid_return, cpu, pc, arg0) ;
		}; break;
		// 47 long sys_getgid ['void']
		case 47: {
			if (PPP_CHECK_CB(on_sys_getgid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_getgid_return, cpu, pc) ;
		}; break;
		// 48 long sys_signal ['int sig', '__sighandler_t handler']
		case 48: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_signal_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_signal_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 49 long sys_geteuid ['void']
		case 49: {
			if (PPP_CHECK_CB(on_sys_geteuid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_geteuid_return, cpu, pc) ;
		}; break;
		// 50 long sys_getegid ['void']
		case 50: {
			if (PPP_CHECK_CB(on_sys_getegid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_getegid_return, cpu, pc) ;
		}; break;
		// 51 long sys_acct ['const char __user *name']
		case 51: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_acct_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_acct_return, cpu, pc, arg0) ;
		}; break;
		// 52 long sys_umount2 ['const char __user *name', 'int flags']
		case 52: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_umount2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_umount2_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 54 long sys_ioctl ['unsigned int fd', 'unsigned int cmd', 'unsigned long arg']
		case 54: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_ioctl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_ioctl_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 55 long sys_fcntl ['unsigned int fd', 'unsigned int cmd', 'unsigned long arg']
		case 55: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_fcntl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fcntl_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 57 long sys_setpgid ['pid_t pid', 'pid_t pgid']
		case 57: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_setpgid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_setpgid_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 59 long sys_oldolduname ['struct oldold_utsname __user *']
		case 59: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_oldolduname_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_oldolduname_return, cpu, pc, arg0) ;
		}; break;
		// 60 long sys_umask ['int mask']
		case 60: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_umask_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_umask_return, cpu, pc, arg0) ;
		}; break;
		// 61 long sys_chroot ['const char __user *filename']
		case 61: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_chroot_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_chroot_return, cpu, pc, arg0) ;
		}; break;
		// 62 long sys_ustat ['unsigned dev', 'struct ustat __user *ubuf']
		case 62: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_ustat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_ustat_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 63 long sys_dup2 ['unsigned int oldfd', 'unsigned int newfd']
		case 63: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_dup2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_dup2_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 64 long sys_getppid ['void']
		case 64: {
			if (PPP_CHECK_CB(on_sys_getppid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_getppid_return, cpu, pc) ;
		}; break;
		// 65 long sys_getpgrp ['void']
		case 65: {
			if (PPP_CHECK_CB(on_sys_getpgrp_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_getpgrp_return, cpu, pc) ;
		}; break;
		// 66 long sys_setsid ['void']
		case 66: {
			if (PPP_CHECK_CB(on_sys_setsid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_setsid_return, cpu, pc) ;
		}; break;
		// 67 long sys_sigaction ['int', 'const struct old_sigaction __user *', 'struct old_sigaction __user *']
		case 67: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sigaction_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sigaction_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 68 long sys_sgetmask ['void']
		case 68: {
			if (PPP_CHECK_CB(on_sys_sgetmask_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_sgetmask_return, cpu, pc) ;
		}; break;
		// 69 long sys_ssetmask ['int newmask']
		case 69: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_ssetmask_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_ssetmask_return, cpu, pc, arg0) ;
		}; break;
		// 70 long sys_setreuid ['uid_t ruid', 'uid_t euid']
		case 70: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_setreuid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setreuid_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 71 long sys_setregid ['gid_t rgid', 'gid_t egid']
		case 71: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_setregid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setregid_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 72 long sys_sigsuspend ['int unused1', 'int unused2', 'old_sigset_t mask']
		case 72: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sigsuspend_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sigsuspend_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 73 long sys_sigpending ['old_sigset_t __user *set']
		case 73: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_sigpending_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sigpending_return, cpu, pc, arg0) ;
		}; break;
		// 74 long sys_sethostname ['char __user *name', 'int len']
		case 74: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_sethostname_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_sethostname_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 75 long sys_setrlimit ['unsigned int resource', 'struct rlimit __user *rlim']
		case 75: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_setrlimit_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setrlimit_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 76 long sys_getrlimit ['unsigned int resource', 'struct rlimit __user *rlim']
		case 76: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_getrlimit_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getrlimit_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 77 long sys_getrusage ['int who', 'struct rusage __user *ru']
		case 77: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_getrusage_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getrusage_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 78 long sys_gettimeofday ['struct timeval __user *tv', 'struct timezone __user *tz']
		case 78: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_gettimeofday_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_gettimeofday_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 79 long sys_settimeofday ['struct timeval __user *tv', 'struct timezone __user *tz']
		case 79: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_settimeofday_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_settimeofday_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 80 long sys_getgroups ['int gidsetsize', 'gid_t __user *grouplist']
		case 80: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_getgroups_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getgroups_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 81 long sys_setgroups ['int gidsetsize', 'gid_t __user *grouplist']
		case 81: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_setgroups_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setgroups_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 82, 142 long sys_select ['int n', 'fd_set __user *inp', 'fd_set __user *outp', 'fd_set __user *exp', 'struct timeval __user *tvp']
		case 82:case 142: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_select_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_select_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 83 long sys_symlink ['const char __user *old', 'const char __user *new']
		case 83: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_symlink_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_symlink_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 84 long sys_oldlstat ['const char __user *filename', 'struct __old_kernel_stat __user *statbuf']
		case 84: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_oldlstat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_oldlstat_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 85 long sys_readlink ['const char __user *path', 'char __user *buf', 'int bufsiz']
		case 85: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_readlink_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_readlink_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 87 long sys_swapon ['const char __user *specialfile', 'int swap_flags']
		case 87: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_swapon_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_swapon_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 88 long sys_reboot ['int magic1', 'int magic2', 'unsigned int cmd', 'void __user *arg']
		case 88: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_reboot_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_reboot_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 89 long sys_readdir ['unsigned int', 'struct old_linux_dirent __user *', 'unsigned int']
		case 89: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_readdir_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_readdir_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 90 long sys_mmap ['unsigned long addr', 'unsigned long len', 'unsigned long prot', 'unsigned long flags', 'unsigned long fd', 'unsigned long pgoff']
		case 90: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_mmap_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mmap_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 91 long sys_munmap ['unsigned long addr', 'size_t len']
		case 91: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_munmap_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_munmap_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 92 long sys_truncate ['const char __user *path', 'long length']
		case 92: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_truncate_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_truncate_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 93 long sys_ftruncate ['unsigned int fd', 'unsigned long length']
		case 93: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_ftruncate_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_ftruncate_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 94 long sys_fchmod ['unsigned int fd', 'umode_t mode']
		case 94: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_fchmod_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fchmod_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 95 long sys_fchown ['unsigned int fd', 'uid_t user', 'gid_t group']
		case 95: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_fchown_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fchown_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 96 long sys_getpriority ['int which', 'int who']
		case 96: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_getpriority_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_getpriority_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 97 long sys_setpriority ['int which', 'int who', 'int niceval']
		case 97: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_setpriority_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_setpriority_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 99 long sys_statfs ['const char __user *path', 'struct statfs __user *buf']
		case 99: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_statfs_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_statfs_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 100 long sys_fstatfs ['unsigned int fd', 'struct statfs __user *buf']
		case 100: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_fstatfs_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fstatfs_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 101 long sys_ioperm ['unsigned long', 'unsigned long', 'int']
		case 101: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_ioperm_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_ioperm_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 102 long sys_socketcall ['int call', 'unsigned long __user *args']
		case 102: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_socketcall_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_socketcall_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 103 long sys_syslog ['int type', 'char __user *buf', 'int len']
		case 103: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_syslog_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_syslog_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 104 long sys_setitimer ['int which', 'struct itimerval __user *value', 'struct itimerval __user *ovalue']
		case 104: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_setitimer_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setitimer_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 105 long sys_getitimer ['int which', 'struct itimerval __user *value']
		case 105: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_getitimer_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getitimer_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 106 long sys_stat ['const char __user *filename', 'struct stat __user *statbuf']
		case 106: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_stat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_stat_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 107 long sys_lstat ['const char __user *filename', 'struct stat __user *statbuf']
		case 107: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_lstat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lstat_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 108 long sys_fstat ['unsigned int fd', 'struct stat __user *statbuf']
		case 108: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_fstat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fstat_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 109 long sys_olduname ['struct oldold_utsname __user *']
		case 109: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_olduname_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_olduname_return, cpu, pc, arg0) ;
		}; break;
		// 110 long sys_iopl ['unsigned int']
		case 110: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_iopl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_iopl_return, cpu, pc, arg0) ;
		}; break;
		// 111 long sys_vhangup ['void']
		case 111: {
			if (PPP_CHECK_CB(on_sys_vhangup_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_vhangup_return, cpu, pc) ;
		}; break;
		// 113 long sys_vm86 ['unsigned long', 'unsigned long']
		case 113: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_vm86_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_vm86_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 114 long sys_wait4 ['pid_t pid', 'int __user *stat_addr', 'int options', 'struct rusage __user *ru']
		case 114: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_wait4_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_wait4_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 115 long sys_swapoff ['const char __user *specialfile']
		case 115: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_swapoff_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_swapoff_return, cpu, pc, arg0) ;
		}; break;
		// 116 long sys_sysinfo ['struct sysinfo __user *info']
		case 116: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_sysinfo_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sysinfo_return, cpu, pc, arg0) ;
		}; break;
		// 117 long sys_ipc ['unsigned int call', 'int first', 'unsigned long second', 'unsigned long third', 'void __user *ptr', 'long fifth']
		case 117: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			int32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_ipc_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_ipc_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 118 long sys_fsync ['unsigned int fd']
		case 118: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_fsync_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fsync_return, cpu, pc, arg0) ;
		}; break;
		// 119 long sys_sigreturn ['void']
		case 119: {
			if (PPP_CHECK_CB(on_sys_sigreturn_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_sigreturn_return, cpu, pc) ;
		}; break;
		// 120 long sys_clone ['unsigned long', 'unsigned long', 'int __user *', 'int __user *', 'unsigned long']
		case 120: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_clone_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clone_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 121 long sys_setdomainname ['char __user *name', 'int len']
		case 121: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_setdomainname_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_setdomainname_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 122 long sys_uname ['struct old_utsname __user *']
		case 122: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_uname_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_uname_return, cpu, pc, arg0) ;
		}; break;
		// 123 long sys_modify_ldt ['int', 'void __user *', 'unsigned long']
		case 123: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_modify_ldt_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_modify_ldt_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 124 long sys_adjtimex ['struct timex __user *txc_p']
		case 124: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_adjtimex_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_adjtimex_return, cpu, pc, arg0) ;
		}; break;
		// 125 long sys_mprotect ['unsigned long start', 'size_t len', 'unsigned long prot']
		case 125: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_mprotect_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mprotect_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 126 long sys_sigprocmask ['int how', 'old_sigset_t __user *set', 'old_sigset_t __user *oset']
		case 126: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sigprocmask_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sigprocmask_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 128 long sys_init_module ['void __user *umod', 'unsigned long len', 'const char __user *uargs']
		case 128: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_init_module_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_init_module_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 129 long sys_delete_module ['const char __user *name_user', 'unsigned int flags']
		case 129: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_delete_module_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_delete_module_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 131 long sys_quotactl ['unsigned int cmd', 'const char __user *special', 'qid_t id', 'void __user *addr']
		case 131: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_quotactl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_quotactl_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 132 long sys_getpgid ['pid_t pid']
		case 132: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_getpgid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_getpgid_return, cpu, pc, arg0) ;
		}; break;
		// 133 long sys_fchdir ['unsigned int fd']
		case 133: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_fchdir_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fchdir_return, cpu, pc, arg0) ;
		}; break;
		// 135 long sys_sysfs ['int option', 'unsigned long arg1', 'unsigned long arg2']
		case 135: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sysfs_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sysfs_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 136 long sys_personality ['unsigned int personality']
		case 136: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_personality_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_personality_return, cpu, pc, arg0) ;
		}; break;
		// 138 long sys_setfsuid ['uid_t uid']
		case 138: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_setfsuid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setfsuid_return, cpu, pc, arg0) ;
		}; break;
		// 139 long sys_setfsgid ['gid_t gid']
		case 139: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_setfsgid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setfsgid_return, cpu, pc, arg0) ;
		}; break;
		// 140 long sys_llseek ['unsigned int fd', 'unsigned long offset_high', 'unsigned long offset_low', 'loff_t __user *result', 'unsigned int whence']
		case 140: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_llseek_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_llseek_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 141 long sys_getdents ['unsigned int fd', 'struct linux_dirent __user *dirent', 'unsigned int count']
		case 141: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_getdents_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getdents_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 143 long sys_flock ['unsigned int fd', 'unsigned int cmd']
		case 143: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_flock_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_flock_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 144 long sys_msync ['unsigned long start', 'size_t len', 'int flags']
		case 144: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_msync_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_msync_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 145 long sys_readv ['unsigned long fd', 'const struct iovec __user *vec', 'unsigned long vlen']
		case 145: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_readv_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_readv_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 146 long sys_writev ['unsigned long fd', 'const struct iovec __user *vec', 'unsigned long vlen']
		case 146: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_writev_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_writev_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 147 long sys_getsid ['pid_t pid']
		case 147: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_getsid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_getsid_return, cpu, pc, arg0) ;
		}; break;
		// 148 long sys_fdatasync ['unsigned int fd']
		case 148: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_fdatasync_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fdatasync_return, cpu, pc, arg0) ;
		}; break;
		// 150 long sys_mlock ['unsigned long start', 'size_t len']
		case 150: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_mlock_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mlock_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 151 long sys_munlock ['unsigned long start', 'size_t len']
		case 151: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_munlock_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_munlock_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 152 long sys_mlockall ['int flags']
		case 152: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_mlockall_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_mlockall_return, cpu, pc, arg0) ;
		}; break;
		// 153 long sys_munlockall ['void']
		case 153: {
			if (PPP_CHECK_CB(on_sys_munlockall_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_munlockall_return, cpu, pc) ;
		}; break;
		// 154 long sys_sched_setparam ['pid_t pid', 'struct sched_param __user *param']
		case 154: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_sched_setparam_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_setparam_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 155 long sys_sched_getparam ['pid_t pid', 'struct sched_param __user *param']
		case 155: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_sched_getparam_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_getparam_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 156 long sys_sched_setscheduler ['pid_t pid', 'int policy', 'struct sched_param __user *param']
		case 156: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sched_setscheduler_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_setscheduler_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 157 long sys_sched_getscheduler ['pid_t pid']
		case 157: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_sched_getscheduler_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_sched_getscheduler_return, cpu, pc, arg0) ;
		}; break;
		// 158 long sys_sched_yield ['void']
		case 158: {
			if (PPP_CHECK_CB(on_sys_sched_yield_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_sched_yield_return, cpu, pc) ;
		}; break;
		// 159 long sys_sched_get_priority_max ['int policy']
		case 159: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_sched_get_priority_max_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_sched_get_priority_max_return, cpu, pc, arg0) ;
		}; break;
		// 160 long sys_sched_get_priority_min ['int policy']
		case 160: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_sched_get_priority_min_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_sched_get_priority_min_return, cpu, pc, arg0) ;
		}; break;
		// 161 long sys_sched_rr_get_interval ['pid_t pid', 'struct timespec __user *interval']
		case 161: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_sched_rr_get_interval_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_rr_get_interval_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 162 long sys_nanosleep ['struct timespec __user *rqtp', 'struct timespec __user *rmtp']
		case 162: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_nanosleep_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_nanosleep_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 163 long sys_mremap ['unsigned long addr', 'unsigned long old_len', 'unsigned long new_len', 'unsigned long flags', 'unsigned long new_addr']
		case 163: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_mremap_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mremap_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 164 long sys_setresuid ['uid_t ruid', 'uid_t euid', 'uid_t suid']
		case 164: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_setresuid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setresuid_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 165 long sys_getresuid ['uid_t __user *ruid', 'uid_t __user *euid', 'uid_t __user *suid']
		case 165: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_getresuid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getresuid_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 167 long sys_poll ['struct pollfd __user *ufds', 'unsigned int nfds', 'int timeout']
		case 167: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_poll_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_poll_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 169 long sys_setresgid ['gid_t rgid', 'gid_t egid', 'gid_t sgid']
		case 169: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_setresgid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_setresgid_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 170 long sys_getresgid ['gid_t __user *rgid', 'gid_t __user *egid', 'gid_t __user *sgid']
		case 170: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_getresgid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getresgid_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 171 long sys_prctl ['int option', 'unsigned long arg2', 'unsigned long arg3', 'unsigned long arg4', 'unsigned long arg5']
		case 171: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_prctl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_prctl_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 172 long sys_rt_sigreturn ['void']
		case 172: {
			if (PPP_CHECK_CB(on_sys_rt_sigreturn_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_rt_sigreturn_return, cpu, pc) ;
		}; break;
		// 173 long sys_rt_sigaction ['int', 'const struct sigaction __user *', 'struct sigaction __user *', 'size_t']
		case 173: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_rt_sigaction_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rt_sigaction_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 174 long sys_rt_sigprocmask ['int how', 'sigset_t __user *set', 'sigset_t __user *oset', 'size_t sigsetsize']
		case 174: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_rt_sigprocmask_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rt_sigprocmask_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 175 long sys_rt_sigpending ['sigset_t __user *set', 'size_t sigsetsize']
		case 175: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_rt_sigpending_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rt_sigpending_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 176 long sys_rt_sigtimedwait ['const sigset_t __user *uthese', 'siginfo_t __user *uinfo', 'const struct timespec __user *uts', 'size_t sigsetsize']
		case 176: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_rt_sigtimedwait_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rt_sigtimedwait_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 177 long sys_rt_sigqueueinfo ['pid_t pid', 'int sig', 'siginfo_t __user *uinfo']
		case 177: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_rt_sigqueueinfo_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rt_sigqueueinfo_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 178 long sys_rt_sigsuspend ['sigset_t __user *unewset', 'size_t sigsetsize']
		case 178: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_rt_sigsuspend_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rt_sigsuspend_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 179 long sys_pread64 ['unsigned int fd', 'char __user *buf', 'size_t count', 'loff_t pos']
		case 179: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint64_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_pread64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sys_pread64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 180 long sys_pwrite64 ['unsigned int fd', 'const char __user *buf', 'size_t count', 'loff_t pos']
		case 180: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint64_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_pwrite64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sys_pwrite64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 181 long sys_chown ['const char __user *filename', 'uid_t user', 'gid_t group']
		case 181: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_chown_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_chown_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 182 long sys_getcwd ['char __user *buf', 'unsigned long size']
		case 182: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_getcwd_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getcwd_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 183 long sys_capget ['cap_user_header_t header', 'cap_user_data_t dataptr']
		case 183: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_capget_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_capget_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 184 long sys_capset ['cap_user_header_t header', 'const cap_user_data_t data']
		case 184: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_capset_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_capset_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 185 long sys_sigaltstack ['const struct sigaltstack __user *uss', 'struct sigaltstack __user *uoss']
		case 185: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_sigaltstack_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sigaltstack_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 186 long sys_sendfile ['int out_fd', 'int in_fd', 'off_t __user *offset', 'size_t count']
		case 186: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_sendfile_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sendfile_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 189 long sys_vfork ['void']
		case 189: {
			if (PPP_CHECK_CB(on_sys_vfork_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_vfork_return, cpu, pc) ;
		}; break;
		// 190 long sys_ugetrlimit ['unsigned int resource', 'struct rlimit __user *rlim']
		case 190: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_ugetrlimit_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_ugetrlimit_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 191 long sys_readahead ['int fd', 'loff_t offset', 'size_t count']
		case 191: {
			int32_t arg0 = 0;
			uint64_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_readahead_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint64_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_readahead_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 192 long sys_mmap2 ['unsigned long addr', 'unsigned long len', 'unsigned long prot', 'unsigned long flags', 'unsigned long fd', 'unsigned long pgoff']
		case 192: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_mmap2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mmap2_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 193 long sys_truncate64 ['const char __user *path', 'loff_t length']
		case 193: {
			uint32_t arg0 = 0;
			uint64_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_truncate64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sys_truncate64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 194 long sys_ftruncate64 ['unsigned int fd', 'loff_t length']
		case 194: {
			uint32_t arg0 = 0;
			uint64_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_ftruncate64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sys_ftruncate64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 195 long sys_stat64 ['const char __user *filename', 'struct stat64 __user *statbuf']
		case 195: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_stat64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_stat64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 196 long sys_lstat64 ['const char __user *filename', 'struct stat64 __user *statbuf']
		case 196: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_lstat64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lstat64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 197 long sys_fstat64 ['unsigned long fd', 'struct stat64 __user *statbuf']
		case 197: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_fstat64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fstat64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 198 long sys_pciconfig_read ['unsigned long bus', 'unsigned long dfn', 'unsigned long off', 'unsigned long len', 'void __user *buf']
		case 198: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_pciconfig_read_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pciconfig_read_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 199 long sys_pciconfig_write ['unsigned long bus', 'unsigned long dfn', 'unsigned long off', 'unsigned long len', 'void __user *buf']
		case 199: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_pciconfig_write_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pciconfig_write_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 200 long sys_pciconfig_iobase ['long which', 'unsigned long bus', 'unsigned long devfn']
		case 200: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_pciconfig_iobase_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pciconfig_iobase_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 201 long sys_multiplexer ['void']
		case 201: {
			if (PPP_CHECK_CB(on_sys_multiplexer_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_multiplexer_return, cpu, pc) ;
		}; break;
		// 202 long sys_getdents64 ['unsigned int fd', 'struct linux_dirent64 __user *dirent', 'unsigned int count']
		case 202: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_getdents64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getdents64_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 203 long sys_pivot_root ['const char __user *new_root', 'const char __user *put_old']
		case 203: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_pivot_root_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pivot_root_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 204 long sys_fcntl64 ['unsigned int fd', 'unsigned int cmd', 'unsigned long arg']
		case 204: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_fcntl64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fcntl64_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 205 long sys_madvise ['unsigned long start', 'size_t len', 'int behavior']
		case 205: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_madvise_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_madvise_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 206 long sys_mincore ['unsigned long start', 'size_t len', 'unsigned char __user *vec']
		case 206: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_mincore_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mincore_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 207 long sys_gettid ['void']
		case 207: {
			if (PPP_CHECK_CB(on_sys_gettid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_gettid_return, cpu, pc) ;
		}; break;
		// 208 long sys_tkill ['pid_t pid', 'int sig']
		case 208: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_tkill_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_tkill_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 209 long sys_setxattr ['const char __user *path', 'const char __user *name', 'const void __user *value', 'size_t size', 'int flags']
		case 209: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_setxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_setxattr_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 210 long sys_lsetxattr ['const char __user *path', 'const char __user *name', 'const void __user *value', 'size_t size', 'int flags']
		case 210: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_lsetxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_lsetxattr_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 211 long sys_fsetxattr ['int fd', 'const char __user *name', 'const void __user *value', 'size_t size', 'int flags']
		case 211: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_fsetxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_fsetxattr_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 212 long sys_getxattr ['const char __user *path', 'const char __user *name', 'void __user *value', 'size_t size']
		case 212: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_getxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getxattr_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 213 long sys_lgetxattr ['const char __user *path', 'const char __user *name', 'void __user *value', 'size_t size']
		case 213: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_lgetxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lgetxattr_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 214 long sys_fgetxattr ['int fd', 'const char __user *name', 'void __user *value', 'size_t size']
		case 214: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_fgetxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fgetxattr_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 215 long sys_listxattr ['const char __user *path', 'char __user *list', 'size_t size']
		case 215: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_listxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_listxattr_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 216 long sys_llistxattr ['const char __user *path', 'char __user *list', 'size_t size']
		case 216: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_llistxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_llistxattr_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 217 long sys_flistxattr ['int fd', 'char __user *list', 'size_t size']
		case 217: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_flistxattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_flistxattr_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 218 long sys_removexattr ['const char __user *path', 'const char __user *name']
		case 218: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_removexattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_removexattr_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 219 long sys_lremovexattr ['const char __user *path', 'const char __user *name']
		case 219: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_lremovexattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lremovexattr_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 220 long sys_fremovexattr ['int fd', 'const char __user *name']
		case 220: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_fremovexattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fremovexattr_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 221 long sys_futex ['u32 __user *uaddr', 'int op', 'u32 val', 'struct timespec __user *utime', 'u32 __user *uaddr2', 'u32 val3']
		case 221: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_futex_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_futex_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 222 long sys_sched_setaffinity ['pid_t pid', 'unsigned int len', 'unsigned long __user *user_mask_ptr']
		case 222: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sched_setaffinity_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_setaffinity_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 223 long sys_sched_getaffinity ['pid_t pid', 'unsigned int len', 'unsigned long __user *user_mask_ptr']
		case 223: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sched_getaffinity_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_getaffinity_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 226 long sys_sendfile64 ['int out_fd', 'int in_fd', 'loff_t __user *offset', 'size_t count']
		case 226: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_sendfile64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sendfile64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 227 long sys_io_setup ['unsigned nr_reqs', 'aio_context_t __user *ctx']
		case 227: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_io_setup_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_setup_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 228 long sys_io_destroy ['aio_context_t ctx']
		case 228: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_io_destroy_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_destroy_return, cpu, pc, arg0) ;
		}; break;
		// 229 long sys_io_getevents ['aio_context_t ctx_id', 'long min_nr', 'long nr', 'struct io_event __user *events', 'struct timespec __user *timeout']
		case 229: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_io_getevents_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_getevents_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 230 long sys_io_submit ['aio_context_t', 'long', 'struct iocb __user * __user *']
		case 230: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_io_submit_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_submit_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 231 long sys_io_cancel ['aio_context_t ctx_id', 'struct iocb __user *iocb', 'struct io_event __user *result']
		case 231: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_io_cancel_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_cancel_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 232 long sys_set_tid_address ['int __user *tidptr']
		case 232: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_set_tid_address_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_set_tid_address_return, cpu, pc, arg0) ;
		}; break;
		// 233 long sys_fadvise64 ['int fd', 'loff_t offset', 'size_t len', 'int advice']
		case 233: {
			int32_t arg0 = 0;
			uint64_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_fadvise64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint64_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_fadvise64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 234 long sys_exit_group ['int error_code']
		case 234: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_exit_group_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_exit_group_return, cpu, pc, arg0) ;
		}; break;
		// 235 long sys_lookup_dcookie ['u64 cookie64', 'char __user *buf', 'size_t len']
		case 235: {
			uint64_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_lookup_dcookie_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint64_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lookup_dcookie_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 236 long sys_epoll_create ['int size']
		case 236: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_epoll_create_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_epoll_create_return, cpu, pc, arg0) ;
		}; break;
		// 237 long sys_epoll_ctl ['int epfd', 'int op', 'int fd', 'struct epoll_event __user *event']
		case 237: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_epoll_ctl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_epoll_ctl_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 238 long sys_epoll_wait ['int epfd', 'struct epoll_event __user *events', 'int maxevents', 'int timeout']
		case 238: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_epoll_wait_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_epoll_wait_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 239 long sys_remap_file_pages ['unsigned long start', 'unsigned long size', 'unsigned long prot', 'unsigned long pgoff', 'unsigned long flags']
		case 239: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_remap_file_pages_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_remap_file_pages_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 240 long sys_timer_create ['clockid_t which_clock', 'struct sigevent __user *timer_event_spec', 'timer_t __user *created_timer_id']
		case 240: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_timer_create_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timer_create_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 241 long sys_timer_settime ['timer_t timer_id', 'int flags', 'const struct itimerspec __user *new_setting', 'struct itimerspec __user *old_setting']
		case 241: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_timer_settime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timer_settime_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 242 long sys_timer_gettime ['timer_t timer_id', 'struct itimerspec __user *setting']
		case 242: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_timer_gettime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timer_gettime_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 243 long sys_timer_getoverrun ['timer_t timer_id']
		case 243: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_timer_getoverrun_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timer_getoverrun_return, cpu, pc, arg0) ;
		}; break;
		// 244 long sys_timer_delete ['timer_t timer_id']
		case 244: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_timer_delete_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timer_delete_return, cpu, pc, arg0) ;
		}; break;
		// 245 long sys_clock_settime ['clockid_t which_clock', 'const struct timespec __user *tp']
		case 245: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clock_settime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_settime_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 246 long sys_clock_gettime ['clockid_t which_clock', 'struct timespec __user *tp']
		case 246: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clock_gettime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_gettime_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 247 long sys_clock_getres ['clockid_t which_clock', 'struct timespec __user *tp']
		case 247: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clock_getres_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_getres_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 248 long sys_clock_nanosleep ['clockid_t which_clock', 'int flags', 'const struct timespec __user *rqtp', 'struct timespec __user *rmtp']
		case 248: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_clock_nanosleep_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_nanosleep_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 249 long sys_swapcontext ['struct ucontext __user *old_ctx', 'struct ucontext __user *new_ctx', 'int ctx_size']
		case 249: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_swapcontext_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_swapcontext_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 250 long sys_tgkill ['pid_t tgid', 'pid_t pid', 'int sig']
		case 250: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_tgkill_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_tgkill_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 251 long sys_utimes ['char __user *filename', 'struct timeval __user *utimes']
		case 251: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_utimes_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_utimes_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 252 long sys_statfs64 ['const char __user *path', 'size_t sz', 'struct statfs64 __user *buf']
		case 252: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_statfs64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_statfs64_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 253 long sys_fstatfs64 ['unsigned int fd', 'size_t sz', 'struct statfs64 __user *buf']
		case 253: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_fstatfs64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fstatfs64_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 254 long sys_fadvise64_64 ['int fd', 'loff_t offset', 'loff_t len', 'int advice']
		case 254: {
			int32_t arg0 = 0;
			uint64_t arg1 = 0;
			uint64_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_fadvise64_64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint64_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint64_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_fadvise64_64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 255 long sys_rtas ['struct rtas_args __user *uargs']
		case 255: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_rtas_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rtas_return, cpu, pc, arg0) ;
		}; break;
		// 256 long sys_sys_debug_setcontext ['struct ucontext __user *ctx', 'int ndbg', 'struct sig_dbg_op __user *dbg', 'int r4', 'int r5', 'int r6']
		case 256: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			int32_t arg4 = 0;
			int32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_sys_debug_setcontext_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
				memcpy(&arg5, ctx->args[5], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_sys_debug_setcontext_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 258 long sys_migrate_pages ['pid_t pid', 'unsigned long maxnode', 'const unsigned long __user *from', 'const unsigned long __user *to']
		case 258: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_migrate_pages_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_migrate_pages_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 259 long sys_mbind ['unsigned long start', 'unsigned long len', 'unsigned long mode', 'const unsigned long __user *nmask', 'unsigned long maxnode', 'unsigned flags']
		case 259: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_mbind_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mbind_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 260 long sys_get_mempolicy ['int __user *policy', 'unsigned long __user *nmask', 'unsigned long maxnode', 'unsigned long addr', 'unsigned long flags']
		case 260: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_get_mempolicy_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_get_mempolicy_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 261 long sys_set_mempolicy ['int mode', 'const unsigned long __user *nmask', 'unsigned long maxnode']
		case 261: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_set_mempolicy_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_set_mempolicy_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 262 long sys_mq_open ['const char __user *name', 'int oflag', 'umode_t mode', 'struct mq_attr __user *attr']
		case 262: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_mq_open_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mq_open_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 263 long sys_mq_unlink ['const char __user *name']
		case 263: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_mq_unlink_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mq_unlink_return, cpu, pc, arg0) ;
		}; break;
		// 264 long sys_mq_timedsend ['mqd_t mqdes', 'const char __user *msg_ptr', 'size_t msg_len', 'unsigned int msg_prio', 'const struct timespec __user *abs_timeout']
		case 264: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_mq_timedsend_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mq_timedsend_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 265 long sys_mq_timedreceive ['mqd_t mqdes', 'char __user *msg_ptr', 'size_t msg_len', 'unsigned int __user *msg_prio', 'const struct timespec __user *abs_timeout']
		case 265: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_mq_timedreceive_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mq_timedreceive_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 266 long sys_mq_notify ['mqd_t mqdes', 'const struct sigevent __user *notification']
		case 266: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_mq_notify_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mq_notify_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 267 long sys_mq_getsetattr ['mqd_t mqdes', 'const struct mq_attr __user *mqstat', 'struct mq_attr __user *omqstat']
		case 267: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_mq_getsetattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mq_getsetattr_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 268 long sys_kexec_load ['unsigned long entry', 'unsigned long nr_segments', 'struct kexec_segment __user *segments', 'unsigned long flags']
		case 268: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_kexec_load_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_kexec_load_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 269 long sys_add_key ['const char __user *_type', 'const char __user *_description', 'const void __user *_payload', 'size_t plen', 'key_serial_t destringid']
		case 269: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_add_key_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_add_key_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 270 long sys_request_key ['const char __user *_type', 'const char __user *_description', 'const char __user *_callout_info', 'key_serial_t destringid']
		case 270: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_request_key_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_request_key_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 271 long sys_keyctl ['int cmd', 'unsigned long arg2', 'unsigned long arg3', 'unsigned long arg4', 'unsigned long arg5']
		case 271: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_keyctl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_keyctl_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 272 long sys_waitid ['int which', 'pid_t pid', 'struct siginfo __user *infop', 'int options', 'struct rusage __user *ru']
		case 272: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_waitid_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_waitid_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 273 long sys_ioprio_set ['int which', 'int who', 'int ioprio']
		case 273: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_ioprio_set_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_ioprio_set_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 274 long sys_ioprio_get ['int which', 'int who']
		case 274: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_ioprio_get_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_ioprio_get_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 275 long sys_inotify_init ['void']
		case 275: {
			if (PPP_CHECK_CB(on_sys_inotify_init_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_inotify_init_return, cpu, pc) ;
		}; break;
		// 276 long sys_inotify_add_watch ['int fd', 'const char __user *path', 'u32 mask']
		case 276: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_inotify_add_watch_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_inotify_add_watch_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 277 long sys_inotify_rm_watch ['int fd', '__s32 wd']
		case 277: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_inotify_rm_watch_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_inotify_rm_watch_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 278 long sys_spu_run ['int fd', '__u32 __user *unpc', '__u32 __user *ustatus']
		case 278: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_spu_run_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_spu_run_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 279 long sys_spu_create ['const char __user *name', 'unsigned int flags', 'mode_t mode', 'int neighbor_fd']
		case 279: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_spu_create_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_spu_create_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 280 long sys_pselect6 ['int', 'fd_set __user *', 'fd_set __user *', 'fd_set __user *', 'struct timespec __user *', 'void __user *']
		case 280: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_pselect6_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pselect6_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 281 long sys_ppoll ['struct pollfd __user *', 'unsigned int', 'struct timespec __user *', 'const sigset_t __user *', 'size_t']
		case 281: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_ppoll_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_ppoll_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 282 long sys_unshare ['unsigned long unshare_flags']
		case 282: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_unshare_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_unshare_return, cpu, pc, arg0) ;
		}; break;
		// 283 long sys_splice ['int fd_in', 'loff_t __user *off_in', 'int fd_out', 'loff_t __user *off_out', 'size_t len', 'unsigned int flags']
		case 283: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_splice_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_splice_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 284 long sys_tee ['int fdin', 'int fdout', 'size_t len', 'unsigned int flags']
		case 284: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_tee_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_tee_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 285 long sys_vmsplice ['int fd', 'const struct iovec __user *iov', 'unsigned long nr_segs', 'unsigned int flags']
		case 285: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_vmsplice_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_vmsplice_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 286 long sys_openat ['int dfd', 'const char __user *filename', 'int flags', 'umode_t mode']
		case 286: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_openat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_openat_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 287 long sys_mkdirat ['int dfd', 'const char __user *pathname', 'umode_t mode']
		case 287: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_mkdirat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mkdirat_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 288 long sys_mknodat ['int dfd', 'const char __user *filename', 'umode_t mode', 'unsigned dev']
		case 288: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_mknodat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mknodat_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 289 long sys_fchownat ['int dfd', 'const char __user *filename', 'uid_t user', 'gid_t group', 'int flag']
		case 289: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_fchownat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_fchownat_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 290 long sys_futimesat ['int dfd', 'const char __user *filename', 'struct timeval __user *utimes']
		case 290: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_futimesat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_futimesat_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 291 long sys_fstatat64 ['int dfd', 'const char __user *filename', 'struct stat64 __user *statbuf', 'int flag']
		case 291: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_fstatat64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_fstatat64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 292 long sys_unlinkat ['int dfd', 'const char __user *pathname', 'int flag']
		case 292: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_unlinkat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_unlinkat_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 293 long sys_renameat ['int olddfd', 'const char __user *oldname', 'int newdfd', 'const char __user *newname']
		case 293: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_renameat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_renameat_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 294 long sys_linkat ['int olddfd', 'const char __user *oldname', 'int newdfd', 'const char __user *newname', 'int flags']
		case 294: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_linkat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_linkat_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 295 long sys_symlinkat ['const char __user *oldname', 'int newdfd', 'const char __user *newname']
		case 295: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_symlinkat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_symlinkat_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 296 long sys_readlinkat ['int dfd', 'const char __user *path', 'char __user *buf', 'int bufsiz']
		case 296: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_readlinkat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_readlinkat_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 297 long sys_fchmodat ['int dfd', 'const char __user *filename', 'umode_t mode']
		case 297: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_fchmodat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fchmodat_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 298 long sys_faccessat ['int dfd', 'const char __user *filename', 'int mode']
		case 298: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_faccessat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_faccessat_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 299 long sys_get_robust_list ['int pid', 'struct robust_list_head __user * __user *head_ptr', 'size_t __user *len_ptr']
		case 299: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_get_robust_list_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_get_robust_list_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 300 long sys_set_robust_list ['struct robust_list_head __user *head', 'size_t len']
		case 300: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_set_robust_list_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_set_robust_list_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 301 long sys_move_pages ['pid_t pid', 'unsigned long nr_pages', 'const void __user * __user *pages', 'const int __user *nodes', 'int __user *status', 'int flags']
		case 301: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			int32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_move_pages_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_move_pages_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 302 long sys_getcpu ['unsigned __user *cpu', 'unsigned __user *node', 'struct getcpu_cache __user *cache']
		case 302: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_getcpu_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getcpu_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 303 long sys_epoll_pwait ['int epfd', 'struct epoll_event __user *events', 'int maxevents', 'int timeout', 'const sigset_t __user *sigmask', 'size_t sigsetsize']
		case 303: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			int32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_epoll_pwait_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_epoll_pwait_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 304 long sys_utimensat ['int dfd', 'const char __user *filename', 'struct timespec __user *utimes', 'int flags']
		case 304: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_utimensat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_utimensat_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 305 long sys_signalfd ['int ufd', 'sigset_t __user *user_mask', 'size_t sizemask']
		case 305: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_signalfd_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_signalfd_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 306 long sys_timerfd_create ['int clockid', 'int flags']
		case 306: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_timerfd_create_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_timerfd_create_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 307 long sys_eventfd ['unsigned int count']
		case 307: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_eventfd_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_eventfd_return, cpu, pc, arg0) ;
		}; break;
		// 308 long sys_sync_file_range2 ['int fd', 'unsigned int flags', 'loff_t offset', 'loff_t nbytes']
		case 308: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint64_t arg2 = 0;
			uint64_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_sync_file_range2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint64_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sys_sync_file_range2_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 309 long sys_fallocate ['int fd', 'int mode', 'loff_t offset', 'loff_t len']
		case 309: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint64_t arg2 = 0;
			uint64_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_fallocate_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint64_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint64_t));
			}
			PPP_RUN_CB(on_sys_fallocate_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 310 long sys_subpage_prot ['unsigned long addr', 'unsigned long len', 'uint32_t __user *map']
		case 310: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_subpage_prot_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_subpage_prot_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 311 long sys_timerfd_settime ['int ufd', 'int flags', 'const struct itimerspec __user *utmr', 'struct itimerspec __user *otmr']
		case 311: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_timerfd_settime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timerfd_settime_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 312 long sys_timerfd_gettime ['int ufd', 'struct itimerspec __user *otmr']
		case 312: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_timerfd_gettime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timerfd_gettime_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 313 long sys_signalfd4 ['int ufd', 'sigset_t __user *user_mask', 'size_t sizemask', 'int flags']
		case 313: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_signalfd4_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_signalfd4_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 314 long sys_eventfd2 ['unsigned int count', 'int flags']
		case 314: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_eventfd2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_eventfd2_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 315 long sys_epoll_create1 ['int flags']
		case 315: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_epoll_create1_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_epoll_create1_return, cpu, pc, arg0) ;
		}; break;
		// 316 long sys_dup3 ['unsigned int oldfd', 'unsigned int newfd', 'int flags']
		case 316: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_dup3_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_dup3_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 317 long sys_pipe2 ['int __user *fildes', 'int flags']
		case 317: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_pipe2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_pipe2_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 318 long sys_inotify_init1 ['int flags']
		case 318: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_inotify_init1_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_inotify_init1_return, cpu, pc, arg0) ;
		}; break;
		// 319 long sys_perf_event_open ['struct perf_event_attr __user *attr_uptr', 'pid_t pid', 'int cpu', 'int group_fd', 'unsigned long flags']
		case 319: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			int32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_perf_event_open_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_perf_event_open_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 320 long sys_preadv ['unsigned long fd', 'const struct iovec __user *vec', 'unsigned long vlen', 'unsigned long pos_l', 'unsigned long pos_h']
		case 320: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_preadv_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_preadv_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 321 long sys_pwritev ['unsigned long fd', 'const struct iovec __user *vec', 'unsigned long vlen', 'unsigned long pos_l', 'unsigned long pos_h']
		case 321: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_pwritev_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pwritev_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 322 long sys_rt_tgsigqueueinfo ['pid_t tgid', 'pid_t pid', 'int sig', 'siginfo_t __user *uinfo']
		case 322: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_rt_tgsigqueueinfo_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rt_tgsigqueueinfo_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 323 long sys_fanotify_init ['unsigned int flags', 'unsigned int event_f_flags']
		case 323: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_fanotify_init_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fanotify_init_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 324 long sys_fanotify_mark ['int fanotify_fd', 'unsigned int flags', 'u64 mask', 'int fd', 'const char __user *pathname']
		case 324: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint64_t arg2 = 0;
			int32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_fanotify_mark_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint64_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fanotify_mark_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 325 long sys_prlimit64 ['pid_t pid', 'unsigned int resource', 'const struct rlimit64 __user *new_rlim', 'struct rlimit64 __user *old_rlim']
		case 325: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_prlimit64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_prlimit64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 326 long sys_socket ['int', 'int', 'int']
		case 326: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_socket_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_socket_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 327 long sys_bind ['int', 'struct sockaddr __user *', 'int']
		case 327: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_bind_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_bind_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 328 long sys_connect ['int', 'struct sockaddr __user *', 'int']
		case 328: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_connect_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_connect_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 329 long sys_listen ['int', 'int']
		case 329: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_listen_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_listen_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 330 long sys_accept ['int', 'struct sockaddr __user *', 'int __user *']
		case 330: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_accept_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_accept_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 331 long sys_getsockname ['int', 'struct sockaddr __user *', 'int __user *']
		case 331: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_getsockname_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getsockname_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 332 long sys_getpeername ['int', 'struct sockaddr __user *', 'int __user *']
		case 332: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_getpeername_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getpeername_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 333 long sys_socketpair ['int', 'int', 'int', 'int __user *']
		case 333: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_socketpair_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_socketpair_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 334 long sys_send ['int', 'void __user *', 'size_t', 'unsigned']
		case 334: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_send_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_send_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 335 long sys_sendto ['int', 'void __user *', 'size_t', 'unsigned', 'struct sockaddr __user *', 'int']
		case 335: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			int32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_sendto_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_sendto_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 336 long sys_recv ['int', 'void __user *', 'size_t', 'unsigned']
		case 336: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_recv_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_recv_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 337 long sys_recvfrom ['int', 'void __user *', 'size_t', 'unsigned', 'struct sockaddr __user *', 'int __user *']
		case 337: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_recvfrom_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_recvfrom_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 338 long sys_shutdown ['int', 'int']
		case 338: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_shutdown_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_shutdown_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 339 long sys_setsockopt ['int fd', 'int level', 'int optname', 'char __user *optval', 'int optlen']
		case 339: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_setsockopt_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_setsockopt_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 340 long sys_getsockopt ['int fd', 'int level', 'int optname', 'char __user *optval', 'int __user *optlen']
		case 340: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_getsockopt_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getsockopt_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 341 long sys_sendmsg ['int fd', 'struct user_msghdr __user *msg', 'unsigned flags']
		case 341: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sendmsg_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sendmsg_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 342 long sys_recvmsg ['int fd', 'struct user_msghdr __user *msg', 'unsigned flags']
		case 342: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_recvmsg_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_recvmsg_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 343 long sys_recvmmsg ['int fd', 'struct mmsghdr __user *msg', 'unsigned int vlen', 'unsigned flags', 'struct timespec __user *timeout']
		case 343: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_recvmmsg_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_recvmmsg_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 344 long sys_accept4 ['int', 'struct sockaddr __user *', 'int __user *', 'int']
		case 344: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_accept4_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_accept4_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 345 long sys_name_to_handle_at ['int dfd', 'const char __user *name', 'struct file_handle __user *handle', 'int __user *mnt_id', 'int flag']
		case 345: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_name_to_handle_at_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_name_to_handle_at_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 346 long sys_open_by_handle_at ['int mountdirfd', 'struct file_handle __user *handle', 'int flags']
		case 346: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_open_by_handle_at_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_open_by_handle_at_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 347 long sys_clock_adjtime ['clockid_t which_clock', 'struct timex __user *tx']
		case 347: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clock_adjtime_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_adjtime_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 348 long sys_syncfs ['int fd']
		case 348: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_syncfs_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_syncfs_return, cpu, pc, arg0) ;
		}; break;
		// 349 long sys_sendmmsg ['int fd', 'struct mmsghdr __user *msg', 'unsigned int vlen', 'unsigned flags']
		case 349: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_sendmmsg_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sendmmsg_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 350 long sys_setns ['int fd', 'int nstype']
		case 350: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_setns_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_setns_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 351 long sys_process_vm_readv ['pid_t pid', 'const struct iovec __user *lvec', 'unsigned long liovcnt', 'const struct iovec __user *rvec', 'unsigned long riovcnt', 'unsigned long flags']
		case 351: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_process_vm_readv_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_process_vm_readv_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 352 long sys_process_vm_writev ['pid_t pid', 'const struct iovec __user *lvec', 'unsigned long liovcnt', 'const struct iovec __user *rvec', 'unsigned long riovcnt', 'unsigned long flags']
		case 352: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_process_vm_writev_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_process_vm_writev_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 353 long sys_finit_module ['int fd', 'const char __user *uargs', 'int flags']
		case 353: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_finit_module_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_finit_module_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 354 long sys_kcmp ['pid_t pid1', 'pid_t pid2', 'int type', 'unsigned long idx1', 'unsigned long idx2']
		case 354: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_kcmp_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_kcmp_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 355 long sys_sched_setattr ['pid_t pid', 'struct sched_attr __user *attr', 'unsigned int flags']
		case 355: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_sched_setattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_setattr_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 356 long sys_sched_getattr ['pid_t pid', 'struct sched_attr __user *attr', 'unsigned int size', 'unsigned int flags']
		case 356: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_sched_getattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_getattr_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 357 long sys_renameat2 ['int olddfd', 'const char __user *oldname', 'int newdfd', 'const char __user *newname', 'unsigned int flags']
		case 357: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_renameat2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_renameat2_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 358 long sys_seccomp ['unsigned int op', 'unsigned int flags', 'const char __user *uargs']
		case 358: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_seccomp_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_seccomp_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 359 long sys_getrandom ['char __user *buf', 'size_t count', 'unsigned int flags']
		case 359: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_getrandom_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_getrandom_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 360 long sys_memfd_create ['const char __user *uname_ptr', 'unsigned int flags']
		case 360: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_memfd_create_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_memfd_create_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 361 long sys_bpf ['int cmd', 'union bpf_attr *attr', 'unsigned int size']
		case 361: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_bpf_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_bpf_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 362 long sys_execveat ['int dfd', 'const char __user *filename', 'const char __user *const __user *argv', 'const char __user *const __user *envp', 'int flags']
		case 362: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_execveat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_execveat_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 363 long sys_switch_endian ['void']
		case 363: {
			if (PPP_CHECK_CB(on_sys_switch_endian_return) || PPP_CHECK_CB(on_all_sys_return2)) {
			}
			PPP_RUN_CB(on_sys_switch_endian_return, cpu, pc) ;
		}; break;
		// 364 long sys_userfaultfd ['int flags']
		case 364: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_userfaultfd_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_userfaultfd_return, cpu, pc, arg0) ;
		}; break;
		// 365 long sys_membarrier ['int cmd', 'int flags']
		case 365: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_membarrier_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_membarrier_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 378 long sys_mlock2 ['unsigned long start', 'size_t len', 'int flags']
		case 378: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_mlock2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_mlock2_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 379 long sys_copy_file_range ['int fd_in', 'loff_t __user *off_in', 'int fd_out', 'loff_t __user *off_out', 'size_t len', 'unsigned int flags']
		case 379: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_copy_file_range_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_copy_file_range_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 380 long sys_preadv2 ['unsigned long fd', 'const struct iovec __user *vec', 'unsigned long vlen', 'unsigned long pos_l', 'unsigned long pos_h', 'rwf_t flags']
		case 380: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_preadv2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_preadv2_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 381 long sys_pwritev2 ['unsigned long fd', 'const struct iovec __user *vec', 'unsigned long vlen', 'unsigned long pos_l', 'unsigned long pos_h', 'rwf_t flags']
		case 381: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_pwritev2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pwritev2_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 382 long sys_kexec_file_load ['int kernel_fd', 'int initrd_fd', 'unsigned long cmdline_len', 'const char __user *cmdline_ptr', 'unsigned long flags']
		case 382: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_kexec_file_load_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_kexec_file_load_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 383 long sys_statx ['int dfd', 'const char __user *path', 'unsigned flags', 'unsigned mask', 'struct statx __user *buffer']
		case 383: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_statx_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_statx_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 384 long sys_pkey_alloc ['unsigned long flags', 'unsigned long init_val']
		case 384: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_pkey_alloc_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pkey_alloc_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 385 long sys_pkey_free ['int pkey']
		case 385: {
			int32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_pkey_free_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_pkey_free_return, cpu, pc, arg0) ;
		}; break;
		// 386 long sys_pkey_mprotect ['unsigned long start', 'size_t len', 'unsigned long prot', 'int pkey']
		case 386: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_pkey_mprotect_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_pkey_mprotect_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 387 long sys_rseq ['struct rseq __user *rseq', 'u32 rseq_len', 'int flags', 'u32 sig']
		case 387: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_rseq_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rseq_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 388 long sys_io_pgetevents ['aio_context_t ctx_id', 'long min_nr', 'long nr', 'struct io_event __user *events', 'struct timespec __user *timeout', 'const struct io_sigset __user *usig']
		case 388: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_io_pgetevents_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_pgetevents_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 393 long sys_semget ['key_t key', 'int nsems', 'int semflg']
		case 393: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_semget_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_semget_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 394 long sys_semctl ['int semid', 'int semnum', 'int cmd', 'unsigned long arg']
		case 394: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_semctl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_semctl_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 395 long sys_shmget ['key_t key', 'size_t size', 'int flag']
		case 395: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_shmget_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_shmget_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 396 long sys_shmctl ['int shmid', 'int cmd', 'struct shmid_ds __user *buf']
		case 396: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_shmctl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_shmctl_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 397 long sys_shmat ['int shmid', 'char __user *shmaddr', 'int shmflg']
		case 397: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_shmat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_shmat_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 398 long sys_shmdt ['char __user *shmaddr']
		case 398: {
			uint32_t arg0 = 0;
			if (PPP_CHECK_CB(on_sys_shmdt_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_shmdt_return, cpu, pc, arg0) ;
		}; break;
		// 399 long sys_msgget ['key_t key', 'int msgflg']
		case 399: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_msgget_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_msgget_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 400 long sys_msgsnd ['int msqid', 'struct msgbuf __user *msgp', 'size_t msgsz', 'int msgflg']
		case 400: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_msgsnd_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_msgsnd_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 401 long sys_msgrcv ['int msqid', 'struct msgbuf __user *msgp', 'size_t msgsz', 'long msgtyp', 'int msgflg']
		case 401: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_msgrcv_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_msgrcv_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 402 long sys_msgctl ['int msqid', 'int cmd', 'struct msqid_ds __user *buf']
		case 402: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_msgctl_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_msgctl_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 403 long sys_clock_gettime64 ['clockid_t which_clock', 'struct timespec64 __user *tp']
		case 403: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clock_gettime64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_gettime64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 404 long sys_clock_settime64 ['clockid_t which_clock', 'const struct timespec64 __user *tp']
		case 404: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clock_settime64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_settime64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 405 long sys_clock_adjtime64 ['clockid_t which_clock', 'struct timex64 __user *tx']
		case 405: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clock_adjtime64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_adjtime64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 406 long sys_clock_getres_time64 ['clockid_t which_clock', 'struct timespec64 __user *tp']
		case 406: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clock_getres_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_getres_time64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 407 long sys_clock_nanosleep_time64 ['clockid_t which_clock', 'int flags', 'const struct timespec64 __user *rqtp', 'struct timespec64 __user *rmtp']
		case 407: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_clock_nanosleep_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clock_nanosleep_time64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 408 long sys_timer_gettime64 ['timer_t timer_id', 'struct itimerspec64 __user *setting']
		case 408: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_timer_gettime64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timer_gettime64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 409 long sys_timer_settime64 ['timer_t timer_id', 'int flags', 'const struct itimerspec64 __user *new_setting', 'struct itimerspec64 __user *old_setting']
		case 409: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_timer_settime64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timer_settime64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 410 long sys_timerfd_gettime64 ['int ufd', 'struct itimerspec64 __user *otmr']
		case 410: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_timerfd_gettime64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timerfd_gettime64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 411 long sys_timerfd_settime64 ['int ufd', 'int flags', 'const struct itimerspec64 __user *utmr', 'struct itimerspec64 __user *otmr']
		case 411: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_timerfd_settime64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_timerfd_settime64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 412 long sys_utimensat_time64 ['int dfd', 'const char __user *filename', 'struct timespec64 __user *utimes', 'int flags']
		case 412: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_utimensat_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_utimensat_time64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 413 long sys_pselect6_time64 ['int', 'fd_set __user *', 'fd_set __user *', 'fd_set __user *', 'struct timespec64 __user *', 'void __user *']
		case 413: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_pselect6_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pselect6_time64_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 414 long sys_ppoll_time64 ['struct pollfd __user *', 'unsigned int', 'struct timespec64 __user *', 'const sigset_t __user *', 'size_t']
		case 414: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_ppoll_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_ppoll_time64_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 416 long sys_io_pgetevents_time64 ['aio_context_t ctx_id', 'long min_nr', 'long nr', 'struct io_event __user *events', 'struct timespec64 __user *timeout', 'const struct io_sigset __user *usig']
		case 416: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_io_pgetevents_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_pgetevents_time64_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 417 long sys_recvmmsg_time64 ['int fd', 'struct mmsghdr __user *msg', 'unsigned int vlen', 'unsigned flags', 'struct timespec64 __user *timeout']
		case 417: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_recvmmsg_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_recvmmsg_time64_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 418 long sys_mq_timedsend_time64 ['mqd_t mqdes', 'const char __user *msg_ptr', 'size_t msg_len', 'unsigned int msg_prio', 'const struct timespec64 __user *abs_timeout']
		case 418: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_mq_timedsend_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mq_timedsend_time64_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 419 long sys_mq_timedreceive_time64 ['mqd_t mqdes', 'char __user *msg_ptr', 'size_t msg_len', 'unsigned int __user *msg_prio', 'const struct timespec64 __user *abs_timeout']
		case 419: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_mq_timedreceive_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mq_timedreceive_time64_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 420 long sys_semtimedop_time64 ['int semid', 'struct sembuf __user *sops', 'unsigned nsops', 'const struct timespec64 __user *timeout']
		case 420: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_semtimedop_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_semtimedop_time64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 421 long sys_rt_sigtimedwait_time64 ['const sigset_t __user *uthese', 'siginfo_t __user *uinfo', 'const struct timespec64 __user *uts', 'size_t sigsetsize']
		case 421: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_rt_sigtimedwait_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_rt_sigtimedwait_time64_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 422 long sys_futex_time64 ['u32 __user *uaddr', 'int op', 'u32 val', 'struct timespec64 __user *utime', 'u32 __user *uaddr2', 'u32 val3']
		case 422: {
			uint32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_futex_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_futex_time64_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 423 long sys_sched_rr_get_interval_time64 ['pid_t pid', 'struct timespec64 __user *interval']
		case 423: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_sched_rr_get_interval_time64_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_sched_rr_get_interval_time64_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 424 long sys_pidfd_send_signal ['int pidfd', 'int sig', 'siginfo_t __user *info', 'unsigned int flags']
		case 424: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_pidfd_send_signal_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pidfd_send_signal_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 425 long sys_io_uring_setup ['u32 entries', 'struct io_uring_params __user *p']
		case 425: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_io_uring_setup_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_uring_setup_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 426 long sys_io_uring_enter ['unsigned int fd', 'u32 to_submit', 'u32 min_complete', 'u32 flags', 'const sigset_t __user *sig', 'size_t sigsz']
		case 426: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_io_uring_enter_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_uring_enter_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 427 long sys_io_uring_register ['unsigned int fd', 'unsigned int opcode', 'void __user *arg', 'unsigned int nr_args']
		case 427: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_io_uring_register_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_io_uring_register_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 428 long sys_open_tree ['int dfd', 'const char __user *filename', 'unsigned flags']
		case 428: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_open_tree_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_open_tree_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 429 long sys_move_mount ['int from_dfd', 'const char __user *from_pathname', 'int to_dfd', 'const char __user *to_pathname', 'unsigned int flags']
		case 429: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_move_mount_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_move_mount_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 430 long sys_fsopen ['const char __user *fs_name', 'unsigned int flags']
		case 430: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_fsopen_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fsopen_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 431 long sys_fsconfig ['int fd', 'unsigned int cmd', 'const char __user *key', 'const void __user *value', 'int aux']
		case 431: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_fsconfig_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_fsconfig_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 432 long sys_fsmount ['int fs_fd', 'unsigned int flags', 'unsigned int attr_flags']
		case 432: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_fsmount_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fsmount_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 433 long sys_fspick ['int dfd', 'const char __user *path', 'unsigned int flags']
		case 433: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_fspick_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_fspick_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 434 long sys_pidfd_open ['pid_t pid', 'unsigned int flags']
		case 434: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_pidfd_open_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pidfd_open_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 435 long sys_clone3 ['struct clone_args __user *uargs', 'size_t size']
		case 435: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_clone3_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_clone3_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 436 long sys_close_range ['unsigned int fd', 'unsigned int max_fd', 'unsigned int flags']
		case 436: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_close_range_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_close_range_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 437 long sys_openat2 ['int dfd', 'const char __user *filename', 'struct open_how __user *how', 'size_t size']
		case 437: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_openat2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_openat2_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 438 long sys_pidfd_getfd ['int pidfd', 'int fd', 'unsigned int flags']
		case 438: {
			int32_t arg0 = 0;
			int32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_pidfd_getfd_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(int32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_pidfd_getfd_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 439 long sys_faccessat2 ['int dfd', 'const char __user *filename', 'int mode', 'int flags']
		case 439: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_faccessat2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_faccessat2_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 440 long sys_process_madvise ['int pidfd', 'const struct iovec __user *vec', 'size_t vlen', 'int behavior', 'unsigned int flags']
		case 440: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_process_madvise_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_process_madvise_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 441 long sys_epoll_pwait2 ['int epfd', 'struct epoll_event __user *events', 'int maxevents', 'const struct timespec64 __user *timeout', 'const sigset_t __user *sigmask', 'size_t sigsetsize']
		case 441: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			int32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			uint32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_epoll_pwait2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(int32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_epoll_pwait2_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 442 long sys_mount_setattr ['int dfd', 'const char __user *path', 'unsigned int flags', 'struct mount_attr __user *uattr', 'size_t usize']
		case 442: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_mount_setattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mount_setattr_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 443 long sys_quotactl_fd ['unsigned int fd', 'unsigned int cmd', 'qid_t id', 'void __user *addr']
		case 443: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_quotactl_fd_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_quotactl_fd_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 444 long sys_landlock_create_ruleset ['const struct landlock_ruleset_attr __user *attr', 'size_t size', '__u32 flags']
		case 444: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_landlock_create_ruleset_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_landlock_create_ruleset_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 445 long sys_landlock_add_rule ['int ruleset_fd', 'enum landlock_rule_type rule_type', 'const void __user *rule_attr', '__u32 flags']
		case 445: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_landlock_add_rule_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_landlock_add_rule_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 446 long sys_landlock_restrict_self ['int ruleset_fd', '__u32 flags']
		case 446: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_landlock_restrict_self_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_landlock_restrict_self_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 448 long sys_process_mrelease ['int pidfd', 'unsigned int flags']
		case 448: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			if (PPP_CHECK_CB(on_sys_process_mrelease_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_process_mrelease_return, cpu, pc, arg0, arg1) ;
		}; break;
		// 449 long sys_futex_waitv ['struct futex_waitv __user *waiters', 'unsigned int nr_futexes', 'unsigned int flags', 'struct timespec __user *timo', 'clockid_t clockid']
		case 449: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_futex_waitv_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_futex_waitv_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 450 long sys_set_mempolicy_home_node ['unsigned long start', 'unsigned long len', 'unsigned long home_node', 'unsigned long flags']
		case 450: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_set_mempolicy_home_node_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_set_mempolicy_home_node_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 451 long sys_cachestat ['unsigned int fd', 'struct cachestat_range __user *cstat_range', 'struct cachestat __user *cstat', 'unsigned int flags']
		case 451: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_cachestat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_cachestat_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 452 long sys_fchmodat2 ['int dfd', 'const char __user *filename', 'umode_t mode', 'int flags']
		case 452: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_fchmodat2_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_fchmodat2_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 453 long sys_map_shadow_stack ['unsigned long addr', 'unsigned long size', 'unsigned int flags']
		case 453: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_map_shadow_stack_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_map_shadow_stack_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 454 long sys_futex_wake ['u32 __user *uaddr', 'u32 mask', 'u32 val']
		case 454: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_futex_wake_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_futex_wake_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 455 long sys_futex_wait ['u32 __user *uaddr', 'u32 val', 'u32 mask', 'struct timespec __user *timeout']
		case 455: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_futex_wait_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_futex_wait_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 456 long sys_futex_requeue ['u32 __user *uaddr1', 'u32 __user *uaddr2', 'u32 val', 'u32 val2', 'u32 mask']
		case 456: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_futex_requeue_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_futex_requeue_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 457 long sys_statmount ['const struct mnt_id_req __user *req', 'struct statmount __user *buf', 'size_t bufsize', 'unsigned int flags']
		case 457: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_statmount_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_statmount_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 458 long sys_listmount ['const struct mnt_id_req __user *req', 'u64 __user *mnt_ids', 'size_t nr_mnt_ids', 'unsigned int flags']
		case 458: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_listmount_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_listmount_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 459 long sys_lsm_get_self_attr ['unsigned int attr', 'struct lsm_ctx __user *ctx', 'u32 __user *size', 'u32 flags']
		case 459: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_lsm_get_self_attr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lsm_get_self_attr_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 460 long sys_lsm_set_self_attr ['unsigned int attr', 'struct lsm_ctx __user *ctx', 'u32 size', 'u32 flags']
		case 460: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_lsm_set_self_attr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lsm_set_self_attr_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 461 long sys_lsm_list_modules ['u64 __user *ids', 'u32 __user *size', 'u32 flags']
		case 461: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_lsm_list_modules_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_lsm_list_modules_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 462 long sys_mseal ['unsigned long start', 'size_t len', 'unsigned long flags']
		case 462: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			if (PPP_CHECK_CB(on_sys_mseal_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_mseal_return, cpu, pc, arg0, arg1, arg2) ;
		}; break;
		// 463 long sys_setxattrat ['int dfd', 'const char __user *path', 'const char __user *name', 'const void __user *value', 'size_t size', 'int flags']
		case 463: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			int32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_setxattrat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_setxattrat_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 464 long sys_getxattrat ['int dfd', 'const char __user *path', 'const char __user *name', 'void __user *value', 'size_t size', 'int flags']
		case 464: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			int32_t arg5 = 0;
			if (PPP_CHECK_CB(on_sys_getxattrat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
				memcpy(&arg5, ctx->args[5], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_getxattrat_return, cpu, pc, arg0, arg1, arg2, arg3, arg4, arg5) ;
		}; break;
		// 465 long sys_listxattrat ['int dfd', 'const char __user *path', 'char __user *list', 'size_t size', 'int flags']
		case 465: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			int32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_listxattrat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_listxattrat_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 466 long sys_removexattrat ['int dfd', 'const char __user *path', 'const char __user *name', 'int flags']
		case 466: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			int32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_removexattrat_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(int32_t));
			}
			PPP_RUN_CB(on_sys_removexattrat_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 467 long sys_open_tree_attr ['int dfd', 'const char __user *filename', 'unsigned flags', 'struct open_how __user *how', 'size_t usize']
		case 467: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_open_tree_attr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_open_tree_attr_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 468 long sys_file_getattr ['int dfd', 'const char __user *filename', 'struct statx __user *buffer', 'unsigned int flags', 'unsigned int mask']
		case 468: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			uint32_t arg4 = 0;
			if (PPP_CHECK_CB(on_sys_file_getattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
				memcpy(&arg4, ctx->args[4], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_file_getattr_return, cpu, pc, arg0, arg1, arg2, arg3, arg4) ;
		}; break;
		// 469 long sys_file_setattr ['int dfd', 'const char __user *filename', 'struct file_attr __user *attr', 'unsigned int flags']
		case 469: {
			int32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_file_setattr_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(int32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_file_setattr_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		// 470 long sys_listns ['unsigned int fd', 'unsigned int flags', 'u64 __user *ns_ids', 'size_t size']
		case 470: {
			uint32_t arg0 = 0;
			uint32_t arg1 = 0;
			uint32_t arg2 = 0;
			uint32_t arg3 = 0;
			if (PPP_CHECK_CB(on_sys_listns_return) || PPP_CHECK_CB(on_all_sys_return2)) {
				memcpy(&arg0, ctx->args[0], sizeof(uint32_t));
				memcpy(&arg1, ctx->args[1], sizeof(uint32_t));
				memcpy(&arg2, ctx->args[2], sizeof(uint32_t));
				memcpy(&arg3, ctx->args[3], sizeof(uint32_t));
			}
			PPP_RUN_CB(on_sys_listns_return, cpu, pc, arg0, arg1, arg2, arg3) ;
		}; break;
		default:
			PPP_RUN_CB(on_unknown_sys_return, cpu, pc, ctx->no);
	}
	PPP_RUN_CB(on_all_sys_return, cpu, pc, ctx->no);
	PPP_RUN_CB(on_all_sys_return2, cpu, pc, call, ctx);
#endif
}

/* vim: set tabstop=4 softtabstop=4 noexpandtab ft=cpp: */