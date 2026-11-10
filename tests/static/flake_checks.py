#!/usr/bin/env python3
"""The flake's checks and the tests' metadata agree: every check that runs a test script names
that script's PANDA_TEST nix_check, and every nix_check is a check that runs its script.

Run: python3 tests/static/flake_checks.py [flake.nix]   (Python only: no nix, engine or guest)

A check runs a script when its build command has `python3` or `bash` followed by the script's
path (${./tests/static/x.py}, ${./tests/static}/x.py, ${./.}/tests/x.py). Checks that only copy
a script (perf-tools) are not counted.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TESTS = os.path.dirname(HERE)
sys.path.insert(0, TESTS)
from pandatest import meta  # noqa: E402

PANDA_TEST = dict(area="tests", tier="static", args=["{repo}/flake.nix"], nix_check="flake-checks")

RUN = re.compile(r"(?:python3|bash)\s+(?:-I\s+)?\$\{\./([^}]*)\}([\w./-]*)")


def flake_checks(text):
    """{check name: {script path relative to the repo}} from the flake's checks attribute set."""
    start = text.index("checks = forAllSystems")
    start = re.compile(r"\n\s*in\n").search(text, start).end()     # past its let bindings
    end = text.index("devShells = forAllSystems", start)
    block = text[start:end].splitlines()
    names = [(i, ln) for i, ln in enumerate(block) if re.match(r"\s+[\w-]+\s*=", ln)]
    # The checks are the least-indented attributes in the block (their bodies' keys are deeper).
    indent = min(len(ln) - len(ln.lstrip()) for i, ln in names)
    heads = [i for i, ln in names if len(ln) - len(ln.lstrip()) == indent]
    out = {}
    for a, b in zip(heads, heads[1:] + [len(block)]):
        name = block[a].split("=")[0].strip()
        scripts = set()
        for m in RUN.finditer("\n".join(block[a:b])):
            p = os.path.normpath((m[1] + m[2]).lstrip("./") if m[1] != "." else m[2].lstrip("/"))
            scripts.add(p)
        out[name] = scripts
    return out


def main():
    flake = sys.argv[1] if len(sys.argv) > 1 else os.path.join(TESTS, "..", "flake.nix")
    checks = flake_checks(open(flake).read())
    declared = {}
    for path in meta.discover(TESTS):
        found, m = meta.read(path)
        if found and m and m.get("nix_check"):
            declared[os.path.relpath(path, os.path.dirname(TESTS))] = m["nix_check"]
    errors = []
    for script, check in sorted(declared.items()):
        if check not in checks:
            errors.append(f"{script}: nix_check {check!r} is not a flake check")
        elif script not in checks[check]:
            errors.append(f"{script}: flake check {check!r} does not run it "
                          f"(it runs {', '.join(sorted(checks[check])) or 'no test script'})")
    for check, scripts in sorted(checks.items()):
        for script in sorted(scripts):
            if script.startswith("tests/") and declared.get(script) != check:
                errors.append(f"flake check {check!r} runs {script}, whose PANDA_TEST "
                              f"{'says nix_check=' + repr(declared[script]) if script in declared else 'names no nix_check'}")
    for e in errors:
        print("FAIL", e)
    print(f"{len(declared)} scripts with a nix_check, {sum(map(len, checks.values()))} scripts run by "
          f"{len(checks)} flake checks: {'FAIL' if errors else 'PASS'}")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
