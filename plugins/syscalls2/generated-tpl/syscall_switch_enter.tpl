#include "panda.h"

#include "syscalls2.h"
#include "syscalls2_info.h"
#include "hooks/hooks_int_fns.h"
//#include "hw_proc_id/hw_proc_id_ext.h"

extern bool load_info;
#if {{ arch_conf.get('runner_target', arch_conf.qemu_target) }}
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
void syscall_enter_switch_{{os}}_{{arch}}(CPUState *cpu, int profile, target_ptr_t pc, int static_callno) {
#if {{ arch_conf.get('runner_target', arch_conf.qemu_target) }}
	CPUArchState *env = (CPUArchState*)panda_cpu_env(cpu);
	syscall_ctx_t ctx = {0};
	ctx.profile = profile;
	if (static_callno == -1) {
	  ctx.no = {{arch_conf.rt_callno_reg}};
	} else {
	  ctx.no = static_callno;
	}
	ctx.asid = syscalls_get_id(cpu);
	ctx.retaddr = calc_retaddr(cpu, &ctx, pc);
	ctx.double_return = false;
	bool panda_noreturn;	// true if PANDA should not track the return of this system call
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
	{%- for syscall in syscalls %}
	// {{syscall.nos|join(', ')}} {{syscall.rettype}} {{syscall.name}} {{syscall.args_raw|join(' OR ')}}
	{% for no in syscall.nos -%} case {{no}}: {% endfor -%} {
		panda_noreturn = {{ 'true' if syscall.panda_noreturn else 'false' }};
		ctx.double_return = {{ 'true' if syscall.panda_double_return else 'false' }};
		{%- if syscall.args|length > 0 %}
		{{syscall.temp_assignments()}}
		if (PPP_CHECK_CB(on_all_sys_enter2) ||
			(!panda_noreturn && (PPP_CHECK_CB(on_all_sys_return2) ||
					PPP_CHECK_CB(on_{{syscall.name}}_return)))) {
			{%- for arg in syscall.args %}
			{{arg.emit_memcpy_temp_to_ref()}}
			{%- endfor %}
		}
		{%- endif %}
		PPP_RUN_CB(on_{{syscall.name}}_enter, {{syscall.cargs}});
	}; break;
	{%- endfor %}
	default:
		panda_noreturn = false;
		PPP_RUN_CB(on_unknown_sys_enter, cpu, pc, ctx.no);
	} // switch (ctx.no)

	PPP_RUN_CB(on_all_sys_enter, cpu, pc, ctx.no);
	PPP_RUN_CB(on_all_sys_enter2, cpu, pc, call, &ctx);
	if (!panda_noreturn) {
		struct hook h;
		h.addr = ctx.retaddr;
		h.asid = ctx.asid;
		h.cb.before_tcg_codegen = hook_syscall_return;
		h.type = PANDA_CB_BEFORE_TCG_CODEGEN;
		h.enabled = true;
		h.km = MODE_ANY; //you'd expect this to be user only
		hooks_add_hook(&h);

		running_syscalls[std::make_pair(ctx.retaddr, ctx.asid)] = ctx;
	}
#endif
}

/* vim: set tabstop=4 softtabstop=4 noexpandtab ft=cpp: */
