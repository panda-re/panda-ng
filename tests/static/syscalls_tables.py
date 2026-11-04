#!/usr/bin/env python3
"""The `syscalls` plugin's system call tables equal the prototype files, and syscalls2. No engine,
no guest.

    python3 tests/static/syscalls_tables.py
    (nix build .#checks.x86_64-linux.syscalls-tables-test)

The committed tables (plugins/syscalls/tables/generated) are regenerated into a temporary
directory and must be the same. Then they are compiled with the real lookup code
(tables/syscalls_tables.c, with ASan and UBSan) into a driver that answers queries through
syscalls_tables.h's functions, and:

- per profile, every number, name, return type, argument count and declared argument type and
  name equals the prototype file's, as this test parses it; no other number is there; a name
  finds its lowest number;
- every argument's kind, width, signedness and slots follow the ABI's rules, and a set of known
  calls have the expected ones;
- for every profile syscalls2 has, its compiled dso_info_<profile>.c (get_syscall_info) has the
  same calls with the same names, argument types, sizes, names and structure names and no-return
  flag, and every callback typedef in generated/syscalls_ext_typedefs_<arch>.h has the same
  parameter types and names (where syscalls2 gives one name two signatures, its typedef is one
  of them);
- mips64's three ABIs in syscalls2's one table equal the o32, n32 and n64 profiles;
- the ABI values are system/syscall-trap.h's, and profile selection picks the profiles the OS
  names say.

Needs a C compiler with ASan and UBSan, and jinja2 (syscalls2's generator, for the regeneration).
"""
import ctypes
import os
import re
import shlex
import subprocess
import sys
import tempfile
import unittest
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PLUGIN = ROOT / "plugins/syscalls"
TABLES = PLUGIN / "tables"
S2 = ROOT / "plugins/syscalls2"
TRAP_H = ROOT / "engine/src/include/system/syscall-trap.h"
CC = shlex.split(os.environ.get("CC", "cc"))
CXX = shlex.split(os.environ.get("CXX", "c++"))

ABIS = ["X86_64", "I386", "AARCH64", "ARM", "ARM_OABI", "MIPS", "PPC64", "PPC", "RISCV64", "RISCV32",
        "LOONGARCH64", "MIPS_N32", "MIPS_N64", "X32"]
S2IN = "plugins/syscalls2/generated-in"
EXIN = "plugins/syscalls/tables/generated-in"
# profile: (abi or None, source, number range, syscalls2 arch: its typedef header, or None)
LINUX = {
    "linux_x64": ("X86_64", f"{S2IN}/linux_x64_prototypes.txt", None, "x64"),
    "linux_x86": ("I386", f"{S2IN}/linux_x86_prototypes.txt", None, "x86"),
    "linux_arm64": ("AARCH64", f"{S2IN}/linux_arm64_prototypes.txt", None, "arm64"),
    "linux_arm": ("ARM", f"{S2IN}/linux_arm_prototypes.txt", None, "arm"),
    "linux_arm_oabi": ("ARM_OABI", f"{EXIN}/linux_arm_oabi_prototypes.txt", None, None),
    "linux_mips": ("MIPS", f"{S2IN}/linux_mips_prototypes.txt", None, "mips"),
    "linux_mips64": (None, f"{S2IN}/linux_mips64_prototypes.txt", None, "mips64"),
    "linux_mips64n64": ("MIPS_N64", f"{S2IN}/linux_mips64_prototypes.txt", (5000, 5999), None),
    "linux_mips64n32": ("MIPS_N32", f"{S2IN}/linux_mips64n32_prototypes.txt", None, "mips64n32"),
    "linux_ppc": ("PPC", f"{S2IN}/linux_ppc_prototypes.txt", None, "ppc"),
    "linux_ppc64": ("PPC64", f"{S2IN}/linux_ppc64_prototypes.txt", None, "ppc64"),
    "linux_riscv64": ("RISCV64", f"{S2IN}/linux_riscv64_prototypes.txt", None, "riscv64"),
    "linux_riscv32": ("RISCV32", f"{EXIN}/linux_riscv32_prototypes.txt", None, None),
    "linux_loongarch64": ("LOONGARCH64", f"{EXIN}/linux_loongarch64_prototypes.txt", None, None),
    "linux_x32": ("X32", f"{EXIN}/linux_x32_prototypes.txt", None, None),
}
S2_SERVED = {"linux_x64", "linux_x86", "linux_arm64", "linux_arm", "linux_mips", "linux_mips64",
             "linux_mips64n32", "linux_ppc", "linux_ppc64", "linux_riscv64", "freebsd_x64", "windows_2000_x86",
             "windows_xpsp2_x86", "windows_xpsp3_x86", "windows_7_x86", "windows_7_x64"}


def expected_profiles():
    out = {name: dict(abi=v[0], source=v[1], range=v[2], s2arch=v[3], os="linux", version=None)
           for name, v in LINUX.items()}
    out["freebsd_x64"] = dict(abi="X86_64", source=f"{S2IN}/freebsd_x64_prototypes.txt", range=None,
                              s2arch="x64", os="freebsd", version=None)
    for f in sorted((ROOT / S2IN).iterdir()):
        m = re.fullmatch(r"windows_(\w+)_(x86|x64)_prototypes\.txt", f.name)
        if m:
            name = f"windows_{m.group(1)}_{m.group(2)}"
            out[name] = dict(abi="I386" if m.group(2) == "x86" else "X86_64", source=f"{S2IN}/{f.name}",
                             range=None, s2arch=m.group(2) if name in S2_SERVED else None, os="windows",
                             version=m.group(1))
    return out

# Numbers syscalls2's prototype parser reads differently from the file: (profile, number, the
# name syscalls2 gives it). freebsd_x64's line "325 int munlockall(void); 326 int __getcwd(...);"
# is one call to syscalls2: 325 __getcwd.
S2_MISREAD = [("freebsd_x64", 325, "__getcwd")]

# Pointer size, C long size, 64-bit integer arguments: one slot, a pair, an even-aligned pair.
ABI_RULES = {
    "X86_64": (8, 8, None), "I386": (4, 4, "pair"), "AARCH64": (8, 8, None), "ARM": (4, 4, "even"),
    "ARM_OABI": (4, 4, "pair"), "MIPS": (4, 4, "even"), "PPC64": (8, 8, None), "PPC": (4, 4, "even"),
    "RISCV64": (8, 8, None), "RISCV32": (4, 4, "pair"), "LOONGARCH64": (8, 8, None),
    "MIPS_N32": (4, 4, None), "MIPS_N64": (8, 8, None), "X32": (4, 4, None),
}
KINDS = ["INT", "UINT", "LONG", "ULONG", "PTR", "STR", "WSTR", "BUF", "STRUCT_PTR", "STR_ARRAY"]
POINTER_KINDS = {"PTR", "STR", "WSTR", "BUF", "STRUCT_PTR", "STR_ARRAY"}
FLAGS = {"NORETURN": 1, "DOUBLE_RETURN": 2, "NO_COMPAT_INFO": 4, "NO_COMPAT_CALLBACK": 8}
# Types with no '*' that are pointers.
POINTER_TYPEDEFS = {"cap_user_header_t", "cap_user_data_t", "const cap_user_data_t", "__sighandler_t", "caddr_t"}

DRIVER = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syscalls_tables.h"

static void call(const SyscallInfo *c)
{
    if (!c) {
        puts("none");
        return;
    }
    printf("C\t%lld\t%s\t%s\t%d\t%u\t%u\t%u\n", (long long)c->num, c->name, c->rettype, c->abi,
           c->nargs, c->nslots, c->flags);
    for (unsigned i = 0; i < c->nargs; i++) {
        const SyscallArg *a = &c->args[i];
        printf("A\t%s\t%s\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\t%u\t%s\n", a->name, a->ctype,
               a->type_name ? a->type_name : "-", a->compat_type_name, a->kind, a->width,
               a->is_signed, a->slot, a->nslots, a->compat_type, a->compat_size,
               syscalls_compat_ctype(a));
    }
}

static const char *pname(const SyscallProfile *p)
{
    return p ? p->name : "none";
}

int main(void)
{
    char line[512], a[256], b[256];
    long long n, m;
    printf("abis %d\n.\n", SYSCALLS_ABI_COUNT);
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = 0;
        if (!strcmp(line, "list")) {
            size_t np;
            const SyscallProfile *const *ps = syscalls_profiles(&np);
            for (size_t i = 0; i < np; i++) {
                const SyscallProfile *p = ps[i];
                printf("P\t%s\t%d\t%d\t%s\t%s\t%u\t%u\t%u\t%u\t%u\t%u\n", p->name, p->os, p->abi,
                       p->version ? p->version : "-", p->source, p->flags, p->ncalls, p->max_args,
                       p->compat_max, p->compat_max_generic, p->compat_max_args);
            }
        } else if (sscanf(line, "dump %255s", a) == 1) {
            const SyscallProfile *p = syscalls_profile_by_name(a);
            for (uint32_t i = 0; p && i < p->ncalls; i++) {
                call(&p->calls[i]);
            }
        } else if (sscanf(line, "num %255s %lld", a, &n) == 2) {
            call(syscalls_profile_lookup(syscalls_profile_by_name(a), n));
        } else if (sscanf(line, "name %255s %255s", a, b) == 2) {
            call(syscalls_profile_lookup_name(syscalls_profile_by_name(a), b));
        } else if (sscanf(line, "abi %lld %255s", &n, a) == 2) {
            call(syscalls_lookup((int)n, strtoll(a, NULL, 0)));
        } else if (sscanf(line, "abiname %lld %255s", &n, a) == 2) {
            call(syscalls_by_name((int)n, a));
        } else if (sscanf(line, "profile %lld", &n) == 1) {
            puts(pname(syscalls_profile((int)n)));
        } else if (sscanf(line, "find %lld %255s %lld", &n, a, &m) == 3) {
            puts(pname(syscalls_profile_find((int)n, strcmp(a, "-") ? a : NULL, (int)m)));
        } else if (sscanf(line, "select %lld %255s", &n, a) == 2) {
            printf("%d\n", syscalls_select((int)n, strcmp(a, "-") ? a : NULL));
        } else if (sscanf(line, "selectos %255s", a) == 1) {
            printf("%d\n", syscalls_select_os_name(a));
        } else {
            printf("bad %s\n", line);
        }
        puts(".");
    }
    return 0;
}
'''


def run(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode:
        raise AssertionError(f"{' '.join(map(str, cmd))} failed:\n{r.stdout}\n{r.stderr}")
    return r.stdout


def ctokens(text):
    return re.findall(r"\w+|\*|\[\]", text)


def parse_prototypes(path, rng):
    """[(num, rettype, name, [raw args])]: one per declaration (a line can hold several)."""
    out = []
    for line in (ROOT / path).read_text().splitlines():
        if line.startswith("//"):
            continue
        for stmt in line.split(";"):
            m = re.fullmatch(r"\s*(\d+)\s+(.+?)\s*\b(\w+)\s*\((.*)\)\s*", stmt)
            if not m:
                continue
            num = int(m.group(1))
            if rng and not rng[0] <= num <= rng[1]:
                continue
            args = [a.strip() for a in m.group(4).split(",") if a.strip() not in ("", "void")]
            out.append((num, " ".join(m.group(2).split()), m.group(3), args))
    return out


class Driver:
    def __init__(self, exe):
        self.exe = exe

    def query(self, cmds):
        """One answer (a list of lines) per command."""
        r = subprocess.run([self.exe], input="".join(c + "\n" for c in cmds), capture_output=True, text=True,
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1", UBSAN_OPTIONS="halt_on_error=1"))
        if r.returncode:
            raise AssertionError(f"driver failed ({r.returncode}):\n{r.stderr[-4000:]}")
        blocks, cur = [], []
        for line in r.stdout.split("\n"):
            if line == ".":
                blocks.append(cur)
                cur = []
            elif line:
                cur.append(line)
        assert blocks[0] == [f"abis {len(ABIS)}"], blocks[0]
        assert len(blocks) == len(cmds) + 1, (len(blocks), len(cmds))
        return blocks[1:]


def calls_of(lines):
    """[call dict] of a driver answer."""
    out = []
    for line in lines:
        f = line.split("\t")
        if f[0] == "none":
            out.append(None)
        elif f[0] == "C":
            out.append(dict(num=int(f[1]), name=f[2], rettype=f[3], abi=int(f[4]), nargs=int(f[5]),
                            nslots=int(f[6]), flags=int(f[7]), args=[]))
        elif f[0] == "A":
            out[-1]["args"].append(dict(name=f[1], ctype=f[2], type_name=None if f[3] == "-" else f[3],
                                        compat_tname=f[4], kind=KINDS[int(f[5])], width=int(f[6]),
                                        signed=bool(int(f[7])), slot=int(f[8]), nslots=int(f[9]),
                                        compat_type=int(f[10]), compat_size=int(f[11]), compat_ctype=f[12]))
        else:
            raise AssertionError(line)
    return out


class SyscallInfoT(ctypes.Structure):
    """syscalls2_info.h's syscall_info_t"""
    _fields_ = [("no", ctypes.c_int), ("name", ctypes.c_char_p), ("nargs", ctypes.c_int),
                ("argt", ctypes.POINTER(ctypes.c_int)), ("argsz", ctypes.POINTER(ctypes.c_uint8)),
                ("argn", ctypes.POINTER(ctypes.c_char_p)), ("argtn", ctypes.POINTER(ctypes.c_char_p)),
                ("noreturn", ctypes.c_bool)]


class SyscallMetaT(ctypes.Structure):
    _fields_ = [("max", ctypes.c_uint32), ("max_generic", ctypes.c_uint32), ("max_args", ctypes.c_uint32)]


class Tables(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        tmp = Path(cls.tmp.name)
        (tmp / "driver.c").write_text(DRIVER)
        san = ["-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-g", "-O1"]
        srcs = [TABLES / "syscalls_tables.c"] + sorted((TABLES / "generated").glob("*.c"))
        run(CC + ["-std=c11", "-Wall", "-Wextra", "-Werror", *san, f"-I{PLUGIN}", "-o", tmp / "driver",
                  tmp / "driver.c", *srcs])
        cls.driver = Driver(str(tmp / "driver"))
        cls.expected = expected_profiles()
        listing = cls.driver.query(["list"])[0]
        cls.profiles = {}
        for line in listing:
            f = line.split("\t")
            cls.profiles[f[1]] = dict(os=int(f[2]), abi=int(f[3]), version=None if f[4] == "-" else f[4],
                                      source=f[5], flags=int(f[6]), ncalls=int(f[7]), max_args=int(f[8]),
                                      compat_max=int(f[9]), compat_max_generic=int(f[10]),
                                      compat_max_args=int(f[11]))
        names = list(cls.profiles)
        dumps = cls.driver.query([f"dump {n}" for n in names])
        cls.calls = {n: calls_of(d) for n, d in zip(names, dumps)}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_regenerated_tables_are_committed(self):
        out = Path(self.tmp.name) / "regen"
        run([sys.executable, TABLES / "gen_tables.py", "--outdir", out])
        committed = sorted(p.name for p in (TABLES / "generated").iterdir())
        self.assertEqual(sorted(p.name for p in out.iterdir()), committed)
        for name in committed:
            with self.subTest(file=name):
                self.assertTrue((out / name).read_text() == (TABLES / "generated" / name).read_text(),
                                f"plugins/syscalls/tables/generated/{name} is stale: run gen_tables.py")

    def test_header_compiles_as_cxx(self):
        src = Path(self.tmp.name) / "h.cpp"
        src.write_text('#include "syscalls_tables.h"\nint main() { return syscalls_lookup(0, 0) ? 0 : 1; }\n')
        run(CXX + ["-std=c++17", "-Wall", "-Wextra", "-Werror", "-fsyntax-only", f"-I{PLUGIN}", src])

    def test_abi_values_are_the_trap_s(self):
        trap = re.search(r"enum\s*\{([^}]*)\}", TRAP_H.read_text()).group(1)
        trap = [t.strip() for t in re.sub(r"/\*.*?\*/", "", trap, flags=re.S).split(",") if t.strip()]
        self.assertEqual(trap[-1], "SYSCALL_ABI_COUNT")
        ours = re.search(r"enum\s*\{([^}]*SYSCALLS_ABI_COUNT[^}]*)\}", (PLUGIN / "syscalls_tables.h").read_text())
        ours = [t.strip() for t in re.sub(r"/\*.*?\*/", "", ours.group(1), flags=re.S).split(",") if t.strip()]
        self.assertEqual(ours, [f"SYSCALLS_ABI_{a}" for a in ABIS] + ["SYSCALLS_ABI_COUNT"])
        self.assertEqual([t.replace("SYSCALL_ABI_", "") for t in trap[:-1]], ABIS[:len(trap) - 1])

    def test_profiles(self):
        self.assertEqual(sorted(self.profiles), sorted(self.expected))
        oses = {"linux": 0, "freebsd": 1, "windows": 2}
        for name, e in self.expected.items():
            with self.subTest(profile=name):
                p = self.profiles[name]
                self.assertEqual(p["os"], oses[e["os"]])
                self.assertEqual(p["abi"], ABIS.index(e["abi"]) if e["abi"] else -1)
                self.assertEqual(p["source"], e["source"])
                self.assertEqual(p["version"], e["version"])
                self.assertEqual(bool(p["flags"] & 1), name in S2_SERVED)
                self.assertEqual(bool(p["flags"] & 2), e["abi"] is None)

    def test_tables_equal_the_prototypes(self):
        for name, e in self.expected.items():
            protos = parse_prototypes(e["source"], e["range"])
            calls = self.calls[name]
            with self.subTest(profile=name):
                self.assertEqual(self.profiles[name]["ncalls"], len(protos))
                self.assertEqual([c["num"] for c in calls], sorted(n for n, *_ in protos))
                self.assertEqual(self.profiles[name]["max_args"], max(len(a) for *_, a in protos))
            # every number through the lookup, and numbers that aren't there
            nums = sorted(n for n, *_ in protos)
            absent = sorted({-1, nums[-1] + 1, 1 << 40} | {n + 1 for n in nums if n + 1 not in set(nums)})[:40]
            got = calls_of(sum(self.driver.query([f"num {name} {n}" for n in nums + absent]), []))
            self.assertEqual(got[len(nums):], [None] * len(absent), name)
            by_num = dict(zip(nums, got))
            for num, ret, cname, args in protos:
                c = by_num[num]
                with self.subTest(profile=name, num=num):
                    self.assertEqual((c["num"], c["name"], c["rettype"], c["nargs"]), (num, cname, ret, len(args)))
                    for i, (raw, a) in enumerate(zip(args, c["args"])):
                        self.check_declared(raw, a, i)
            # by name: the lowest number of that name
            lowest = {}
            for num, _, cname, _ in sorted(protos):
                lowest.setdefault(cname, num)
            names = sorted(lowest)
            got = calls_of(sum(self.driver.query([f"name {name} {n}" for n in names] + [f"name {name} no_such"]),
                               []))
            self.assertIsNone(got[-1])
            self.assertEqual([c["num"] for c in got[:-1]], [lowest[n] for n in names], name)

    def check_declared(self, raw, a, i):
        """The table's type and name are the declaration's: its tokens without __user are the
        type's, then the name unless the declaration has none (syscalls2's arg<i>)."""
        toks = [t for t in ctokens(raw) if t != "__user"]
        ct = ctokens(a["ctype"])
        if toks == ct:
            self.assertEqual(a["name"], f"arg{i}", raw)
            return
        # else toks is ct with the name inserted before any trailing []
        if toks[-1] == "[]":
            self.assertEqual(toks[:-2] + toks[-1:], ct, raw)
            name = toks[-2]
        else:
            self.assertEqual(toks[:-1], ct, raw)
            name = toks[-1]
        self.assertRegex(name, r"^\w+$", raw)
        sanitized = name if name not in ("new", "data", "int", "cpu") else "_" + name
        self.assertEqual(a["name"], sanitized, raw)

    def test_argument_rules(self):
        for name, calls in self.calls.items():
            for c in calls:
                abi = ABIS[c["abi"]]
                ptr, long_, pairs = ABI_RULES[abi]
                if self.expected[name]["abi"]:
                    self.assertEqual(abi, self.expected[name]["abi"])
                else:   # linux_mips64: by number
                    self.assertEqual(abi, {4: "MIPS", 5: "MIPS_N64", 6: "MIPS_N32"}[c["num"] // 1000])
                slot = 0
                for a in c["args"]:
                    where = f"{name} {c['num']} {c['name']} {a['name']}: {a}"
                    stars = a["ctype"].count("*") + a["ctype"].count("[]")
                    if a["kind"] in POINTER_KINDS:
                        self.assertEqual((a["width"], a["signed"], a["nslots"]), (ptr, False, 1), where)
                        if not stars:
                            self.assertTrue(a["ctype"] in POINTER_TYPEDEFS or re.fullmatch(r"L?P[A-Z0-9_]+",
                                                                                          a["ctype"]), where)
                    else:
                        self.assertEqual(stars, 0, where)
                        self.assertEqual(a["signed"], a["kind"] in ("INT", "LONG"), where)
                        if a["kind"] in ("LONG", "ULONG"):
                            self.assertIn(a["width"], (long_, ptr), where)
                        else:
                            self.assertIn(a["width"], (1, 2, 4, 8), where)
                    if a["kind"] == "STR":
                        self.assertTrue("char" in a["ctype"] and stars == 1, where)
                    if a["kind"] == "STR_ARRAY":
                        self.assertTrue("char" in a["ctype"] and stars == 2, where)
                    if a["kind"] == "WSTR":
                        self.assertEqual(a["ctype"], "PWSTR", where)
                    if a["kind"] == "STRUCT_PTR":
                        self.assertTrue(a["type_name"], where)
                    else:
                        self.assertIsNone(a["type_name"], where)
                    two = pairs is not None and a["width"] == 8 and a["kind"] not in POINTER_KINDS
                    if two and pairs == "even" and slot % 2:
                        slot += 1
                    self.assertEqual((a["slot"], a["nslots"]), (slot, 2 if two else 1), where)
                    slot += a["nslots"]
                self.assertEqual(c["nslots"], slot, f"{name} {c['name']}")

    def arg(self, profile, num, i):
        return self.by_num(profile, num)["args"][i]

    def by_num(self, profile, num):
        return next(c for c in self.calls[profile] if c["num"] == num)

    def test_known_calls(self):
        def kinds(profile, num):
            c = self.by_num(profile, num)
            return [(a["name"], a["kind"], a["width"], a["slot"], a["nslots"]) for a in c["args"]]

        self.assertEqual(kinds("linux_x64", 0), [("fd", "UINT", 4, 0, 1), ("buf", "BUF", 8, 1, 1),
                                                 ("count", "ULONG", 8, 2, 1)])
        self.assertEqual(self.by_num("linux_x64", 0)["name"], "sys_read")
        self.assertEqual([a["kind"] for a in self.by_num("linux_x64", 59)["args"]], ["STR", "STR_ARRAY", "STR_ARRAY"])
        self.assertEqual(self.arg("linux_x64", 257, 1)["kind"], "STR")         # openat filename
        self.assertEqual(self.arg("linux_x64", 257, 3)["width"], 2)            # umode_t
        self.assertEqual(self.arg("linux_x64", 4, 1)["type_name"], "stat")     # newstat statbuf
        # pread64's loff_t: i386 and OABI in the next two slots, EABI/o32 in an even pair, n32/x32 one
        # (x86's entry point is sys_ia32_pread64, with the halves as two u32)
        self.assertEqual(kinds("linux_x86", 180)[3:], [("poslo", "UINT", 4, 3, 1), ("poshi", "UINT", 4, 4, 1)])
        self.assertEqual(kinds("linux_x86", 239)[2], ("offset", "PTR", 4, 2, 1))   # sendfile64's loff_t *
        self.assertEqual(kinds("linux_arm", 180)[3], ("pos", "INT", 8, 4, 2))
        self.assertEqual(kinds("linux_arm_oabi", 180)[3], ("pos", "INT", 8, 3, 2))
        self.assertEqual(kinds("linux_mips", 4200)[3], ("pos", "INT", 8, 4, 2))
        self.assertEqual(kinds("linux_riscv32", 67)[3], ("pos", "INT", 8, 3, 2))
        self.assertEqual(kinds("linux_mips64n32", 6016)[3], ("pos", "INT", 8, 3, 1))
        self.assertEqual(self.by_num("linux_x32", 0x40000000)["name"], "sys_read")
        self.assertEqual(kinds("linux_x32", 0x40000000)[1], ("buf", "BUF", 4, 1, 1))
        self.assertEqual(self.by_num("linux_arm", 0x0f0005)["name"], "ARM_set_tls")
        self.assertEqual(self.by_num("linux_mips64n64", 5000)["name"], "sys_read")
        # Windows: handles are pointer-sized, ULONG is 32 bits on both, P<X> points to X
        rf = next(c for c in self.calls["windows_7_x64"] if c["name"] == "NtReadFile")
        self.assertEqual([(a["kind"], a["width"], a["type_name"]) for a in rf["args"][:7]],
                         [("ULONG", 8, None), ("ULONG", 8, None), ("PTR", 8, None), ("BUF", 8, None),
                          ("STRUCT_PTR", 8, "IO_STATUS_BLOCK"), ("BUF", 8, None), ("UINT", 4, None)])
        rf32 = next(c for c in self.calls["windows_7_x86"] if c["name"] == "NtReadFile")
        self.assertEqual([a["width"] for a in rf32["args"]], [4] * 9)
        # FreeBSD calls returning a pointer, which syscalls2's parser skips
        mmap = self.by_num("freebsd_x64", 477)
        self.assertEqual((mmap["name"], mmap["rettype"], mmap["nargs"]), ("mmap", "void *", 6))
        self.assertTrue(mmap["flags"] & FLAGS["NO_COMPAT_INFO"])
        # two declarations on one line (freebsd_x64 325 and 326)
        self.assertEqual((self.by_num("freebsd_x64", 325)["name"], self.by_num("freebsd_x64", 326)["name"]),
                         ("munlockall", "__getcwd"))

    def test_flags_are_context_target_s(self):
        self.assertTrue(self.by_num("linux_x64", 231)["flags"] & FLAGS["NORETURN"])      # exit_group
        self.assertTrue(self.by_num("linux_x64", 57)["flags"] & FLAGS["DOUBLE_RETURN"])  # fork
        self.assertTrue(self.by_num("linux_riscv32", 93)["flags"] & FLAGS["NORETURN"])   # exit
        self.assertFalse(self.by_num("linux_x64", 0)["flags"] & (FLAGS["NORETURN"] | FLAGS["DOUBLE_RETURN"]))

    def test_mips64_abis_equal_their_profiles(self):
        def sig(c):
            return (c["num"], c["name"], c["rettype"], [(a["name"], a["ctype"], a["kind"], a["width"], a["slot"],
                                                         a["nslots"]) for a in c["args"]])
        both = defaultdict(list)
        for c in self.calls["linux_mips64"]:
            both[c["num"] // 1000].append(sig(c))
        self.assertEqual(both[4], [sig(c) for c in self.calls["linux_mips"]])
        self.assertEqual(both[5], [sig(c) for c in self.calls["linux_mips64n64"]])
        self.assertEqual(both[6], [sig(c) for c in self.calls["linux_mips64n32"]])

    def test_syscalls2_info(self):
        """get_syscall_info()'s tables, compiled from syscalls2's generated dso_info_*.c."""
        tmp = Path(self.tmp.name)
        for name in sorted(S2_SERVED):
            src = S2 / "generated" / f"dso_info_{name}.c"
            lib = tmp / f"dso_{name}.so"
            run(CC + ["-shared", "-fPIC", "-w", f"-I{S2}", "-o", lib, src])
            dso = ctypes.CDLL(str(lib))
            meta = SyscallMetaT.in_dll(dso, "__syscall_meta")
            p = self.profiles[name]
            with self.subTest(profile=name):
                self.assertEqual((p["compat_max"], p["compat_max_generic"], p["compat_max_args"]),
                                 (meta.max, meta.max_generic, meta.max_args))
            # __syscall_info_a[] has the generic calls, at their numbers: max_generic + 1 entries
            table = (SyscallInfoT * (meta.max_generic + 1)).in_dll(dso, "__syscall_info_a")
            info = {}
            for i, e in enumerate(table):
                if e.name:
                    self.assertEqual(e.no, i)
                    info[e.no] = e
            ours = {c["num"]: c for c in self.calls[name]}
            expect = {num for num, c in ours.items() if not c["flags"] & FLAGS["NO_COMPAT_INFO"]}
            misread = {num: s2name for prof, num, s2name in S2_MISREAD if prof == name}
            with self.subTest(profile=name):
                self.assertEqual(sorted(info), sorted(expect | set(misread)))
            for num, s2name in misread.items():
                self.assertEqual(info.pop(num).name.decode(), s2name)
            for num, e in info.items():
                c = ours[num]
                with self.subTest(profile=name, num=num):
                    self.assertEqual((c["name"], c["nargs"]), (e.name.decode(), e.nargs))
                    self.assertEqual(bool(c["flags"] & FLAGS["NORETURN"]), e.noreturn)
                    got = [(e.argt[i], e.argsz[i], e.argn[i].decode(), e.argtn[i].decode()) for i in range(e.nargs)]
                    self.assertEqual(got, [(a["compat_type"], a["compat_size"], a["name"], a["compat_tname"])
                                           for a in c["args"]])

    def test_syscalls2_callback_signatures(self):
        """Every call with a syscalls2 callback has its typedef's parameter types and names."""
        typedef = re.compile(r"PPP_CB_TYPEDEF\(void, on_(\w+)_enter, CPUState\* cpu, target_ulong pc(.*)\);")
        by_arch = defaultdict(list)
        for name, e in self.expected.items():
            if e["s2arch"]:
                by_arch[e["s2arch"]].append(name)
        self.collisions = {}
        for arch, profiles in sorted(by_arch.items()):
            text = (S2 / "generated" / f"syscalls_ext_typedefs_{arch}.h").read_text()
            cbs = {}
            for m in typedef.finditer(text):
                params = [p.strip().rsplit(" ", 1) for p in m.group(2).split(",")[1:]]
                cbs[m.group(1)] = [tuple(p) for p in params]
            variants = defaultdict(set)
            for name in profiles:
                for c in self.calls[name]:
                    if not c["flags"] & FLAGS["NO_COMPAT_CALLBACK"]:
                        variants[c["name"]].add(tuple((a["compat_ctype"], a["name"]) for a in c["args"]))
            with self.subTest(arch=arch):
                self.assertEqual(sorted(variants), sorted(cbs))
            for cname, sigs in variants.items():
                with self.subTest(arch=arch, call=cname):
                    self.assertIn(tuple(cbs[cname]), sigs)
                    if len(sigs) == 1:
                        self.assertEqual([tuple(cbs[cname])], list(sigs))

    def test_selection(self):
        default = {a: n for n, e in self.expected.items() if e["os"] == "linux" and (a := e["abi"])}
        q = [f"profile {i}" for i in range(len(ABIS))]
        q += ["abi 0 0", f"abi {ABIS.index('MIPS_N64')} 5000", f"abi {ABIS.index('X32')} 0x40000000",
              f"abi {ABIS.index('ARM_OABI')} 0x0f0005", f"abiname {ABIS.index('MIPS')} sys_read",
              f"abi {len(ABIS)} 0", "abi -1 0"]
        q += ["select 2 7sp1"] + [f"profile {i}" for i in range(len(ABIS))]
        q += ["selectos windows-32-xpsp3", "profile 1", "profile 0"]
        q += ["selectos bogus-64-x", "profile 1"]
        q += ["select 2 vistasp2", "profile 0", "selectos freebsd-64-13.2", "profile 0", "profile 1"]
        q += ["find 2 2003sp1 1", "find 2 - 1", "find 1 - 0"]
        q += ["selectos linux-64-ubuntu:4.15.0-72-generic-noaslr-nokaslr", "profile 0", "profile 13"]
        r = iter(self.driver.query(q))
        self.assertEqual([next(r)[0] for _ in ABIS], [default[a] for a in ABIS])
        self.assertEqual([c and c["name"] for c in calls_of(sum([next(r) for _ in range(7)], []))],
                         ["sys_read", "sys_read", "sys_read", "ARM_set_tls", "sys_read", None, None])
        self.assertEqual(next(r), ["2"])
        self.assertEqual([next(r)[0] for _ in ABIS],
                         ["windows_7_x64", "windows_7_x86"] + ["none"] * (len(ABIS) - 2))
        self.assertEqual([next(r), next(r), next(r)], [["1"], ["windows_xpsp3_x86"], ["none"]])
        self.assertEqual([next(r), next(r)], [["0"], ["windows_xpsp3_x86"]])
        self.assertEqual([next(r), next(r)], [["2"], ["windows_vistasp12_x64"]])
        self.assertEqual([next(r), next(r), next(r)], [["1"], ["freebsd_x64"], ["none"]])
        self.assertEqual([next(r), next(r), next(r)], [["windows_2003sp12_x86"], ["none"], ["freebsd_x64"]])
        self.assertEqual([next(r), next(r), next(r)], [[str(len(ABIS))], ["linux_x64"], ["linux_x32"]])


def coverage():
    """ABI, profile, source and call count, for the record."""
    t = Tables
    t.setUpClass()
    try:
        for name, e in t.expected.items():
            print(f"{e['abi'] or 'o32/n64/n32':12} {name:22} {t.profiles[name]['ncalls']:5}  {e['source']}")
    finally:
        t.tearDownClass()


if __name__ == "__main__":
    if sys.argv[1:] == ["--coverage"]:
        coverage()
    else:
        unittest.main()
