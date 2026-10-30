#!/usr/bin/env python3
"""Actual option lookup/prepass and common seal policy, no engine or guest.
Run: python3 -I tests/static/startup_capabilities.py PATCHED_QEMU_TREE
"""
from pathlib import Path
import os
import re
import shlex
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[2]
TREE=Path(sys.argv[1]).resolve()
vl=(TREE/'system/vl.c').read_text()


def function(text,name):
    match=re.search(r'^\w[^\n]*\b'+re.escape(name)+r'\([^;]*?\)\s*\{',text,re.M)
    assert match,name
    return text[match.start():text.index('\n}',match.end())+2]


lookup=vl[vl.index('static const QEMUOption *lookup_opt('):vl.index('\n}',vl.index('static const QEMUOption *lookup_opt('))+2]
init=function(vl,'qemu_init')
assert init.index('seal_startup_capabilities(')<init.index('qemu_add_opts(')<init.index('module_call_init(')
board=function(vl,'qemu_init_board')
assert 'analysis_policy_set_analysis_active(true' in board
assert 'analysis_policy_set_analysis_active' not in function(vl,'seal_startup_capabilities')
assert 'parse_analysis_capabilities(optarg, &error_fatal)' not in init

with tempfile.TemporaryDirectory(prefix='startup-capabilities-') as temp:
    out=Path(temp)
    for name,text in {
        'qemu/osdep.h':'#include <stdbool.h>\n#include <stdint.h>\n',
        'qemu/typedefs.h':'typedef struct Error Error;\n',
        'qemu/atomic.h':'#define qatomic_set(p,v) (*(p)=(v))\n#define qatomic_read(p) (*(p))\n',
        'qapi/error.h':'#include "qemu/typedefs.h"\nvoid error_setg(Error **, const char *, ...);\n',
    }.items():
        p=out/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
    stubs=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <glib.h>
#include "system/analysis-policy.h"
struct Error { char text[160]; };
void error_setg(Error **e,const char *fmt,...) {
    if (!e) return;
    assert(!*e); *e=g_new0(Error,1);
    va_list ap; va_start(ap,fmt); vsnprintf((*e)->text,sizeof((*e)->text),fmt,ap);va_end(ap);
}
#define HAS_ARG 1
#define QEMU_OPTION_analysis_capabilities 7
#define loc_set_cmdline(...) ((void)0)
#define error_report(...) ((void)0)
typedef struct QEMUOption { const char *name; int flags,index; } QEMUOption;
static const QEMUOption qemu_options[]={
    {"analysis-capabilities",HAS_ARG,QEMU_OPTION_analysis_capabilities},
    {"append",HAS_ARG,1},{"device",HAS_ARG,2},{"module",HAS_ARG,3},
    {"S",0,4},{0}
};
'''
    tests=r'''
int main(int argc,char **argv) {
    assert(argc==2); Error *error=NULL;
    char *bare[]={"qemu"};
    char *args[]={"qemu","-append","--analysis-capabilities","--analysis-capabilities","fuzz","-device","-analysis-capabilities","-analysis-capabilities","rr","disk.img","-S"};
    char *vanilla[]={"qemu","-analysis-capabilities","vanilla"};
    char *bad[]={"qemu","-module","constructor.so","-analysis-capabilities","bad"};
    if (!strcmp(argv[1],"bare")) {
        assert(seal_startup_capabilities(1,bare,&error));
        assert(analysis_policy_is_frozen());
        assert(analysis_policy_requested()==ANALYSIS_CAP_CLI_DEFAULT);
        assert(analysis_policy_capabilities()==(ANALYSIS_CAP_ANALYSIS|
               ANALYSIS_CAP_GUEST_HYPERCALL|ANALYSIS_CAP_PENGUIN));
    } else if (!strcmp(argv[1],"vanilla")) {
        assert(seal_startup_capabilities(3,vanilla,&error));
        assert(analysis_policy_is_frozen() && !analysis_policy_capabilities());
    } else if (!strcmp(argv[1],"repeat")) {
        assert(seal_startup_capabilities(11,args,&error));
        assert(analysis_policy_requested()==ANALYSIS_CAP_RR);
        assert(analysis_policy_capabilities()==(ANALYSIS_CAP_RR|ANALYSIS_CAP_ANALYSIS));
    } else if (!strcmp(argv[1],"embedded")) {
        assert(analysis_policy_seal(ANALYSIS_CAP_ANALYSIS,&error));
        assert(seal_startup_capabilities(1,bare,&error));
        assert(analysis_policy_requested()==ANALYSIS_CAP_ANALYSIS);
        assert(!seal_startup_capabilities(3,vanilla,&error));
        assert(error && strstr(error->text,"sealed"));
        assert(analysis_policy_requested()==ANALYSIS_CAP_ANALYSIS);
    } else if (!strcmp(argv[1],"identical")) {
        assert(analysis_policy_seal(ANALYSIS_CAP_RR,&error));
        assert(seal_startup_capabilities(11,args,&error));
    } else if (!strcmp(argv[1],"unknown")) {
        assert(!seal_startup_capabilities(5,bad,&error));
        assert(error && !analysis_policy_is_frozen() && !analysis_policy_requested());
    } else if (!strcmp(argv[1],"effective-not-identical")) {
        assert(analysis_policy_seal(ANALYSIS_CAP_RR|ANALYSIS_CAP_ANALYSIS,&error));
        assert(!seal_startup_capabilities(11,args,&error));
        assert(error && analysis_policy_requested()==(ANALYSIS_CAP_RR|ANALYSIS_CAP_ANALYSIS));
    } else if (!strcmp(argv[1],"unknown-mask")) {
        assert(!analysis_policy_seal(1u<<31,&error));
        assert(error && !analysis_policy_is_frozen());
    }
    assert(!analysis_policy_analysis_active());
    g_clear_pointer(&error,g_free);
}
'''
    source=stubs+lookup+'\n'+function(vl,'parse_analysis_capabilities')+'\n'+function(vl,'seal_startup_capabilities')+tests
    (out/'test.c').write_text(source)
    flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','glib-2.0'],text=True))
    subprocess.run([os.environ.get('CC','cc'),'-std=gnu11','-O2','-I',str(out),'-I',str(ROOT/'engine/src/include'),str(ROOT/'engine/src/system/analysis-policy.c'),str(out/'test.c'),'-o',str(out/'test'),*flags],check=True)
    for scenario in ('bare','vanilla','repeat','embedded','identical','unknown','effective-not-identical','unknown-mask'):
        subprocess.run([str(out/'test'),scenario],check=True)
print('PASS: actual lookup/prepass skips option arguments, accepts single/double dash, resolves final repeat, seals the legacy default when bare and zero for explicit vanilla, retains embedded policy, rejects mismatch before side effects and never activates')
