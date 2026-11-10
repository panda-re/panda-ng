"""The engine build the tests run on, and the fixture registry: fixture names to cached paths,
identities and the commands that build them.

    none                                no fixture
    busybox-<arch>                      a tests/mkbusybox.py image (json, qcow2, initrd): the file
                                        is busybox-<mkbusybox's name for the arch>.json
    testmachine-<arch>[-smpN][-intel]   a tests/testmachine.py snapshot at the agent, on busybox-<arch>
    cloud-<distro>-<arch>               a pandare2.mkimage image

Locations (each overridable, so existing images can be used in place):
    $PANDATEST_CACHE   default $XDG_CACHE_HOME/panda-tests (~/.cache/panda-tests)
    $PANDATEST_BUSYBOX the busybox-<arch>.json directory (default <cache>/busybox/<recipe>)
    $TESTMACHINE_CACHE the machines (default <cache>/machines)
    $PANDATEST_IMAGES  the mkimage images (default <cache>/images)
    $PANDA_BUSYBOX_DL  tests/busybox.sh's downloads, which a busybox build needs
                       (default ~/.cache/panda-busybox)
"""
import hashlib
import json
import os
import re
import subprocess
import sys

from .meta import BUSYBOX_ARCH, canonical

TESTS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(TESTS)


def _sha(*parts):
    h = hashlib.sha256()
    for p in parts:
        h.update(p if isinstance(p, bytes) else str(p).encode())
    return h.hexdigest()[:16]


def _git(tree, *args):
    r = subprocess.run(["git", "-C", tree, *args], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else None


class Engine:
    """Where the engine, plugins and pandare2 come from: a tools/dev/dev.sh build directory
    (--build, $PANDA_DEV_DIR or <repo>/.dev), with PANDA_PYTHON, PANDA_LIB, PANDA_PLUGIN_DIR,
    PANDA_ENGINE_SRC (the patched QEMU tree) and PANDA_ENGINE_PREFIX (its installed headers)
    overriding it."""

    def __init__(self, build=None):
        dev = build or os.environ.get("PANDA_DEV_DIR") or os.path.join(REPO, ".dev")
        dev = os.path.abspath(dev)
        self.dev = dev if os.path.isfile(os.path.join(dev, "bin", "python3")) else None
        d = (lambda *p: os.path.join(self.dev, *p)) if self.dev else (lambda *p: None)
        env = os.environ.get
        self.python = env("PANDA_PYTHON") or d("bin", "python3")
        self.lib = env("PANDA_LIB") or d("prefix", "lib")
        self.plugins = env("PANDA_PLUGIN_DIR") or d("plugins", "plugins")
        self.src = env("PANDA_ENGINE_SRC") or d("qemu")
        self.prefix = env("PANDA_ENGINE_PREFIX") or d("prefix")
        self.lock = d("lock")
        # The source tree the build came from: the dev directory's repository.
        self.tree = os.path.dirname(self.dev) if self.dev else REPO
        self.rev = _git(self.tree, "rev-parse", "HEAD")
        status = _git(self.tree, "status", "--porcelain", "--untracked-files=no")
        self.dirty = bool(status) if status is not None else None

    def identity(self):
        return dict(tree=self.tree, rev=self.rev, dirty=self.dirty, dev=self.dev, python=self.python)

    def env(self):
        e = {"PANDA_PYTHON": self.python, "PANDA_LIB": self.lib, "PANDA_PLUGIN_DIR": self.plugins}
        return {k: v for k, v in e.items() if v}

    def has_target(self, panda_arch):
        if not self.lib or not panda_arch:
            return True
        return os.path.exists(os.path.join(self.lib, f"libpanda-{panda_arch}-softmmu.so"))

    def has_plugin(self, name, panda_arch):
        if not self.plugins or not panda_arch:
            return True
        return os.path.exists(os.path.join(self.plugins, f"libpanda-{name}_{panda_arch}-softmmu.so"))


def cache_root():
    return os.environ.get("PANDATEST_CACHE") or os.path.join(
        os.environ.get("XDG_CACHE_HOME") or os.path.expanduser("~/.cache"), "panda-tests")


class Fixture:
    def __init__(self, name, kind, arch=None, path=None, ready=True, reason="", identity=None,
                 env=None, copy=False, builds=()):
        self.name, self.kind, self.arch, self.path = name, kind, arch, path
        self.ready, self.reason, self.identity = ready, reason, identity or name
        self.env, self.copy, self.builds = env or {}, copy, list(builds)

    def record(self):
        return dict(name=self.name, identity=self.identity, path=self.path)


def _content_id(json_path):
    """The image as found: its json, and the size and mtime of each file it names."""
    with open(json_path, "rb") as f:
        raw = f.read()
    img, parts = json.loads(raw), [raw]
    files = [img.get("qcow")] + [a for a in img.get("extra_args", []) if a.startswith("/")]
    for p in files:
        if p:
            p = p if os.path.isabs(p) else os.path.join(os.path.dirname(json_path), p)
            if os.path.isfile(p):
                st = os.stat(p)
                parts.append(f"{p}:{st.st_size}:{st.st_mtime_ns}")
    return _sha(*parts)


def busybox(arch, engine):
    recipe = _sha(open(os.path.join(TESTS, "mkbusybox.py"), "rb").read(),
                  *re.findall(r"^(?:LB_VERSION|BUSYBOX_REV|PPC32_\w+)=.*$",
                              open(os.path.join(TESTS, "busybox.sh")).read(), re.M))
    d = os.environ.get("PANDATEST_BUSYBOX") or os.path.join(cache_root(), "busybox", recipe)
    bb = BUSYBOX_ARCH.get(arch, arch)
    path = os.path.join(d, f"busybox-{bb}.json")
    name = f"busybox-{arch}"
    if os.path.isfile(path):
        return Fixture(name, "busybox", arch, path, identity=_content_id(path), copy=True)
    dl = os.environ.get("PANDA_BUSYBOX_DL") or os.path.expanduser("~/.cache/panda-busybox")
    builds = []
    if os.path.isdir(os.path.join(dl, "kernels")) and os.path.isdir(os.path.join(dl, "busybox", bb)):
        builds = [[engine.python, os.path.join(TESTS, "mkbusybox.py"), bb, "-o", d,
                   "--kernels", os.path.join(dl, "kernels"), "--busybox", os.path.join(dl, "busybox")]]
        how = "--build-fixtures builds it"
    else:
        how = f"no downloads in {dl} to build it from: run tests/busybox.sh {bb} once, or set PANDA_BUSYBOX_DL"
    return Fixture(name, "busybox", arch, path, False, f"{name}: no {path}; {how}", recipe,
                   copy=True, builds=builds)


def testmachine(name, arch, smp, intel, engine):
    bb = busybox(arch, engine)
    cache = os.environ.get("TESTMACHINE_CACHE") or os.path.join(cache_root(), "machines")
    env = {"TESTMACHINE_CACHE": cache}
    build = [engine.python, os.path.join(TESTS, "testmachine.py"), bb.path, "--smp", str(smp)] + \
        (["--intel"] if intel else [])
    if not bb.ready:
        return Fixture(name, "testmachine", arch, bb.path, False, f"needs {bb.reason}", env=env,
                       builds=bb.builds + [build] if bb.builds else [])
    sys.path.insert(0, TESTS)
    import testmachine as tm
    with open(bb.path) as f:
        img = json.load(f)
    initrd_src = tm.arg_after(img["extra_args"], "-initrd")
    with open(initrd_src, "rb") as f:
        initrd = f.read()
    saved = os.environ.get("TESTMACHINE_CACHE")
    os.environ["TESTMACHINE_CACHE"] = cache
    try:
        d = tm.machine_dir(img, initrd_src, initrd, smp, intel, img["default_mem"])
    finally:
        if saved is None:
            del os.environ["TESTMACHINE_CACHE"]
        else:
            os.environ["TESTMACHINE_CACHE"] = saved
    ident = os.path.basename(d)
    if os.path.isfile(os.path.join(d, "snapshot.qcow2")):
        return Fixture(name, "testmachine", arch, bb.path, identity=ident, env=env)
    return Fixture(name, "testmachine", arch, bb.path, False,
                   f"{name}: no snapshot in {d}; --build-fixtures builds it", ident, env, builds=[build])


def cloud(name, distro, arch, engine):
    d = os.environ.get("PANDATEST_IMAGES") or os.path.join(cache_root(), "images")
    path = os.path.join(d, f"{distro}-{arch}.json")
    if os.path.isfile(path):
        return Fixture(name, "cloud", arch, path, identity=_content_id(path), copy=True)
    mk = os.path.join(REPO, "python", "core", "pandare2", "mkimage.py")
    recipe = _sha(open(mk, "rb").read(), distro, arch) if os.path.isfile(mk) else name
    return Fixture(name, "cloud", arch, path, False, f"{name}: no {path}; --build-fixtures builds it",
                   recipe, copy=True,
                   builds=[[engine.python, "-m", "pandare2.mkimage", f"{distro}-{arch}", "-o", d]])


def resolve(name, engine):
    if name == "none":
        return Fixture("none", "none")
    m = re.fullmatch(r"testmachine-([a-z0-9_]+?)(?:-smp(\d+))?(-intel)?", name)
    if m:
        return testmachine(name, canonical(m[1]), int(m[2] or 1), bool(m[3]), engine)
    m = re.fullmatch(r"busybox-([a-z0-9_]+)", name)
    if m:
        return busybox(canonical(m[1]), engine)
    m = re.fullmatch(r"cloud-(.+)-([a-z0-9_]+)", name)
    if m:
        return cloud(name, m[1], m[2], engine)
    return Fixture(name, "unknown", ready=False, reason=f"unknown fixture {name!r}")
