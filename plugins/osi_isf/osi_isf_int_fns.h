#pragma once

// BEGIN_PYPANDA_NEEDS_THIS -- do not delete this comment bc pypanda
// api autogen needs it.  And don't put any compiler directives
// between this and END_PYPANDA_NEEDS_THIS except includes of other
// files in this directory that contain subsections like this one.

// The capability report as JSON: each capability's status, strategy, inputs (with their
// provenance) and the strategies considered, and whether the ISF is paired with the running
// kernel. Valid until the next call.
const char *osi_isf_report(void);

// 2: available, 1: available from untrusted inputs (allow_untrusted), 0: not available (also
// before the ISF is paired with the kernel, or when it doesn't match, or for lifecycle,
// exit_codes, reparent and pid_reuse without lifecycle=true), -1: no such capability.
int osi_isf_capability(const char *name);

// Facts about process pid (its thread group) as JSON: which is a comma-separated list of threads,
// cmdline, environ, exe, fs, creds, caps, files, vmas, or "all". A fact that isn't available is
// {"unavailable": "<why>"}. With start_time not 0, the process must have started then (a pid
// reused since is another process). NULL before the ISF is paired with the kernel. Free the
// result with osi_isf_free.
char *osi_isf_facts(CPUState *cpu, int32_t pid, uint64_t start_time, const char *which);
void osi_isf_free(char *s);

// END_PYPANDA_NEEDS_THIS -- do not delete this comment!
