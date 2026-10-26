"""Does everything pandare2 names exist in the engine and plugins it runs on? No guest, no image.

    panda-python tests/static/refs.py [arch ...]        (default: every arch pandare2 has bindings for)

pandare2 reaches the engine through cffi, so a name that doesn't exist fails only when the code
that uses it runs. This reads pandare2's source (the installed package, or the tree in PANDARE2_SRC:
PANDARE2_SRC=python/core/pandare2 checks a working tree without rebuilding pandare2) and, for
each arch, checks:

- lib:    every `libpanda.<name>` resolves in libpanda-<arch>-softmmu.so through Panda(arch).libpanda
          (declared in the cffi header and exported by the engine);
- plugin: every `plugins["<p>"].<name>` resolves in that plugin's .so, as load_plugin opens it;
- type:   every C type string given to ffi.new/cast/callback/typeof/sizeof exists in the arch's
          cffi module;
- field:  every CPUArchState field arch.py reads through `env.<a>.<b>` or `cpu_env(cpu).<a>`
          exists in the arch's CPUArchState.

References inside an arch.py class are checked only on the arches that class serves (X86_64Arch's
on x86_64 and i386, ...). A failure listed in refs-allow.txt (next to this file) is reported as
ALLOWED; an allowlisted name that resolves everywhere or that pandare2 no longer uses is STALE and
fails the run, so the list only shrinks. Exit status: 0 if every failure is allowed and nothing is
stale.

Each arch runs in its own process (one engine per process).
"""
import ast
import json
import os
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

HERE = Path(__file__).resolve().parent
ALLOW = HERE / "refs-allow.txt"
FFI_CALLS = {"new", "cast", "callback", "typeof", "sizeof"}


def pandare2_src():
    if os.environ.get("PANDARE2_SRC"):
        return Path(os.environ["PANDARE2_SRC"])
    import pandare2
    return Path(pandare2.__file__).parent


def arches():
    """The arches the installed pandare2 has cffi bindings for."""
    import pandare2
    return sorted(p.name[len("_pandare_ffi_"):-len("_softmmu.py")]
                  for p in (Path(pandare2.__file__).parent / "autogen").glob("_pandare_ffi_*_softmmu.py"))


def named(node, name):
    """node is `name` or `<anything>.name`."""
    return (isinstance(node, ast.Name) and node.id == name) or \
        (isinstance(node, ast.Attribute) and node.attr == name)


def env_chain(node):
    """For env.a.b (env a Name, or a cpu_env(...) call), the field path 'a.b'; else None."""
    parts = []
    while isinstance(node, ast.Attribute):
        parts.append(node.attr)
        node = node.value
    base = (isinstance(node, ast.Name) and node.id == "env") or \
        (isinstance(node, ast.Call) and named(node.func, "cpu_env"))
    return ".".join(reversed(parts)) if base and parts else None


def scan(src):
    """[(kind, name, file:line, class or None)] for every reference in pandare2's source."""
    refs = []
    for f in sorted(src.rglob("*.py")):
        rel = f.relative_to(src)
        if rel.parts[0] == "autogen":
            continue                          # the generated cffi modules
        text = f.read_text()
        tree = ast.parse(text)
        own_ffi = "FFI()" in text             # a module with its own cffi instance (extras/ioctlFaker.py)
        # arch.py's classes: a reference inside one is checked only on the arches it serves
        classes = [(n.lineno, n.end_lineno, n.name) for n in tree.body
                   if isinstance(n, ast.ClassDef)] if f.name == "arch.py" else []

        # module-level NAME = "string" constants, for plugins[NAME]
        consts = {t.id: n.value.value for n in tree.body if isinstance(n, ast.Assign)
                  and isinstance(n.value, ast.Constant) and isinstance(n.value.value, str)
                  for t in n.targets if isinstance(t, ast.Name)}

        def owner(line):
            return next((c for a, b, c in classes if a <= line <= b), None)

        def plugin_name(sl):
            if isinstance(sl, ast.Constant) and isinstance(sl.value, str):
                return sl.value
            if isinstance(sl, ast.Name):
                return consts.get(sl.id)          # else a variable: can't be checked statically
            return None

        for node in ast.walk(tree):
            where = f"{rel}:{getattr(node, 'lineno', 0)}"
            if isinstance(node, ast.Attribute):
                if named(node.value, "libpanda"):
                    refs.append(("lib", node.attr, where, owner(node.lineno)))
                elif (isinstance(node.value, ast.Subscript) and named(node.value.value, "plugins")
                      and plugin_name(node.value.slice)):
                    refs.append(("plugin", f"{plugin_name(node.value.slice)}.{node.attr}", where,
                                 owner(node.lineno)))
                elif classes:
                    path = env_chain(node)
                    if path and not isinstance(getattr(node, "_parent", None), ast.Attribute):
                        refs.append(("field", path, where, owner(node.lineno)))
            elif (isinstance(node, ast.Call) and not own_ffi and isinstance(node.func, ast.Attribute)
                  and node.func.attr in FFI_CALLS and named(node.func.value, "ffi") and node.args
                  and isinstance(node.args[0], ast.Constant) and isinstance(node.args[0].value, str)):
                refs.append(("type", node.args[0].value, where, owner(node.lineno)))
            for child in ast.iter_child_nodes(node):
                child._parent = node
    return refs


def check_arch(arch):
    """Run in a child process: resolve every reference that applies to this arch."""
    src = pandare2_src()
    from pandare2 import Panda
    panda = Panda(arch=arch, mem="16M")       # no panda_init: nothing runs
    ffi = panda.ffi
    mro = {c.__name__ for c in type(panda.arch).__mro__}
    handles = {}
    out = {}

    def resolve(kind, name):
        if kind == "lib":
            getattr(panda.libpanda, name)
        elif kind == "plugin":
            p, _, sym = name.partition(".")
            if p not in handles:
                so = Path(panda.get_plugin_path()) / f"libpanda-{p}_{arch}-softmmu.so"
                handles[p] = ffi.dlopen(str(so)) if so.exists() else None
            if handles[p] is None:
                raise LookupError(f"plugin {p} is not built")
            getattr(handles[p], sym)
        elif kind == "type":
            try:
                ffi.typeof(name)
            except Exception as e:
                if "function type" not in str(e):
                    raise
                ffi.callback(name)        # a function type: typeof refuses it, callback takes it
        elif kind == "field":
            t = ffi.typeof("CPUArchState")
            for part in name.split("."):
                while t.kind == "array":
                    t = t.item
                if t.kind not in ("struct", "union") or part not in dict(t.fields):
                    raise LookupError(f"no field {part} in {t.cname}")
                t = dict(t.fields)[part].type

    for kind, name, where, cls in scan(src):
        if cls is not None and cls not in mro:
            continue
        key = f"{kind} {name}"
        if key in out:
            continue
        try:
            resolve(kind, name)
            out[key] = None
        except Exception as e:  # noqa: BLE001 - every failure is a finding
            msg = str(e).splitlines()[0] if str(e) else ""
            out[key] = f"{where}: {type(e).__name__}: {msg[:90]}"
    return out


def main():
    if sys.argv[1:2] == ["--child"]:
        print("\n" + json.dumps(check_arch(sys.argv[2])), flush=True)   # the last line: Panda() prints too
        os._exit(0)                           # skip the engine's atexit work: it never started
    src = pandare2_src()
    todo = sys.argv[1:] or arches()
    allow = {l.strip() for l in ALLOW.read_text().splitlines() if l.strip() and not l.startswith("#")}
    used = {f"{k} {n}" for k, n, _, _ in scan(src)}
    failing = defaultdict(dict)               # key -> {arch: why}
    seen = set()
    for arch in todo:
        p = subprocess.run([sys.executable, __file__, "--child", arch], capture_output=True, text=True)
        if p.returncode != 0:
            print(p.stderr, file=sys.stderr)
            sys.exit(f"refs.py: the check for {arch} failed to run")
        res = json.loads(p.stdout.strip().splitlines()[-1])
        seen.update(res)
        for key, why in res.items():
            if why:
                failing[key][arch] = why
    bad = 0
    for key in sorted(failing):
        status = "ALLOWED" if key in allow else "FAIL"
        bad += status == "FAIL"
        archs = failing[key]
        first = next(iter(archs.values()))
        on = "all arches" if len(archs) == len(todo) else ",".join(sorted(archs))
        print(f"{status:7} {key:55} ({on}) {first}")
    stale = sorted(k for k in allow if k not in failing and (k in seen or k not in used))
    for key in stale:
        print(f"STALE   {key:55} " + ("resolves now" if key in used else "no longer used") +
              ": take it out of tests/static/refs-allow.txt")
    counts = defaultdict(int)
    for key in seen:
        counts[key.split()[0]] += 1
    print(f"refs.py: {len(todo)} arches; " + ", ".join(f"{n} {k}" for k, n in sorted(counts.items())) +
          f" names checked; {len(failing)} fail ({len(failing) - bad} allowed), {len(stale)} stale")
    sys.exit(1 if bad or stale else 0)


if __name__ == "__main__":
    main()
