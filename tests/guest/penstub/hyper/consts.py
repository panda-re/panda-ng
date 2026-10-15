"""igloo_driver's constants, as penguin's hyper/consts.py loads them from
the driver's ISF: HYPER_OP (HYPER_OP_NONE, then src/portal/portal_op_list.h
in order; the responses from 0xf0000000), value_filter_type,
igloo_hypercall_constants; igloo_base_hypercalls."""
import os
import re

H = os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', 'igloo_driver',
                 'src', 'portal')


class _Consts:
    pass


HYPER_OP = _Consts()
HYPER_OP.HYPER_OP_NONE = 0
ops = re.findall(r'X\(\w+, (\w+)\)', open(os.path.join(H, 'portal_op_list.h')).read())
for i, name in enumerate(ops, 1):
    setattr(HYPER_OP, f'HYPER_OP_{name}', i)
HYPER_OP.HYPER_OP_MAX = len(ops) + 1
for i, name in enumerate(['NONE', 'READ_OK', 'READ_FAIL', 'READ_PARTIAL', 'WRITE_OK',
                          'WRITE_FAIL', 'READ_NUM', 'MAX']):
    setattr(HYPER_OP, f'HYPER_RESP_{name}', 0xf0000000 + i)
igloo_hypercall_constants = _Consts()

for k, v in re.findall(r'^\s*(\w+)\s*=\s*(0x[0-9a-fA-F]+|\d+)', open(os.path.join(
        H, '..', 'igloo_hypercall_consts.h')).read(), re.M):
    setattr(igloo_hypercall_constants, k, int(v, 0))
value_filter_type = _Consts()
src = open(os.path.join(H, '..', 'hooks', 'syscalls_hc.h')).read()
for i, k in enumerate(re.findall(r'^\s*(SYSCALLS_HC_FILTER_\w+)', src, re.M)):
    setattr(value_filter_type, k, i)
igloo_base_hypercalls = _Consts()
igloo_base_hypercalls.IGLOO_HYP_SETUP_SYSCALL = 0x1337
