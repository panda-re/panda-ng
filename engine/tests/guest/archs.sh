# Per-arch facts, matching penguin's arch_registry + penguin_run.py.
# Sets: QARCH MACH CPU CON KFILE ZT DISK ENDIAN MEM EXTRA
arch_facts() {
  EXTRA=""; CPU=""; ENDIAN=""; MEM=2G   # penguin core.mem default
  case $1 in
  armel) QARCH=arm; MACH=virt; CON=ttyAMA0; KFILE=zImage.armel; ZT=arm-linux-musleabi; DISK=mmio
         if [ "$2" = 4.10 ]; then MACH="virt,highmem=off,highmem-ecam=off,highmem-mmio=off"; fi;;
  aarch64) QARCH=aarch64; MACH=virt; CPU=cortex-a57; CON=ttyAMA0; KFILE=zImage.arm64; ZT=aarch64-linux-musl; DISK=mmio;;
  mipsel) QARCH=mipsel; MACH=malta; CON=ttyS0; KFILE=vmlinux.mipsel; ZT=mipsel-linux-musleabi; DISK=pcileg;;
  mipseb) QARCH=mips; MACH=malta; CON=ttyS0; KFILE=vmlinux.mipseb; ZT=mips-linux-musleabi; DISK=pcileg;;
  mips64el) QARCH=mips64el; MACH=malta; CPU=MIPS64R2-generic; CON=ttyS0; KFILE=vmlinux.mips64el; ZT=mips64el-linux-muslabi64; DISK=pcileg;;
  mips64eb) QARCH=mips64; MACH=malta; CPU=MIPS64R2-generic; CON=ttyS0; KFILE=vmlinux.mips64eb; ZT=mips64-linux-muslabi64; DISK=pcileg;;
  powerpc64) QARCH=ppc64; MACH=pseries; CPU=power9; CON=hvc0; KFILE=vmlinux.powerpc64; ZT=powerpc64-linux-musl; DISK=virtio; ENDIAN=big;;
  powerpc64le) QARCH=ppc64; MACH=pseries; CPU=power9; CON=hvc0; KFILE=vmlinux.powerpc64le; ZT=powerpc64le-linux-musl; DISK=virtio; ENDIAN=little;;
  riscv64) QARCH=riscv64; MACH=virt; CON=ttyS0; KFILE=Image.riscv64; ZT=riscv64-linux-musl; DISK=virtio;;
  loongarch64) QARCH=loongarch64; MACH=virt; CPU=la464; CON=ttyS0; KFILE=vmlinuz.efi.loongarch64; ZT=loongarch64-linux-musl; DISK=virtio
         EXTRA="-bios $QBUILD/pc-bios/edk2-loongarch64-code.fd";;
  x86_64) QARCH=x86_64; MACH=pc; CON=ttyS0; KFILE=bzImage.x86_64; ZT=x86_64-linux-musl; DISK=virtio;;
  *) echo "unknown arch $1" >&2; return 1;;
  esac
  # 4.10 mips64 hangs in early memory setup above 256M (eb) / 1G (el), with
  # or without the plugin
  case "$1-$2" in mips64*-4.10) MEM=256M;; esac
}
