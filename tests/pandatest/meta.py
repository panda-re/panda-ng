"""Test metadata: the literal PANDA_TEST dict a test file declares, read with ast (nothing is
imported), and its expansion into items, one per (test, arch, variant).

    PANDA_TEST = dict(
        area="syscalls",              # what the test covers: syscalls, rr, taint, ...
        tier="guest",                 # static | harness | guest | stress | bench
        arches=["x86_64", "aarch64"], # mkbusybox arch names for busybox/testmachine fixtures
        fixture="testmachine",        # none | busybox | testmachine[-smpN][-intel] | cloud-<distro>
        plugins=["syscalls"],         # PANDA plugins it loads (checked in the plugin dir)
        variants=dict(smp=[1, 4], intel=dict(values=[False, True], arches=["x86_64"])),
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

# Variant dimensions and how each value reaches the test: argv, environment, fixture name.
# A dimension with "arches" only takes values other than its first on those arches.
VARIANTS = {
    "smp": dict(args=lambda v: [] if v == 1 else ["--smp", str(v)],
                fixture=lambda v: "" if v == 1 else f"-smp{v}"),
    "intel": dict(args=lambda v: ["--intel"] if v else [], fixture=lambda v: "-intel" if v else "",
                  arches=["x86_64"]),
    # The syscalls plugin's callback form in tests/plugins/scobserve.
    "form": dict(env=lambda v: {"SCOBSERVE_LEAN": {"call": "0", "lean": "1", "entry": "entry"}[v]}),
    # The syscalls plugin's event source (transitional, while both exist).
    "backend": dict(env=lambda v: {"PANDA_SYSCALLS_BACKEND": v}),
}

# The engine's targets (flake.nix defaultTargets)
TARGETS = ["x86_64", "i386", "arm", "aarch64", "mips", "mipsel", "mips64", "mips64el", "ppc", "ppc64",
           "loongarch64", "riscv32", "riscv64"]

# mkbusybox arch -> pandare2 arch (the libpanda-<arch>-softmmu.so target)
PANDA_ARCH = {"armel": "arm", "mipseb": "mips", "mips64eb": "mips64", "powerpc": "ppc",
              "powerpc64": "ppc64", "powerpc64le": "ppc64"}


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


def _values(name, spec, arch):
    values, only = (spec["values"], spec.get("arches")) if isinstance(spec, dict) else (spec, None)
    only = only or VARIANTS[name].get("arches")
    if only and arch not in only:
        return None                     # not applicable on this arch
    return list(values)


def expand(meta):
    """[(arch, {variant: value})] for the metadata, in a stable order."""
    out = []
    for arch in meta.get("arches") or [None]:
        dims = []
        for name, spec in sorted(meta.get("variants", {}).items()):
            vals = _values(name, spec, arch)
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
