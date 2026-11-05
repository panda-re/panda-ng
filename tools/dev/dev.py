"""panda-ng's local development build. Run it through tools/dev/dev.sh, which enters the flake's
dev shell first; docs/guides/development.md describes the commands.

Layout of the build directory (.dev/ by default):

  qemu/                the engine's tree: engine/scripts/import-series.sh's git tree (the tarball,
                       one commit per patch, the engine/src overlay), with QEMU's build in build/
  headers/<target>/    libpanda-ng's working directories
  h2b/                 the taint H2b shadow helpers (engine/h2b)
  prefix/              what the plugins and pandare2 use, laid out like the flake's engine:
                       lib/ (links into qemu/build), include/panda-ng, include/panda,
                       share/qemu (the firmware)
  plugins/             the plugins' meson build; plugins/plugins is PANDA_PLUGIN_DIR
  bin/python3          Python with pandare2 from python/core on this build (PANDA_PYTHON)
  state.json           what each step last built from

Each step does its work only when its inputs changed: the patch series (a changed series is
applied to a fresh checkout of the base and the tree moved to it with `git reset --hard`, so only
files whose content changed get new mtimes), the overlay (files copied when their content
differs), the engine's headers (libpanda-ng runs when the content of any header in the tree or the
build changed; its output is copied into prefix/include only where it differs, so the plugins
rebuild only on a real change), and pandare2's bindings (made again when panda_python_<arch>.h or a
plugin header they read changed).
"""

import argparse
import concurrent.futures
import fcntl
import hashlib
import json
import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(os.environ.get("PANDA_DEV_ROOT") or Path(__file__).resolve().parents[2])
DEV = Path(os.environ.get("PANDA_DEV_DIR") or ROOT / ".dev")
ENGINE = ROOT / "engine"
TREE = DEV / "qemu"
BUILD = TREE / "build"
PREFIX = DEV / "prefix"
PLUGINS_BUILD = DEV / "plugins"
STATE = DEV / "state.json"
STEPS = ("engine", "headers", "h2b", "plugins", "bindings")
ARCHES = ("x86_64", "i386", "arm", "aarch64", "mips", "mipsel", "mips64", "mips64el", "ppc", "ppc64",
          "loongarch64", "riscv32", "riscv64")
# The flake's configure line (mkEngine in flake.nix).
CONFIGURE = ["--enable-plugins", "--enable-panda", "--disable-docs", "--disable-werror",
             "--disable-gtk", "--disable-sdl", "--disable-opengl", "--disable-virglrenderer",
             "--disable-spice", "--disable-vnc", "--disable-containers"]
# The engine's additive API headers the flake installs into include/panda (mkEngine).
PANDA_HEADERS = ("analysis-effect.h", "guest-write.h", "events.h")
# QEMU's firmware bundle in the build tree (configure's default prefix, /usr/local).
FIRMWARE = BUILD / "qemu-bundle/usr/local/share/qemu"


def log(msg):
    print(f"dev: {msg}", flush=True)


def die(msg):
    print(f"dev: error: {msg}", file=sys.stderr, flush=True)
    sys.exit(1)


def run(cmd, cwd=None, env=None, quiet=False):
    if not quiet:
        log("$ " + " ".join(shlex.quote(str(c)) for c in cmd))
    r = subprocess.run([str(c) for c in cmd], cwd=cwd, env=env,
                       stdout=subprocess.DEVNULL if quiet else None)
    if r.returncode:
        die(f"{cmd[0]} failed ({r.returncode})")


def git(*args, capture=False):
    cmd = ["git", "-C", TREE, "-c", "user.email=dev@local", "-c", "user.name=dev", *args]
    if capture:
        return subprocess.run([str(c) for c in cmd], check=True, capture_output=True,
                              text=True).stdout.strip()
    run(cmd, quiet=True)


def sha(*parts):
    h = hashlib.sha256()
    for p in parts:
        h.update(p if isinstance(p, bytes) else str(p).encode())
        h.update(b"\0")
    return h.hexdigest()


def file_sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def copy_if_changed(src, dst):
    """Copy src to dst unless dst already has its content; True if it copied."""
    src, dst = Path(src), Path(dst)
    data = src.read_bytes()
    if dst.is_file() and not dst.is_symlink() and dst.read_bytes() == data:
        return False
    dst.parent.mkdir(parents=True, exist_ok=True)
    tmp = dst.with_name(dst.name + ".dev-tmp")
    tmp.write_bytes(data)
    shutil.copymode(src, tmp)
    tmp.replace(dst)
    return True


def link(target, path):
    path = Path(path)
    if path.is_symlink() and os.readlink(path) == str(target):
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.exists():
        shutil.rmtree(path)
    path.symlink_to(target)


class State:
    def __init__(self):
        self.d = json.loads(STATE.read_text()) if STATE.exists() else {}

    def get(self, k, default=None):
        return self.d.get(k, default)

    def set(self, k, v):
        self.d[k] = v
        tmp = STATE.with_name("state.json.tmp")
        tmp.write_text(json.dumps(self.d, indent=1, sort_keys=True))
        tmp.replace(STATE)


# ---------------------------------------------------------------- engine


def series():
    """(version, tag, patch paths in order, digest of base.json + series + patches)."""
    version = os.environ.get("PANDA_DEV_QEMU_VERSION")
    base = json.loads((ENGINE / "base.json").read_text())
    if not version or version not in base:
        die(f"engine/base.json has no {version!r}; is this the dev shell (tools/dev/dev.sh)?")
    tag = base[version]["tag"]
    lines = (ENGINE / "patches" / version / "series").read_text().splitlines()
    patches = [ENGINE / "patches" / l.strip() for l in lines if l.strip() and not l.startswith("#")]
    digest = sha(json.dumps(base[version], sort_keys=True),
                 *(p.name + file_sha(p) for p in patches))
    return version, tag, patches, digest


def overlay_files():
    src = ENGINE / "src"
    return sorted(str(p.relative_to(src)) for p in src.rglob("*") if p.is_file())


def import_tree(state, version, tag, digest):
    tarball = os.environ["PANDA_DEV_QEMU_TARBALL"]
    if TREE.exists():
        log(f"the engine base changed (now {tag}): importing a new tree")
        shutil.rmtree(TREE)
    run(["bash", ENGINE / "scripts/import-series.sh", version, tarball, TREE])
    state.set("engine", {"tag": tag, "series": digest, "overlay": overlay_files()})


def reimport_series(state, tag, patches, digest):
    """Apply the current series and overlay to a fresh checkout of the base, then move the tree to
    it: files whose content is unchanged keep their mtimes, so ninja rebuilds only what changed."""
    log("the patch series changed: applying it again")
    run(["python3", ENGINE / "scripts/check-series.py", ENGINE / "patches",
         os.environ["PANDA_DEV_QEMU_VERSION"]], quiet=True)
    wt = DEV / "qemu-reimport"
    if wt.exists():
        shutil.rmtree(wt)
    git("worktree", "prune")
    git("worktree", "add", "-q", "--detach", wt, f"base-{tag}")
    try:
        ident = ["-c", "user.email=dev@local", "-c", "user.name=dev"]
        r = subprocess.run(["git", "-C", wt, *ident, "am", "-q", *patches])
        if r.returncode:
            die("the series does not apply (above); the tree is unchanged: fix it and run again")
        shutil.copytree(ENGINE / "src", wt, dirs_exist_ok=True)
        run(["git", "-C", wt, "add", "-A"], quiet=True)
        run(["git", "-C", wt, *ident, "commit", "-qm",
             "VENDORED: src/ (NOT part of the series -- do not export)"], quiet=True)
        head = subprocess.run(["git", "-C", wt, "rev-parse", "HEAD"], check=True,
                              capture_output=True, text=True).stdout.strip()
    finally:
        git("worktree", "remove", "--force", wt)
    git("reset", "-q", "--hard", head)
    state.set("engine", {"tag": tag, "series": digest, "overlay": overlay_files()})


def sync_overlay():
    src = ENGINE / "src"
    changed = [f for f in overlay_files() if copy_if_changed(src / f, TREE / f)]
    if changed:
        log(f"overlay: {len(changed)} file(s) from engine/src: " + ", ".join(changed[:8]) +
            (" ..." if len(changed) > 8 else ""))
    return changed


def configure(state, targets, jobs):
    want = ",".join(f"{t}-softmmu" for t in targets)
    if (BUILD / "build.ninja").exists() and state.get("configured") == want:
        return
    BUILD.mkdir(exist_ok=True)
    log(f"configuring the engine for {want}")
    run([TREE / "configure", f"--target-list={want}", *CONFIGURE], cwd=BUILD)
    state.set("configured", want)


def ninja_engine(targets, jobs):
    outs = []
    for t in targets:
        outs += [f"libpanda-{t}-softmmu.so", f"libpanda_plugin_interface-{t}-softmmu.so",
                 f"guest-layout-{t}-softmmu.h"]
    outs += ["contrib/plugins/libpanda_plugin_interface.so"]
    # The firmware: what the default target builds in pc-bios/ (decompressed EDK2 images, device
    # trees) and qemu-bundle/, the install tree of links -L points at (prefix/share/qemu).
    allq = subprocess.run(["ninja", "-C", BUILD, "-t", "query", "all"], check=True,
                          capture_output=True, text=True).stdout.splitlines()
    inputs, on = [], False
    for l in allq:
        if l.startswith("  input:"):
            on = True
        elif l.startswith("  outputs:"):
            on = False
        elif on:
            inputs.append(l.split()[-1])
    extra = [i for i in inputs if i.startswith(("pc-bios/", "qemu-bundle"))]
    run(["ninja", "-C", BUILD, f"-j{jobs}", *outs, *extra])


def stage_prefix(targets):
    """prefix/: links into the build tree laid out like the flake's engine output."""
    lib = PREFIX / "lib"
    lib.mkdir(parents=True, exist_ok=True)
    want = {}
    for t in targets:
        for n in (f"libpanda-{t}-softmmu.so", f"libpanda_plugin_interface-{t}-softmmu.so"):
            want[n] = BUILD / n
    want["libpanda_plugin_interface.so"] = BUILD / "contrib/plugins/libpanda_plugin_interface.so"
    for p in lib.iterdir():
        if p.is_symlink() and p.name not in want and p.name != "panda-taint":
            p.unlink()
    for n, target in want.items():
        if not target.exists():
            die(f"the engine build has no {target}")
        link(target, lib / n)
    if (DEV / "h2b").is_dir():
        link(DEV / "h2b", lib / "panda-taint")
    link(FIRMWARE, PREFIX / "share/qemu")
    inc = PREFIX / "include/panda"
    for h in PANDA_HEADERS:
        copy_if_changed(TREE / "include/panda" / h, inc / h)
    for t in targets:
        copy_if_changed(BUILD / f"guest-layout-{t}-softmmu.h",
                        inc / "guest-layout" / f"guest-layout-{t}-softmmu.h")
    for p in (inc / "guest-layout").glob("guest-layout-*.h"):
        if p.name.removeprefix("guest-layout-").removesuffix("-softmmu.h") not in targets:
            p.unlink()


def step_engine(state, targets, jobs):
    version, tag, patches, digest = series()
    eng = state.get("engine") or {}
    if not (TREE / ".git").exists() or eng.get("tag") != tag:
        import_tree(state, version, tag, digest)
    elif eng.get("series") != digest or set(eng.get("overlay", [])) - set(overlay_files()):
        # A file removed from engine/src is removed from the tree by the same re-application.
        reimport_series(state, tag, patches, digest)
    sync_overlay()
    configure(state, targets, jobs)
    ninja_engine(targets, jobs)
    stage_prefix(targets)


# ---------------------------------------------------------------- headers (libpanda-ng)


def header_digest(state):
    """A digest of the content of every header in the engine's tree and build (and of
    libpanda-ng), hashing only the files whose size or mtime changed since the last scan."""
    cache = DEV / "header-scan.json"
    old = json.loads(cache.read_text()) if cache.exists() else {}
    new = {}
    for dirpath, dirnames, filenames in os.walk(TREE):
        dirnames[:] = [d for d in dirnames if d not in (".git", "pyvenv", "qemu-bundle")
                       and not d.endswith(".p")]
        for f in filenames:
            if not f.endswith(".h"):
                continue
            p = os.path.join(dirpath, f)
            try:
                st = os.stat(p)
            except FileNotFoundError:
                continue
            key = f"{st.st_size}:{st.st_mtime_ns}"
            o = old.get(p)
            new[p] = [key, o[1] if o and o[0] == key else file_sha(p)]
    cache.write_text(json.dumps(new))
    tool = ROOT / "tools/libpanda-ng"
    toolsum = [file_sha(p) for p in sorted(tool.rglob("*")) if p.is_file()]
    return sha(*(f"{p}={v[1]}" for p, v in sorted(new.items())), *toolsum)


def gen_headers(target):
    work = DEV / "headers" / target
    work.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, PYTHONPATH=os.environ["PANDA_DEV_GDB_PYTHONPATH"])
    r = subprocess.run(["bash", ROOT / "tools/libpanda-ng/run_one.sh", TREE,
                        BUILD / f"libpanda-{target}-softmmu.so"], cwd=work, env=env,
                       capture_output=True, text=True)
    (work / "log").write_text(r.stdout + r.stderr)
    return target, r.returncode, work


def step_headers(state, targets, jobs, force=False):
    digest = header_digest(state)
    done = state.get("headers", {})
    todo = [t for t in targets if force or done.get(t) != digest
            or not (PREFIX / f"include/panda-ng/panda_c_{t}.h").exists()]
    if todo:
        log(f"libpanda-ng: generating headers for {', '.join(todo)}")
        with concurrent.futures.ThreadPoolExecutor(max(1, min(jobs, len(todo)))) as ex:
            for t, rc, work in ex.map(gen_headers, todo):
                if rc:
                    die(f"libpanda-ng failed for {t}: {work / 'log'}")
                changed = [h.name for h in sorted(work.glob(f"panda_*_{t}.h"))
                           if copy_if_changed(h, PREFIX / "include/panda-ng" / h.name)]
                log(f"libpanda-ng {t}: " + (", ".join(changed) + " changed" if changed
                                            else "headers unchanged"))
                done[t] = digest
                state.set("headers", done)
    inc = PREFIX / "include/panda-ng"
    if inc.is_dir():
        for p in inc.glob("panda_*_*.h"):
            if re.sub(r"^panda_[a-z]+_(.*)\.h$", r"\1", p.name) not in targets:
                p.unlink()


# ---------------------------------------------------------------- h2b


def step_h2b(state, targets, jobs, force=False):
    sources = (ENGINE / "h2b/sources").read_text().splitlines()
    rows = [l.split() for l in sources if l.strip() and not l.startswith("#")]
    rows = [r for r in rows if r[0] in targets]
    if not rows:
        return
    h2b = ENGINE / "h2b"
    digest = sha(*(file_sha(p) for p in sorted(h2b.iterdir()) if p.is_file()),
                 *(f"{r[0]}:{s}:{file_sha(TREE / s)}" for r in rows for s in r[1:]))
    out = DEV / "h2b"
    have = all((out / f"libtaint-h2b-{r[0]}.so").exists() for r in rows)
    if not force and have and state.get("h2b") == digest:
        return
    log("building the taint H2b shadow helpers (engine/h2b)")
    run(["bash", h2b / "build-all.sh", BUILD, out])
    state.set("h2b", digest)
    link(out, PREFIX / "lib/panda-taint")


# ---------------------------------------------------------------- plugins


def plugin_list():
    lines = (ROOT / "plugins/config.panda").read_text().splitlines()
    return [l.strip() for l in lines if l.strip() and not l.startswith("#")]


def step_plugins(state, targets, jobs, names):
    env = dict(os.environ)
    env["NIX_CFLAGS_COMPILE"] = (env.get("NIX_CFLAGS_COMPILE", "") + " -std=c++17").strip()
    # debug (-O0 -g): at -O2, syscalls2.cpp alone takes minutes, and every plugin is rebuilt when
    # an engine header changes.
    opts = {"buildtype": "debug",
            "targets": ",".join(f"{t}-softmmu" for t in targets),
            # Relative to the source root when it is inside it (.dev/): meson refuses an absolute
            # include directory in the source tree.
            "engine_includedir": os.path.relpath(PREFIX / "include", ROOT)
            if (PREFIX / "include").resolve().is_relative_to(ROOT.resolve()) else str(PREFIX / "include"),
            "engine_libdir": str(PREFIX / "lib")}
    flags = [f"-D{k}={v}" for k, v in opts.items()]
    if not (PLUGINS_BUILD / "build.ninja").exists():
        if PLUGINS_BUILD.exists():
            shutil.rmtree(PLUGINS_BUILD)
        run(["meson", "setup", *flags, PLUGINS_BUILD, ROOT], env=env)
    elif state.get("plugins_opts") != opts:
        run(["meson", "configure", *flags, PLUGINS_BUILD], env=env)
    state.set("plugins_opts", opts)
    outs = []
    if names:
        known = plugin_list()
        for n in names:
            if n not in known:
                die(f"no plugin {n!r} in plugins/config.panda")
            outs += [f"plugins/libpanda-{n}_{t}-softmmu.so" for t in targets]
    run(["ninja", "-C", PLUGINS_BUILD, f"-j{jobs}", *outs], env=env)


# ---------------------------------------------------------------- pandare2's bindings


def bindings_one(arch):
    sys.path.insert(0, str(ROOT / "python/core"))
    os.chdir(ROOT / "python/core")
    import pandare_build
    header = PREFIX / f"include/panda-ng/panda_python_{arch}.h"
    pandare_build.handle_python(arch, header.read_text(), str(ROOT / "plugins"))
    return arch


def step_bindings(state, targets, jobs):
    plugin_headers = sorted((ROOT / "plugins").rglob("*.h"))
    common = sha(file_sha(ROOT / "python/core/pandare_build.py"),
                 *(f"{p}:{file_sha(p)}" for p in plugin_headers))
    done = state.get("bindings", {})
    autogen = ROOT / "python/core/pandare2/autogen"
    todo, digests = [], {}
    for t in targets:
        digests[t] = sha(common, file_sha(PREFIX / f"include/panda-ng/panda_python_{t}.h"))
        if done.get(t) != digests[t] or not (autogen / f"_pandare_ffi_{t}_softmmu.py").exists():
            todo.append(t)
    if not todo:
        return
    log(f"pandare2: generating the cffi bindings for {', '.join(todo)}")
    with concurrent.futures.ProcessPoolExecutor(max(1, min(jobs, len(todo)))) as ex:
        for arch in ex.map(bindings_one, todo):
            done[arch] = digests[arch]
            state.set("bindings", done)


# ---------------------------------------------------------------- python wrapper and env


def env_vars():
    return {
        "PANDA_PYTHON": str(DEV / "bin/python3"),
        "PANDA_LIB": str(PREFIX / "lib"),
        "PANDA_PLUGIN_DIR": str(PLUGINS_BUILD / "plugins"),
    }


def write_python():
    """bin/python3: the dev shell's Python (kept from garbage collection by the dev shell's
    profile), with pandare2 from python/core and the dev engine and plugins."""
    py = shutil.which("python3")
    v = env_vars()
    script = f"""#!/bin/sh
# Python with pandare2 from {ROOT}/python/core on the dev build in {DEV}
# (tools/dev/dev.sh). Use it as PANDA_PYTHON.
export PANDA_LIB={shlex.quote(v["PANDA_LIB"])}
export PANDA_PLUGIN_DIR={shlex.quote(v["PANDA_PLUGIN_DIR"])}
export PYTHONPATH={shlex.quote(str(ROOT / "python/core"))}${{PYTHONPATH:+:$PYTHONPATH}}
if [ -z "${{SSL_CERT_FILE:-}}" ] && [ -r /etc/ssl/certs/ca-certificates.crt ]; then
  export SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
fi
exec {shlex.quote(os.path.realpath(py))} "$@"
"""
    b = DEV / "bin"
    b.mkdir(exist_ok=True)
    p = b / "python3"
    if not p.exists() or p.read_text() != script:
        p.write_text(script)
        p.chmod(0o755)
    link("python3", b / "panda-python")


# ---------------------------------------------------------------- commands


def parse_targets(s):
    out = []
    for t in s.split(","):
        t = t.strip().removesuffix("-softmmu")
        if not t:
            continue
        if t not in ARCHES:
            die(f"unknown target {t!r} (one of {', '.join(ARCHES)})")
        out.append(t)
    return out


def cmd_build(args):
    state = State()
    targets = parse_targets(args.targets) if args.targets else state.get("targets")
    if not targets:
        host = {"x86_64": "x86_64", "aarch64": "aarch64", "arm64": "aarch64"}.get(platform.machine())
        targets = [host or "x86_64"]
    state.set("targets", targets)
    steps = args.steps or list(STEPS)
    for s in steps:
        if s not in STEPS:
            die(f"unknown step {s!r} (steps: {' '.join(STEPS)})")
    if "h2b" in steps and not args.steps and args.no_h2b:
        steps.remove("h2b")
    jobs = args.jobs or len(os.sched_getaffinity(0))
    names = [n for n in (args.plugins or "").split(",") if n]
    t0 = time.monotonic()
    times = []
    for s in STEPS:
        if s not in steps:
            continue
        if s != "engine" and not (BUILD / "build.ninja").exists():
            die("no engine build yet: run `tools/dev/dev.sh build` first")
        t = time.monotonic()
        if s == "engine":
            step_engine(state, targets, jobs)
        elif s == "headers":
            step_headers(state, targets, jobs, force=args.force_headers)
        elif s == "h2b":
            step_h2b(state, targets, jobs, force=bool(args.steps))
        elif s == "plugins":
            step_plugins(state, targets, jobs, names)
        elif s == "bindings":
            step_bindings(state, targets, jobs)
        times.append(f"{s} {time.monotonic() - t:.1f}s")
    write_python()
    log(f"done in {time.monotonic() - t0:.1f}s ({', '.join(times)}); targets {','.join(targets)}")
    log(f"PANDA_PYTHON={DEV / 'bin/python3'}")


def cmd_env(args):
    v = env_vars()
    v["PATH"] = f"{DEV / 'bin'}:$PATH"
    for k, val in v.items():
        print(f'export {k}="{val}"' if k == "PATH" else f"export {k}={shlex.quote(val)}")


def cmd_status(args):
    state = State()
    print(f"build directory  {DEV}")
    print(f"targets          {','.join(state.get('targets') or []) or '(none yet)'}")
    eng = state.get("engine") or {}
    if eng:
        current = series()[3] == eng.get("series")
        print(f"engine tree      {TREE} (base {eng.get('tag')}; series "
              f"{'current' if current else 'changed since the last build'})")
    for k, val in env_vars().items():
        print(f"{k:<16} {val}")
    if DEV.exists():
        du = subprocess.run(["du", "-sh", DEV], capture_output=True, text=True).stdout.split()
        print(f"size             {du[0] if du else '?'}")


def cmd_clean(args):
    what = args.what or "all"
    if what in ("plugins", "all"):
        shutil.rmtree(PLUGINS_BUILD, ignore_errors=True)
    if what in ("engine", "all"):
        for d in (TREE, DEV / "qemu-reimport", DEV / "headers", DEV / "h2b", PREFIX):
            shutil.rmtree(d, ignore_errors=True)
        (DEV / "header-scan.json").unlink(missing_ok=True)
    if what == "all":
        STATE.unlink(missing_ok=True)
        shutil.rmtree(DEV / "bin", ignore_errors=True)
    else:
        state = State()
        drop = {"plugins": ["plugins_opts"],
                "engine": ["engine", "configured", "headers", "h2b", "bindings"]}[what]
        for k in drop:
            state.d.pop(k, None)
        state.set("targets", state.get("targets"))
    log(f"removed the {what} build ({DEV})")


def main():
    ap = argparse.ArgumentParser(prog="tools/dev/dev.sh",
                                 description="panda-ng's incremental local build (docs/guides/development.md)")
    sub = ap.add_subparsers(dest="cmd", required=True)
    b = sub.add_parser("build", help="build what changed: the engine, its headers, the taint H2b "
                       "helpers, the plugins and pandare2's bindings (or only the steps named)")
    b.add_argument("steps", nargs="*", metavar="step", help=f"any of: {' '.join(STEPS)}")
    b.add_argument("--targets", help="comma-separated arches, e.g. x86_64,aarch64 (remembered; "
                   "default: the last ones, else the host's)")
    b.add_argument("--plugins", help="comma-separated plugins to build (default: all of "
                   "plugins/config.panda)")
    b.add_argument("-j", "--jobs", type=int, help="parallel jobs (default: the CPUs this process "
                   "may run on)")
    b.add_argument("--force-headers", action="store_true",
                   help="run libpanda-ng even if no header changed")
    b.add_argument("--no-h2b", action="store_true", help="skip the taint H2b helpers")
    b.set_defaults(fn=cmd_build)
    e = sub.add_parser("env", help="print shell exports for PANDA_PYTHON, PANDA_LIB, PANDA_PLUGIN_DIR")
    e.set_defaults(fn=cmd_env)
    s = sub.add_parser("status", help="what is built and where")
    s.set_defaults(fn=cmd_status)
    c = sub.add_parser("clean", help="remove the build (all, or the engine or plugins part)")
    c.add_argument("what", nargs="?", choices=["all", "engine", "plugins"])
    c.set_defaults(fn=cmd_clean)
    args = ap.parse_args()
    DEV.mkdir(parents=True, exist_ok=True)
    if args.cmd in ("build", "clean"):
        lock = open(DEV / "lock", "w")
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            log(f"waiting for another dev.sh in {DEV}")
            fcntl.flock(lock, fcntl.LOCK_EX)
    args.fn(args)


if __name__ == "__main__":
    main()
