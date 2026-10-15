# qemu_builder

The IGLOO QEMU delta, carried as an explicit patch series applied to a pristine
upstream QEMU **release tarball** — replacing the long-lived fork of
`rehosting/qemu`, which sat on a mid-cycle upstream staging merge.

This is the second application of the pattern `linux_builder` adopted in its
PR #59; read that repo as the reference implementation.

## Layout

```
base.json               upstream release tag, URL, tarball hash, expected patched-tree hash
patches/11.1.0/         the series + its `series` file (quilt convention)
src/                    the half of the delta we authored outright — copied in, never patched
configs/                the configure feature set as data
build.sh                builds penguin-qemu from a prepared tree; reads configs/<profile>.json
nix/source.nix          tarball + series + src/ -> a source derivation
nix/qemu.nix            penguin-qemu package (libqemu-system-*.so + qemu-img + CFFI bindings)
scripts/verify-series.sh    THE GATE
scripts/import-series.sh    tarball + series -> a dev git tree
scripts/export-series.sh    dev git tree -> the committed series
scripts/probe-versions.sh   portability report across upstream refs
tests/guest/             the guest harness for the pagein and syscall-engine sources (not in CI)
```

## The invariant that makes this work

**No patch in the series ever references a file we created.**

The delta splits three ways, and only the third is a patch series:

| class | files | destination |
|---|---|---|
| builder scaffolding (`build.sh`, `Dockerfile`, flake, CI, `nix/`) | 9 | this repo's root — never a patch |
| new QEMU source we authored (`system/penguin.c`, `include/system/penguin.h`, `scripts/penguin-*.py`) | 5 | `src/`, copied into the tree |
| **edits to upstream files** | **33** | **`patches/`** |

Getting this wrong is not a style question. Exporting our 38 development commits
chronologically and applying them to v11.1.0 gets **8 of 38**, because 26 of
those patches edit files we invented earlier in the same series. Enforcing the
invariant and curating by topic gets **12 of 12**.

## Why the series is curated, not chronological

`linux_builder`'s series is a numbered `format-patch` export that preserves
commit order. That costs it nothing, because its base never moves. Ours must
move, and chronology is where the conflicts come from:

- `797f446cf8` ("add multi-target system emulation hooks") touches **9 of the 12
  topics**; `d7f87bf73e` touches 5. Those two commits made the history
  unsplittable and every downstream fixup a conflict.
- Three separate "tighten hypercall match" commits each conflict with the
  original add. `0032` reverts an `rt` symlink `0030` committed by accident.

So the 38 commits were folded into 12 topical patches, each naming the original
commits it folds. The pre-conversion history is preserved as a tag; provenance is
recoverable without being load-bearing.

## Why a release tarball, and why that is easier here than for the kernel

Both verified against v11.1.0, not assumed:

- QEMU's `.gitattributes` has **no `export-ignore`**, so there is no subtractive
  tag-vs-tarball skew. `linux_builder`'s gate has to whitelist four paths; ours
  whitelists none. All 33 files the series touches are byte-identical between the
  `v11.1.0` tag and the `v11.1.0` tarball.
- The tarball ships `subprojects/{keycodemapdb,berkeley-softfloat-3,berkeley-testfloat-3}`
  already populated and build-ready. A git base needed three `fetchFromGitLab`
  pins with hand-maintained per-version hashes; `nix/qemu.nix` lost 41 lines
  when that block went away.
- QEMU has 15 git submodules (`roms/*`, `tests/lcitool`). The series touches
  none of them, and a tarball base avoids fetching them entirely.

## Configs are data, and so are their dependencies

`configs/<profile>.json` holds the configure feature set. Entries are objects,
not strings:

```json
{ "flag": "--enable-capstone",
  "why":  "QEMU's own disassembler ... the one genuinely new dependency at ~29 MB",
  "nixDeps": ["capstone"] }
```

Three things that buys over a shell array:

1. **The rationale survives.** `rehosting/qemu`'s `build.sh` carries ~40 lines of
   carefully-costed justification in comments. Flattening that into a JSON list
   of strings would have thrown it away; `why` keeps it, and keeps it queryable.
2. **The flag list and the dependency list cannot drift.** `nix/qemu.nix` no
   longer hand-lists the twelve libraries backing the `--enable-*` group —
   `flake.nix` derives `buildInputs` from `nixDeps`. `rehosting/qemu` keeps the
   same two lists in two files with a comment asking a human to sync them; these
   are *hard* enables, so a desync is a configure failure. Here it is
   unrepresentable.
3. **CI can check the set for incoherence** — duplicate flags, and any flag
   present as both `--enable-` and `--disable-`.

`build.sh` reads `.flag`; nothing else needs to know the schema.

`scripts/check-config-contract.sh` is the gate: it asserts every declared
`nixDep` is in the built artifact's **runtime closure**. Two things it has to get
right, both of which bit during development —

- it resolves deps through **this flake's** pinned nixpkgs, not `<nixpkgs>`; the
  channel is a different revision, so the store hashes never match and all
  twelve deps look absent
- it accepts **any output** of a dep, because nixpkgs' default output is not
  always the linked one: `curl`, `bzip2` and `libjpeg` default to `bin` while the
  closure carries their `lib` output

## The gate

```bash
./scripts/verify-series.sh 11.1.0 qemu-11.1.0.tar.xz
```

Three assertions, increasing in strength:

1. the tarball is the one `base.json` pins (sha256)
2. the series applies with **strict context** — `git am`, deliberately *without*
   `--3way`, so upstream moving under us is a loud failure a human ports rather
   than a silent auto-merge
3. the patched tree hashes to `base.json`'s `patchedTree`

Assertion 3 is the one that earns its keep. `linux_builder`'s `6db7363` had to
repair a patch that was **corrupt rather than stale** — and it still applied
cleanly. Verified here: corrupting one line of `0012` still passes 12/12 in step
2 and is caught by step 3.

## Working on it

```bash
./scripts/import-series.sh 11.1.0 qemu-11.1.0.tar.xz /tmp/qemu-dev
cd /tmp/qemu-dev && git commit ...        # develop normally
./scripts/export-series.sh 11.1.0 /tmp/qemu-dev
./scripts/verify-series.sh 11.1.0 qemu-11.1.0.tar.xz --record
```

`import-series.sh` adds the vendored `src/` overlay as a final commit marked
`VENDORED:`; `export-series.sh` skips it, so the invariant holds automatically.

## Versioning

One live version, rebased forward. `patches/<version>/` is keyed by upstream
release so a second live version is *additive*, but we deliberately maintain
exactly one — unlike the kernel, nothing in our QEMU use needs two.

## Portability

`./scripts/probe-versions.sh 11.1.0 <qemu.git> v11.0.0 v10.2.0 upstream/master`

Failure classes: `ok` (strict), `drift` (3-way resolves it), `moved` (upstream
renamed a file the patch targets), `conflict` (needs a human).

| ref | ok | drift | moved | conflict |
|---|---|---|---|---|
| **v11.1.0** (base) | **12** | 0 | 0 | 0 |
| `upstream/master` | 9 | 2 | 0 | 1 |
| v11.0.0 | 6 | 3 | 1 | 2 |
| v10.2.0 | 4 | 2 | 2 | 4 |

Forward motion is cheap: on a master a week past our base, 11 of 12 patches need
no human attention. Backward degrades faster, as expected — the patches assume
post-v11.0.0 code. `moved` is a path rewrite; `drift` is auto-resolvable.

## CI

Four workflows. The shape matters: **everything expensive is gated behind a
gate that takes minutes.** That is the practical payoff of a patch-based repo
over a fork — a broken patch fails in `Series`, not two hours into a build.

| workflow | trigger | what it does |
|---|---|---|
| `series.yml` | reusable (`workflow_call`) | the fast gate: `verify-series.sh`, config-data validity, and export round-trip |
| `build.yml` | PRs | `series` → `nix flake check` → `nix build .#dist` → artifact layout → delta-presence → every declared arch has a library |
| `publish.yml` | **manual only** | `series` → build → release with `SHA256SUMS` and a `BASE` file naming the upstream release |
| `upstream-canary.yml` | weekly + manual | probes the series against `upstream/master` and files/updates one rolling issue |

`series.yml` has no `pull_request` trigger of its own on purpose — `build.yml`
and `publish.yml` each call it, so it runs exactly once per event instead of
twice.

**`publish.yml` is deliberately `workflow_dispatch`-only.** `rehosting/qemu` is
still the live publisher of `penguin-qemu.tar.gz`, and penguin's non-Nix image
path resolves a `QEMU_VERSION` release tag. Two repos cutting releases of the
same artifact under separate version lines is the confusion that
"non-destructive first" exists to avoid. The `push` trigger is written out and
commented; uncomment it when `rehosting/qemu` is frozen.

### Two things the round-trip check buys

`series.yml` runs `import-series.sh` → `export-series.sh` → `git diff --exit-code`.
That catches a patch hand-edited into a form the tooling would not emit — the
slow drift where a committed series stops matching its own scripts. It only works
because the exporter is deterministic (`--no-numbered --zero-commit
--full-index`); before that fix a no-change export diffed every file.

### The canary over-reports, on purpose

It runs `PROBE_STRICT_ONLY=1`, because `git am --3way` rebuilds a patch's
pre-image from its index-line blob hashes and a fresh upstream clone has none of
our blobs. Strict apply needs no blobs. The cost is that context drift and real
conflicts collapse into one bucket: measured against `upstream/master`, a
full-clone probe says `ok=9 drift=2 conflict=1` while the canary says
`ok=9 conflict=3`. The reported number is an upper bound and the issue body says
so — run `probe-versions.sh` locally against a full clone for the real split.

## State — read before relying on this

**Proven here:**

- The series applies **12/12 to the pristine v11.1.0 tarball with strict context**
  (`git am`, no `--3way`) and reproduces the recorded tree.
- The gate catches corruption that still applies: corrupting one line of `0012`
  still passes 12/12 on step 2 and fails on step 3.
- `nix build .#src` succeeds — an independent confirmation, since nix's
  `applyPatches` uses `patch -p1` rather than `git am`.
- **`nix build` of `penguin-qemu` succeeds on the full matrix**: all **14**
  declared arches produce a library, plus `libqemu-kvm-x86_64.so`, along with
  `qemu-img` (reporting version 11.1.0), the CFFI headers and the compiled env
  modules. Verified with the current 30-flag feature set.
- **`Series` CI is green on `rehosting-arc`**, and the patched tree hash it
  computes there is byte-identical to the local one
  (`1b1a352f65bfe14e1b7cfe4c7c5890cdaf7f939c`) — so the series is reproducible
  across machines, and the python-`lzma` extraction path produces the same tree
  as real `xz`. The gate runs in about two minutes.
- **All 12 declared `nixDeps` are in the built artifact's runtime closure**, and
  every `--enable-*` is observably linked (`libcapstone.so.5`, `libcurl`,
  `libiscsi`, `libnfs`, `libusb`, `libusbredirparser`, `liblzo2`, `libsnappy`,
  `libbz2`, `libpng16`, `libjpeg`, `librdmacm` + `libibverbs`). VNC is in, with
  262 `vnc_` symbols and the `RFB 003` handshake string.
- `check-delta-present.sh` passes on all 14 libraries. It distinguishes the two
  guest-entry paths: 12 targets carry `helper_penguin_guest_hypercall` (22 penguin
  symbols each), while x86 carries the port-0x88 `penguin-hypercall` MemoryRegion
  literal instead (20 symbols) — x86 has no TCG helper by design. Verified to
  fail on a negative control.
- The ported series is content-identical to the original 38-commit delta:
  625 → 626 added lines, the single difference being a deliberate reflow in
  `hw/i386/pc.c`.
- `import-series.sh` → `export-series.sh` is **byte-identical** on round-trip, and
  the patched tree hash is unchanged by it.
- `configs/default.json` reproduces the same 11 targets and 11 libraries as the
  hardcoded arrays it replaces.
- The tarball/tag and subproject claims above.

**Not done yet:**

- **`build.yml`, `publish.yml` and the canary have never executed.** Only
  `series.yml` has run. `build.yml` is PR-triggered, so it needs a PR.
- **The minimal-boot gate is not written.** `check-delta-present.sh` is the
  cheaper stand-in and closes part of the same gap; see its header for why the
  hypercall-round-trip version was declined.
- **No rehost has been booted** on a v11.1.0-based build. That is the project's
  real acceptance bar and it is still outstanding.
- The first CI run found a real runner-environment issue — the Arc pods ship
  `tar` but not the `xz` binary — now fixed via a python-`lzma` fallback.

**Deliberately deferred:**

- **byok's `penguin/` tree is not vendored yet.** `origin/workspace/byok` adds
  `penguin/plugins/` + `penguin/tools/` (+1421) and edits `accel/tcg/cputlb.c`
  and `system/runstate.c`. Its `penguin/` tree belongs in `src/`, its two
  upstream-file edits belong in the series. Landing half of it would be worse
  than none, so it waits for byok.
- **`hw/i386/pc.c` owner argument.** Upstream changed the adjacent
  `ioportF0_io` region to `OBJECT(pcms)`; our `ioport88_io` still passes `NULL`.
  Preserved deliberately — a port should not absorb an unrelated ownership
  refactor — but worth revisiting.
- Nothing further on configs: the contract gate landed (see below).
- `rehosting/qemu` is untouched and still the live repo for byok, qemufeat and
  qemuci. Freezing it, and flipping `penguin/flake.nix`'s input, are separate
  later changes.
