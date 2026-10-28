#!/usr/bin/env python3
"""Historical ABI coverage and exclusions, against the kernel source used by regen.sh."""
import copy
import sys
import unittest

import linux_prototypes as gen


class LinuxABIs(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = gen.Source(cls.kernel_source)
        cls.tables = {abi: dict(gen.abi_table(cls.src, abi, conf))
                      for abi, conf in gen.ABIS.items()}

    def test_retired_calls_keep_their_arguments(self):
        # Independent ABI numbers, including all three MIPS number spaces. These calls must
        # remain recognizable even when the generating kernel only implements ENOSYS there.
        expected = {
            "x64": {156: ("sys_sysctl", 1), 212: ("sys_lookup_dcookie", 3)},
            "x86": {134: ("sys_bdflush", 2), 149: ("sys_sysctl", 1),
                    253: ("sys_lookup_dcookie", 3)},
            "arm": {134: ("sys_bdflush", 2), 149: ("sys_sysctl", 1),
                    249: ("sys_lookup_dcookie", 3)},
            "arm64": {18: ("sys_lookup_dcookie", 3), 42: ("sys_nfsservctl", 3)},
            "riscv64": {18: ("sys_lookup_dcookie", 3), 42: ("sys_nfsservctl", 3)},
            "mips": {4134: ("sys_bdflush", 2), 4153: ("sys_sysctl", 1),
                     4189: ("sys_nfsservctl", 3), 4247: ("sys_lookup_dcookie", 3),
                     4127: ("sys_create_module", 2), 4130: ("sys_get_kernel_syms", 1),
                     4187: ("sys_query_module", 5)},
            "mips64": {4153: ("sys_sysctl", 1), 5152: ("sys_sysctl", 1),
                       6152: ("sys_sysctl", 1), 4247: ("sys_lookup_dcookie", 3),
                       5206: ("sys_lookup_dcookie", 3), 6206: ("sys_lookup_dcookie", 3)},
            "mips64n32": {6152: ("sys_sysctl", 1), 6206: ("sys_lookup_dcookie", 3)},
            "ppc": {134: ("sys_bdflush", 2), 149: ("sys_sysctl", 1),
                    168: ("sys_nfsservctl", 3), 235: ("sys_lookup_dcookie", 3)},
            "ppc64": {149: ("sys_sysctl", 1), 235: ("sys_lookup_dcookie", 3)},
        }
        for abi, calls in expected.items():
            for nr, (name, argc) in calls.items():
                with self.subTest(abi=abi, nr=nr):
                    proto = self.tables[abi][nr]
                    self.assertIn(f" {name}(", proto)
                    self.assertEqual(gen.arg_count(proto), argc)
        self.assertIn("u64 cookie64", self.tables["mips"][4247])

    def test_abi_membership_is_not_inferred_from_a_prototype(self):
        # ARM OABI-only calls are not ARM EABI, and riscv64 never selected renameat.
        for nr in (13, 25, 27, 30, 102, 117):
            self.assertNotIn(nr, self.tables["arm"])
        self.assertNotIn(38, self.tables["riscv64"])
        # HRW reports these numbered PPC64 slots, but the ABI never implemented these calls.
        for nr in (18, 22, 28, 59, 67, 72, 73, 76, 82, 84, 89, 109, 119, 126, 256):
            self.assertNotIn(nr, self.tables["ppc64"])
        self.assertIn(363, self.tables["ppc64"])  # switch_endian is 64-bit only
        self.assertNotIn(363, self.tables["ppc"])
        # MIPS timerfd is a permanently unimplemented slot, not timerfd_create's signature.
        for abi, nr in (("mips", 4318), ("mips64", 5277), ("mips64n32", 6281)):
            self.assertNotIn(nr, self.tables[abi])

    def test_unrecognized_implemented_call_fails(self):
        src = copy.copy(self.src)
        src.files = self.src.files | {
            gen.X86_64: self.src.files[gen.X86_64] +
                        "\n999 common panda_future sys_panda_future\n"}
        with self.assertRaisesRegex(SystemExit, "999 panda_future"):
            gen.abi_table(src, "x64", gen.ABIS["x64"])

    def test_shared_callbacks_have_compatible_argument_counts(self):
        gen.check_shared({abi: list(table.items()) for abi, table in self.tables.items()})


if __name__ == "__main__":
    # unittest parses argv before setUpClass, so keep the kernel path outside its arguments.
    LinuxABIs.kernel_source = sys.argv.pop(1)
    unittest.main()
