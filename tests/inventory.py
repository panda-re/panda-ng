"""Small declarative guest selection inventory, schema 1 (no engine claims).

Each row describes requirements/coverage intent, never implemented mode support.
Explicit TESTS may also name unlisted scripts, which retain unknown requirements.
"""
import os
import platform
import shutil

SCHEMA_VERSION = 1
TIERS = ("quick", "focused-guest", "broad")
TARGETS = ["x86_64", "aarch64", "riscv64"]
# Preserve the original runner selection and order unless a tier is explicitly requested.
LEGACY_DEFAULT = "guest api osi_isf blocks block_activation callbacks calls mem mmio late reset teardown asidstory tbpc hookalias rr rrcb rrstop rrload fuzz fastrestore fastcov syscalls"


def entry(name, tier="broad", modes=None, targets=None, requires=None, script=None, args=None):
    return {"name": name, "script": script or name, "tier": tier,
            "targets": targets or TARGETS, "host": ["Linux"],
            "requires": ["image", *(requires or [])], "modes": modes or ["tcg"],
            "args": args or [], "coverage": "intent only; verdict requires actual execution"}


INVENTORY = [
    entry("guest", "quick"), entry("api", "quick"),
    entry("osi_isf"), entry("blocks", "focused-guest"),
    entry("block_activation", "focused-guest"),
    entry("callbacks", "focused-guest"),
    entry("calls"), entry("mem"), entry("mmio", "focused-guest"), entry("late"),
    entry("reset"), entry("teardown", "focused-guest"), entry("asidstory"),
    entry("tbpc", "focused-guest"), entry("hookalias"),
    entry("rr", "focused-guest", ["record", "replay"]),
    entry("rrcb", modes=["record", "replay"]), entry("rrstop", modes=["record"]),
    entry("rrload", modes=["record", "replay"]),
    entry("rr_blocks", "focused-guest", ["record", "replay"]),
    entry("fuzz", modes=["fuzz"]), entry("fastrestore", "focused-guest", ["tcg", "restore"]),
    entry("fastcov", modes=["fuzz", "restore"]), entry("syscalls", "focused-guest"),
    entry("fastrestore_guards_rr", "focused-guest", ["record", "replay", "restore-rejection"],
          script="fastrestore_guards", args=["rr"]),
    entry("fastrestore_guards_kvm", modes=["kvm", "restore-rejection"],
          requires=["host-target", "kvm"], script="fastrestore_guards", args=["notcg"]),
    entry("fastrestore_hotplug", modes=["tcg", "restore", "hotplug"], targets=["x86_64"]),
    entry("fastrestore_vsock", modes=["tcg", "restore", "vhost-user"], requires=["vhost-device-vsock"]),
    entry("fastrestore_vsock_kernel", modes=["tcg", "restore", "vhost-kernel"],
          requires=["vhost-vsock", "host-target"], script="fastrestore_vsock", args=["--kernel"]),
    entry("kpti", targets=["x86_64"]), entry("physmem"),
]


def select(tier=None, explicit=None):
    if tier is not None and tier not in TIERS:
        raise ValueError(f"unknown TIER {tier!r}; choose {', '.join(TIERS)}")
    by_name = {row["name"]: row for row in INVENTORY}
    if not explicit and tier is None:
        explicit = LEGACY_DEFAULT
    if explicit:
        return [by_name.get(name, {**entry(name), "targets": [], "requires": ["image"],
                                  "coverage": "unlisted test; target/mode requirements unknown"})
                for name in explicit.split()]
    limit = TIERS.index(tier)
    return [row for row in INVENTORY if TIERS.index(row["tier"]) <= limit]


def unmet(row, arch):
    reasons = []
    if row["targets"] and arch not in row["targets"]:
        reasons.append(f"target {arch} not in {row['targets']}")
    if platform.system() not in row["host"]:
        reasons.append(f"host {platform.system()} not in {row['host']}")
    for requirement in row["requires"]:
        if requirement == "host-target":
            host = {"arm64": "aarch64", "amd64": "x86_64"}.get(platform.machine(), platform.machine())
            if arch != host:
                reasons.append(f"requires host target {host}, image is {arch}")
        elif requirement in ("kvm", "vhost-vsock"):
            device = "/dev/" + requirement
            try:
                fd = os.open(device, os.O_RDWR)
                os.close(fd)
            except OSError as e:
                reasons.append(f"requires usable {device}: {e}")
        elif requirement == "vhost-device-vsock":
            executable = os.environ.get("VHOST_DEVICE_VSOCK") or "vhost-device-vsock"
            if not shutil.which(executable):
                reasons.append(f"requires executable {executable}")
    return reasons


if __name__ == "__main__":
    import json
    import sys
    tier = sys.argv[1] if len(sys.argv) > 1 else "broad"
    rows = select(tier)
    print(json.dumps({"schema_version": SCHEMA_VERSION, "tier": tier, "count": len(rows),
                      "tests": rows, "note": "declared coverage, not engine implementation or executed evidence"}, indent=2))
