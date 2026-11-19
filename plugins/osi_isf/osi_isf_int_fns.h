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
// before the ISF is paired with the kernel, or when it doesn't match), -1: no such capability.
int osi_isf_capability(const char *name);

// END_PYPANDA_NEEDS_THIS -- do not delete this comment!
