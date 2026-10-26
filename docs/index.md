# panda-ng

panda-ng is [PANDA](https://github.com/panda-re/panda) rebuilt on QEMU 11.1.0. You run a whole
guest system, Linux on any of 13 architectures, and analyse it from Python as it runs: register
callbacks on blocks, instructions, memory and system calls, load analysis plugins, record an
execution and replay it deterministically, and snapshot and restore the guest fast enough to
fuzz it.

- **[Features](features.md)** lists what works, how settled it is, and where it is tested.
- **[Python API](reference/python.md)** documents pandare2, the package you drive it with.
- **[Callbacks](reference/callbacks.md)** lists every callback and whether it fires.
- **[Engine patches](reference/patches.md)** lists the QEMU patch series and what each patch is for.

## Get it

With nix, `nix run github:panda-re/panda-ng#python` starts a Python that has pandare2, the engine
and the plugins. The repository's
[README](https://github.com/panda-re/panda-ng/blob/main/README.md) covers the wheel, the Debian
package and using the flake from your own.

## A first program

This starts a machine with no disk, loads two C plugins, counts 20,000 basic blocks of its
firmware from a Python callback, and stops. It is `tests/smoke.py`, which CI runs.

```python
--8<-- "tests/smoke.py"
```

Guest images come from `pandare2.mkimage`; the
[tests README](https://github.com/panda-re/panda-ng/blob/main/tests/README.md) shows how to build
one.

## These pages stay current

The features page, the callback list and the patch list are generated from the code by
`tools/docs/gen.py`, and `nix flake check` fails when they are stale or when the feature list
names a patch, test or API that doesn't exist. Code examples are included from tests that run.
