Syscalls2 scripts to generate prototypes and code

Linux: `nix run .#regen-syscalls` from the repository root runs `regen.sh` on the flake's pinned
kernel source, which writes ../generated-in/linux_*_prototypes.txt (`linux_prototypes.py`) and then
../generated/ (`make_all_generated.sh`). Without nix: `PYTHON=<python3 with jinja2> ./regen.sh
<linux source tree or .tar.xz>`. See ../MAINTENANCE.md.

Windows: `./make_all_prototypes.sh` writes the Windows prototype files (`prototype_parser.py`,
volatility data), then `./make_all_generated.sh` regenerates ../generated/.

`requirements2.txt` is the Python environment for running these outside nix:

```sh
python3 -m venv venv
. venv/bin/activate

pip install -r requirements2.txt
```
