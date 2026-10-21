#!/usr/bin/env python3
#
# Package Penguin's QEMU shared libraries and generated CFFI declarations.

import argparse
import bz2
import io
import json
import tarfile
from pathlib import Path


def add_file(archive, path, arcname):
    if path.exists():
        archive.add(path, arcname=arcname)
        return True
    return False


def load_manifest(path):
    if not path.exists():
        return []
    return json.loads(path.read_text())["headers"]


def add_qemu_img(archive, build_dir, entries):
    qemu_img = build_dir / "qemu-img"
    if not qemu_img.exists():
        raise SystemExit(f"missing built qemu-img: {qemu_img}")

    archive.add(qemu_img, arcname="bin/qemu-img")
    entries.append("bin/qemu-img")


# EDK2 UEFI flash images that no Penguin arch loads. Penguin direct-boots
# every guest via -kernel; the only machine that reads a default EDK2 BIOS is
# loongarch64 virt, so its images stay. These .bz2 sources look small but
# decompress to 33-65 MiB zero-padded pflash images each (~260 MiB of the
# shipped share/qemu). The small x86/i386 EDK2 variants (~4 MiB each) are
# kept: not worth the risk for the savings.
EXCLUDED_FIRMWARE = {
    "edk2-arm-code.fd",
    "edk2-arm-vars.fd",
    "edk2-aarch64-code.fd",
    "edk2-riscv-code.fd",
    "edk2-riscv-vars.fd",
}


def add_qemu_data(archive, entries):
    pc_bios = Path("pc-bios")
    if not pc_bios.exists():
        raise SystemExit("missing pc-bios directory")

    for path in sorted(pc_bios.rglob("*")):
        if not path.is_file():
            continue
        if path.name == "meson.build":
            continue
        if path.name.removesuffix(".bz2") in EXCLUDED_FIRMWARE:
            continue

        relative = path.relative_to(pc_bios)
        if path.suffix == ".bz2":
            arcname = f"share/qemu/{relative.with_suffix('')}"
            data = bz2.decompress(path.read_bytes())
            info = tarfile.TarInfo(arcname)
            info.size = len(data)
            info.mode = 0o644
            archive.addfile(info, io.BytesIO(data))
        else:
            arcname = f"share/qemu/{relative}"
            archive.add(path, arcname=arcname)

        entries.append(arcname)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--system-build-dir", default="build-system")
    parser.add_argument("--kvm-build-dir", default="build-kvm")
    parser.add_argument("--output", default="penguin-qemu.tar.gz")
    args = parser.parse_args()

    system_build_dir = Path(args.system_build_dir)
    kvm_build_dir = Path(args.kvm_build_dir)
    output = Path(args.output)

    system_manifest = system_build_dir / "qemu_cffi_system_manifest.json"
    kvm_manifest = kvm_build_dir / "qemu_cffi_kvm_manifest.json"
    manifests = [
        ("system", system_build_dir, system_manifest),
        ("kvm", kvm_build_dir, kvm_manifest),
    ]

    entries = []
    with tarfile.open(output, "w:gz") as archive:
        add_qemu_img(archive, system_build_dir, entries)
        add_qemu_data(archive, entries)

        for mode, build_dir, manifest_path in manifests:
            if add_file(
                archive,
                manifest_path,
                f"include/penguin-qemu-cffi/{manifest_path.name}",
            ):
                entries.append(f"include/penguin-qemu-cffi/{manifest_path.name}")

            for header in load_manifest(manifest_path):
                library = build_dir / header["library"]
                header_path = build_dir / header["header"]
                lib_arcname = f"lib/{header['library']}"
                header_arcname = f"include/penguin-qemu-cffi/{header['header']}"

                if not library.exists():
                    raise SystemExit(f"missing built library: {library}")
                if not header_path.exists():
                    raise SystemExit(f"missing generated header: {header_path}")

                archive.add(library, arcname=lib_arcname)
                archive.add(header_path, arcname=header_arcname)
                entries.extend([lib_arcname, header_arcname])

                env_header = header.get("env_header")
                if env_header:
                    env_path = build_dir / env_header
                    if not env_path.exists():
                        raise SystemExit(
                            f"missing generated env header: {env_path}")
                    env_arcname = f"include/penguin-qemu-cffi/{env_header}"
                    archive.add(env_path, arcname=env_arcname)
                    entries.append(env_arcname)

                env_module = header.get("env_module")
                if env_module:
                    # Compiled modules are per qemu-target; arch aliases in
                    # the manifest share one file.
                    module_arcname = f"lib/penguin-qemu-env/{env_module}"
                    if module_arcname not in entries:
                        module_path = build_dir / "penguin-qemu-env" / env_module
                        if not module_path.exists():
                            raise SystemExit(
                                f"missing compiled env module: {module_path}")
                        archive.add(module_path, arcname=module_arcname)
                        entries.append(module_arcname)

        metadata = {
            "schema": 1,
            "entries": sorted(entries),
        }
        metadata_path = output.parent / "penguin-qemu-package.json"
        metadata_path.write_text(json.dumps(metadata, indent=2, sort_keys=True) + "\n")
        archive.add(metadata_path, arcname="penguin-qemu-package.json")
        metadata_path.unlink()

    print(f"Wrote {output}")


if __name__ == "__main__":
    main()
