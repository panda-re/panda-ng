"""Test metadata: the literal PANDA_TEST dict a test file declares, read with ast (nothing is
imported), and its expansion into items, one per (test, arch, variant).

    PANDA_TEST = dict(
        area="syscalls",              # what the test covers: syscalls, rr, taint, ...
        tier="guest",                 # static | harness | guest | stress | bench
        arches=["x86_64", "aarch64"], # engine target names (ARCH_ALIASES: mkbusybox's names)
        fixture="testmachine",        # none | busybox | testmachine[-smpN][-intel] | cloud-<distro>
        plugins=["syscalls"],         # PANDA plugins it loads (checked in the plugin dir)
        variants=dict(smp=[1, 4], backend=dict(values=["trap", "arch"], default="arch")),
        xfail=[dict(when=dict(backend="arch"), reason="...", issue="#NN")],
        always=False,                 # selected on every change (used by selection)
        args=["{fixture}"],           # argv after the script (default: the fixture, if any)
        env={"SCTRACE_OBSERVER": "syscalls"},
        needs=["cc", "glib"],         # cc, c++, glib (pkg-config), pandare2, all-targets (all 13
                                      # in the engine build), py:<module>, or a program on PATH
        timeout=1800,                 # seconds
        nix_check="layout",           # the flake check that runs this file (static tier)
    )

`PANDA_TEST = None` marks a file that is not a test (a helper module or a runner). In a shell
script the same dict goes in a comment block: lines starting `# PANDA_TEST = dict(` and on.
"""
import ast
import hashlib
import itertools
import json
import os

TIERS = ("static", "harness", "guest", "stress", "bench")
FIELDS = {"area", "tier", "arches", "fixture", "plugins", "variants", "xfail", "always", "args",
          "env", "needs", "timeout", "nix_check"}

# Variant dimensions: how each value reaches the test (argv, environment, fixture name), the
# value `--variants default` runs, and, with "arches", the only arches the dimension applies to.
# A test's metadata may give a dimension as dict(values=[...], default=..., arches=[...]).
VARIANTS = {
    "smp": dict(args=lambda v: [] if v == 1 else ["--smp", str(v)],
                fixture=lambda v: "" if v == 1 else f"-smp{v}", default=1),
    "intel": dict(args=lambda v: ["--intel"] if v else [], fixture=lambda v: "-intel" if v else "",
                  arches=["x86_64"], default=False),
    # The syscalls plugin's callback form in tests/plugins/scobserve.
    "form": dict(env=lambda v: {"SCOBSERVE_LEAN": {"call": "0", "lean": "1", "entry": "entry"}[v]},
                 default="call"),
    # The syscalls plugin's event source (transitional, while both exist): its default is trap.
    "backend": dict(env=lambda v: {"PANDA_SYSCALLS_BACKEND": v}, default="trap"),
}

# The engine's targets (flake.nix defaultTargets)
TARGETS = ["x86_64", "i386", "arm", "aarch64", "mips", "mipsel", "mips64", "mips64el", "ppc", "ppc64",
           "loongarch64", "riscv32", "riscv64"]

# Arch names are the engine's targets; mkbusybox's names are aliases. ppc64le is the one name that
# is not a target: a little-endian guest on the ppc64 engine.
ARCH_ALIASES = {"armel": "arm", "mipseb": "mips", "mips64eb": "mips64", "powerpc": "ppc",
                "powerpc64": "ppc64", "powerpc64le": "ppc64le"}
BUSYBOX_ARCH = {v: k for k, v in ARCH_ALIASES.items()}      # arch -> mkbusybox name


def canonical(arch):
    return ARCH_ALIASES.get(arch, arch)


def panda_arch(arch):
    """The engine target (libpanda-<target>-softmmu.so) an arch runs on."""
    return {"ppc64le": "ppc64"}.get(arch, arch) if arch else None


class MetaError(Exception):
    pass


def _literal(node):
    """ast.literal_eval, plus dict(key=value, ...) calls."""
    if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "dict":
        if node.args or any(k.arg is None for k in node.keywords):
            raise MetaError("dict(...) takes keyword arguments only")
        return {k.arg: _literal(k.value) for k in node.keywords}
    if isinstance(node, ast.List):
        return [_literal(e) for e in node.elts]
    if isinstance(node, ast.Dict):
        return {_literal(k): _literal(v) for k, v in zip(node.keys, node.values)}
    return ast.literal_eval(node)


def read(path):
    """(found, metadata): (False, None) without PANDA_TEST, (True, None) for PANDA_TEST = None."""
    with open(path, encoding="utf-8") as f:
        src = f.read()
    if path.endswith(".sh"):
        lines, on = [], False
        for line in src.splitlines():
            on = on or line.startswith("# PANDA_TEST =")
            if on:
                if not line.startswith("#"):
                    break
                lines.append(line[2:])
        src = "\n".join(lines)
    if "PANDA_TEST" not in src:
        return False, None
    for node in ast.parse(src, path).body:
        if isinstance(node, ast.Assign) and [getattr(t, "id", None) for t in node.targets] == ["PANDA_TEST"]:
            meta = _literal(node.value)
            if meta is not None:
                validate(meta)
            return True, meta
    return False, None


def validate(meta):
    if not isinstance(meta, dict):
        raise MetaError("PANDA_TEST is not a dict")
    unknown = set(meta) - FIELDS
    if unknown:
        raise MetaError(f"unknown fields {sorted(unknown)}")
    for f in ("area", "tier"):
        if not isinstance(meta.get(f), str):
            raise MetaError(f"{f} is required (a string)")
    if meta["tier"] not in TIERS:
        raise MetaError(f"tier {meta['tier']!r}: one of {', '.join(TIERS)}")
    for name in meta.get("variants", {}):
        if name not in VARIANTS:
            raise MetaError(f"variant {name!r}: known are {', '.join(VARIANTS)} (pandatest/meta.py)")
    for x in meta.get("xfail", []):
        if not x.get("reason") or not isinstance(x.get("when", {}), dict):
            raise MetaError("an xfail entry needs a reason and a when dict")
    for a in meta.get("arches", []):
        if a in ARCH_ALIASES:
            raise MetaError(f"arch {a!r}: write {ARCH_ALIASES[a]!r} (the engine target's name)")


def _values(name, spec, arch, defaults):
    spec = spec if isinstance(spec, dict) else dict(values=spec)
    values, only = list(spec["values"]), spec.get("arches") or VARIANTS[name].get("arches")
    if only and arch not in only:
        return None                     # not applicable on this arch
    if defaults:
        d = spec.get("default", VARIANTS[name]["default"])
        return [d] if d in values else values[:1]
    return values


def expand(meta, defaults=False):
    """[(arch, {variant: value})] for the metadata, in a stable order; with defaults, each
    variant at its default value only."""
    out = []
    for arch in meta.get("arches") or [None]:
        dims = []
        for name, spec in sorted(meta.get("variants", {}).items()):
            vals = _values(name, spec, arch, defaults)
            if vals is not None:
                dims.append([(name, v) for v in vals])
        for combo in itertools.product(*dims):
            out.append((arch, dict(combo)))
    return out


def fmt(v):
    return {True: "1", False: "0"}.get(v, str(v)) if isinstance(v, bool) else str(v)


def item_name(stem, arch, params):
    parts = ([arch] if arch else []) + [f"{k}={fmt(v)}" for k, v in sorted(params.items())]
    return f"{stem}[{','.join(parts)}]" if parts else stem


def xfail_for(meta, arch, params):
    have = dict(params, arch=arch)
    for x in meta.get("xfail", []):
        when = x.get("when", {})
        if all(have.get(k) == v or (isinstance(v, list) and have.get(k) in v) for k, v in when.items()):
            return x
    return None


def variant_args(params):
    return [a for k, v in sorted(params.items()) if "args" in VARIANTS[k] for a in VARIANTS[k]["args"](v)]


def variant_env(params):
    env = {}
    for k, v in sorted(params.items()):
        if "env" in VARIANTS[k]:
            env.update(VARIANTS[k]["env"](v))
    return env


def fixture_name(meta, arch, params):
    """The registry name: testmachine-x86_64-smp4-intel, busybox-aarch64, cloud-debian-13-x86_64."""
    fx = meta.get("fixture", "none")
    if fx == "none":
        return "none"
    kind, _, opts = fx.partition("-")
    if kind == "testmachine":
        suffix = "".join(VARIANTS[k]["fixture"](params[k]) for k in VARIANTS     # smp, then intel
                         if k in params and "fixture" in VARIANTS[k])
        return f"testmachine-{arch}{'-' + opts if opts else ''}{suffix}"
    if kind == "cloud":
        return f"cloud-{opts}-{arch}"
    return f"{kind}-{arch}"


def item_hash(path, meta, arch, params):
    """An identity for the item: its file's bytes and its expansion."""
    h = hashlib.sha256(open(path, "rb").read())
    h.update(json.dumps([meta, arch, params], sort_keys=True, default=str).encode())
    return h.hexdigest()[:16]


def discover(tests_dir):
    """The candidate test files: tests/*.py, tests/*.sh, tests/static/*.py and *.sh."""
    out = []
    for sub in ("", "static"):
        d = os.path.join(tests_dir, sub)
        for f in sorted(os.listdir(d)):
            if f.endswith((".py", ".sh")) and f != "conftest.py":
                out.append(os.path.join(d, f))
    return out
