#!/usr/bin/env python3
"""Strict offline analysis: python3 tests/perf/analyze.py OUT.

No samples are dropped. Any invalid/missing cell invalidates the whole experiment.
Ratios > 1 mean slower; copy_mbs is inverted. No performance acceptance gate.
"""
import hashlib
import json
import math
from pathlib import Path
import random
import re
import statistics
import sys


class Invalid(ValueError):
    pass


def digest(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1024 * 1024), b""):
            h.update(block)
    return h.hexdigest()


def load(path):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise Invalid(f"duplicate JSON key: {key}")
            result[key] = value
        return result
    return json.loads(Path(path).read_text(), object_pairs_hook=unique,
                      parse_constant=lambda x: (_ for _ in ()).throw(Invalid(x)))


def positive(value):
    try:
        n = float(value)
    except (ValueError, TypeError) as e:
        raise Invalid(f"invalid number: {value}") from e
    if not math.isfinite(n) or n <= 0:
        raise Invalid(f"not finite and positive: {value}")
    return n


def parse(text, metrics, exits, correctness):
    # Console CRLF is normal. Markers must be whole records, never substring matches.
    text = text.replace("\r\n", "\n")
    lines = text.splitlines()
    footers = [line for line in lines if re.search(r"\b(?:RC|WALL)\b", line)]
    if len(footers) != 1 or not lines or footers[0] != lines[-1]:
        raise Invalid("missing/duplicate/misplaced RC/WALL")
    match = re.fullmatch(r"RC (-?\d+) WALL (\S+)", footers[0])
    if not match or int(match[1]) != 0:
        raise Invalid("malformed footer or nonzero RC")
    wall = positive(match[2])
    done = [i for i, line in enumerate(lines) if re.search(r"\bALLDONE\b", line)]
    if len(done) != 1 or lines[done[0]] != "ALLDONE":
        raise Invalid("missing/duplicate/malformed ALLDONE")
    found = {}
    for i, line in enumerate(lines):
        if re.search(r"\bEXIT\b", line):
            m = re.fullmatch(r"EXIT (\S+) (\d+)", line)
            if not m or m[1] in found or int(m[2]) != 0 or i > done[0]:
                raise Invalid("malformed/duplicate/nonzero guest EXIT")
            found[m[1]] = 0
    if set(found) != set(exits):
        raise Invalid("missing or unexpected guest EXIT")
    values = {"wall": wall}
    body = "\n".join(lines[:done[0]])
    for metric in metrics:
        if not re.fullmatch(r"[a-z][a-z0-9_]*", metric) or metric == "wall":
            raise Invalid(f"invalid metric name: {metric}")
        matches = re.findall(r"(?<![\w])" + re.escape(metric) + r"=([^\s]*)", text)
        if len(matches) != 1 or not re.search(r"\b" + re.escape(metric) + "=", body):
            raise Invalid(f"missing/duplicate/misplaced metric: {metric}")
        values[metric] = positive(matches[0])
    for record in correctness:
        if lines.count(record) != 1 or body.splitlines().count(record) != 1:
            raise Invalid(f"missing/duplicate correctness record: {record}")
    return values


def bootstrap(xs, draws=4000, seed=1):
    """Historical fixed percentile endpoints, resampling complete paired ratios."""
    if len(xs) < 3:
        return None
    rng = random.Random(seed)
    medians = sorted(statistics.median(rng.choices(xs, k=len(xs))) for _ in range(draws))
    return [medians[int(.025 * draws)], medians[int(.975 * draws) - 1]]


def order(names, rep):
    k = rep % len(names)
    return names[k:] + names[:k]


def analyze(root):
    root = Path(root)
    manifest = load(root / "manifest.json")
    config = manifest["config"]
    # Reanalysis must not bless a run whose pinned inputs were subsequently changed.
    config_path = Path(manifest["config_path"])
    if digest(config_path) != manifest["config_sha256"] or load(config_path) != config:
        raise Invalid("configuration identity/hash changed")
    if not manifest["artifacts"]:
        raise Invalid("missing artifact identities")
    for artifact in manifest["artifacts"]:
        path = Path(artifact["path"])
        if (not path.is_absolute() or str(path.resolve()) != artifact["resolved"]
                or digest(path) != artifact["sha256"]):
            raise Invalid("artifact identity/hash changed")
    if manifest["schema"] != 1 or manifest.get("state") != "complete":
        raise Invalid("experiment incomplete")
    if manifest.get("contamination"):
        raise Invalid("experiment marked contaminated")
    names = list(config["variants"])
    if (type(config["repeats"]) is not int or not 3 <= config["repeats"] <= 100
            or len(names) < 3 or config["reference"] not in names
            or len(config["aa"]) != 2 or len(set(config["aa"])) != 2
            or not set(config["aa"]) <= set(names)
            or config["variants"][config["aa"][0]] != config["variants"][config["aa"][1]]):
        raise Invalid("invalid paired design/A-A configuration")
    expected = [(phase, rep, name) for phase, count in [("preflight", 1), ("sample", config["repeats"])]
                for rep in range(count) for name in order(names, rep)]
    cells = manifest["runs"]
    if [(c["phase"], c["rep"], c["variant"]) for c in cells] != expected:
        raise Invalid("missing/duplicate/unordered cell or incomplete pair")
    logs = {c["log"] for c in cells}
    if len(logs) != len(cells) or {p.name for p in root.glob("*.con")} != logs:
        raise Invalid("missing/extra/duplicate console artifact")
    samples = {name: [] for name in names}
    for cell in cells:
        path = root / cell["log"]
        if path.name != cell["log"] or digest(path) != cell["sha256"]:
            raise Invalid("console path/hash mismatch")
        if cell["rc"] != 0 or cell["timeout"]:
            raise Invalid("failed/timed out process")
        values = parse(path.read_text(errors="strict"), config["metrics"],
                       config["exits"], config["correctness"])
        variant = config["variants"][cell["variant"]]
        if variant["kind"] == "shared":
            maps = root / cell["maps"]
            if maps.name != cell["maps"] or digest(maps) != cell["maps_sha256"]:
                raise Invalid("loaded maps hash mismatch")
            engine = str(Path(variant["engine"]).resolve())
            if not any(line.split()[-1:] == [engine] for line in maps.read_text().splitlines()):
                raise Invalid("shared engine not observed loaded")
        if cell["phase"] == "sample":
            samples[cell["variant"]].append(values)
    rows = []
    comparisons = [(name, config["reference"]) for name in names if name != config["reference"]]
    comparisons.append((config["aa"][1], config["aa"][0]))
    for name, ref_name in comparisons:
        reference = samples[ref_name]
        for metric in ["wall", *config["metrics"]]:
            ratios = [sample[metric] / ref[metric] for sample, ref in zip(samples[name], reference)]
            if metric in config["higher_better"]:
                ratios = [1 / r for r in ratios]
            for ratio in ratios:
                positive(ratio)
            rows.append({"variant": name, "reference": ref_name, "metric": metric, "paired_n": len(ratios),
                         "paired_ratios": ratios, "median_ratio": statistics.median(ratios),
                         "ci95": bootstrap(ratios)})
    return {"schema": 1, "status": "inconclusive: A/A and host noise require review",
            "bootstrap": {"draws": 4000, "seed": 1, "endpoints": [100, 3899]},
            "manifest_sha256": digest(root / "manifest.json"), "raw_samples": samples, "rows": rows}


if __name__ == "__main__":
    try:
        print(json.dumps(analyze(sys.argv[1]), indent=2, allow_nan=False))
    except (Invalid, OSError, ValueError, KeyError, TypeError) as e:
        sys.exit(f"INVALID: {e}")
