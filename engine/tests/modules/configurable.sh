#!/usr/bin/env bash
# tests/modules/configurable.sh BUILD: the configurable machine's input checks (each bad configuration is
# an error naming the element, never an assertion) and its QMP preconfig path, on qemu-system-aarch64.
set -u
Q=$(cd "$1" && pwd)/qemu-system-aarch64; T=$(mktemp -d); fail=0
printf 'ROMDATA!ROMDATA!' > $T/rom16.bin
M=configurable
want() {  # want NAME PATTERN -M-value [args...]
    name=$1 pat=$2; shift 2
    out=$(timeout 10 "$Q" -display none -monitor none -serial null -M "$@" 2>&1); rc=$?
    if [ $rc -ne 0 ] && [ $rc -lt 128 ] && grep -qE -- "$pat" <<< "$out"; then echo "PASS $name"
    else echo "FAIL $name (rc=$rc): $out"; fail=1; fi
}
want "no -cpu" "configurable machine: no CPU type: give one with -cpu" $M
want "overlapping regions" "region\[1\] 'b' \[0x2000, 0x3000\) overlaps region\[0\] 'a' \[0x1000, 0x3000\)" \
     $M,region.0.name=a,region.0.base=0x1000,region.0.size=0x2000,region.1.name=b,region.1.base=0x2000,region.1.size=0x1000 -cpu cortex-a53
want "duplicate region name" "region\[1\]: name 'a' is used twice" \
     $M,region.0.name=a,region.0.base=0,region.0.size=0x1000,region.1.name=a,region.1.base=0x1000,region.1.size=0x1000 -cpu cortex-a53
want "no such device type" "device\[0\] \(nosuch\): no such QOM type" $M,device.0.type=nosuch -cpu cortex-a53
want "a bus device" "device\[0\] \(tmp105\): sits on a i2c-bus bus" $M,device.0.type=tmp105 -cpu cortex-a53
want "unknown IRQ target" "device\[0\] \(pl011\): irq\[0\]: 'gic' is neither 'cpu' nor a device id" \
     $M,device.0.type=pl011,device.0.mmio.0=0x9000000,device.0.irq.0=gic:1 -cpu cortex-a53
want "IRQ input out of range" "irq\[0\]: cpu has [0-9]+ interrupt input\(s\), no input 99" \
     $M,device.0.type=pl011,device.0.mmio.0=0x9000000,device.0.irq.0=cpu:99 -cpu cortex-a53
want "IRQ output out of range" "irq\[6\]: the device has 6 interrupt output\(s\)" \
     $M,device.0.type=pl011,device.0.mmio.0=0x9000000,device.0.irq.6=cpu:0,device.0.irq.0=,device.0.irq.1=,device.0.irq.2=,device.0.irq.3=,device.0.irq.4=,device.0.irq.5= -cpu cortex-a53
want "MMIO index out of range" "device\[0\] \(pl011\): mmio\[1\]: the device has 1 MMIO region" \
     $M,device.0.type=pl011,device.0.mmio.0=0x9000000,device.0.mmio.1=0xa000000 -cpu cortex-a53
want "device MMIO over a region" "device\[0\] \(pl011\) mmio\[0\] \[0x9000000, 0x9001000\) overlaps region\[0\] 'r'" \
     $M,region.0.name=r,region.0.base=0x9000000,region.0.size=0x10000,device.0.type=pl011,device.0.mmio.0=0x9000000 -cpu cortex-a53
want "bad device property" "device\[0\] \(pl011\): prop 'nosuch'" \
     $M,device.0.type=pl011,device.0.prop.nosuch=1 -cpu cortex-a53
want "file larger than its region" "region\[0\] 'r': '.*rom16.bin' gives 0x10 bytes, more than the region \(0x8\)" \
     $M,region.0.name=r,region.0.base=0,region.0.size=8,region.0.kind=rom,region.0.file=$T/rom16.bin -cpu cortex-a53
want "file-offset past the end" "file-offset 0x20 is past the end" \
     $M,region.0.name=r,region.0.base=0,region.0.size=0x1000,region.0.file=$T/rom16.bin,region.0.file-offset=0x20 -cpu cortex-a53
want "unimp with a file" "an unimp region can't have a file" \
     $M,region.0.name=r,region.0.base=0,region.0.size=0x1000,region.0.kind=unimp,region.0.file=$T/rom16.bin -cpu cortex-a53
want "alias of nothing" "region\[0\] 'b': alias-of 'zz' is not a region" \
     $M,region.0.name=b,region.0.base=0,region.0.size=0x1000,region.0.alias-of=zz -cpu cortex-a53
want "alias past its target" "run past the end of 'a'" \
     $M,region.0.name=a,region.0.base=0,region.0.size=0x1000,region.1.name=b,region.1.base=0x10000,region.1.size=0x1000,region.1.alias-of=a,region.1.alias-offset=0x800 -cpu cortex-a53
want "ram-base without RAM" "ram-base is set but there is no RAM" $M,ram-base=0x40000000 -cpu cortex-a53
want "QAPI type check (keyval)" "region.0.base.*(uint64|integer)" $M,region.0.name=a,region.0.base=xyz,region.0.size=1 -cpu cortex-a53
want "QAPI enum check" "Parameter '(region.0.)?kind' does not accept value 'flash'" \
     $M,region.0.name=a,region.0.base=0,region.0.size=1,region.0.kind=flash -cpu cortex-a53
# B8: id 'cpu' is the CPU's; region names are capped (RAMBlock ids are cut at 256 bytes); an ELF
# for another CPU is an error, not a raw image; an M-profile CPU needs the NVIC.
want "device id 'cpu'" "device\[0\] \(pl011\): id 'cpu' is taken" \
     $M,device.0.type=pl011,device.0.id=cpu,device.0.mmio.0=0x9000000 -cpu cortex-a53
long=$(printf 'r%.0s' $(seq 300))
want "region name too long" "region\[0\]: name 'rrrrrrrrrrrrrrrrrrrr...' is longer than 128 characters" \
     $M,region.0.name=$long,region.0.base=0,region.0.size=0x1000 -cpu cortex-a53
want "an ELF for another CPU (aarch64 ELF, arm CPU)" "-kernel /bin/true: .*(ELF machine|architecture).*this CPU takes ELF machine 40" \
     $M,ram-base=0x40000000 -m 16M -cpu cortex-a15 -kernel /bin/true
want "an M-profile CPU" "cortex-m3[-a-z]* is an M-profile CPU: it needs an NVIC" $M -cpu cortex-m3

# QMP in preconfig: the same properties as JSON values, then the machine is built.
out=$( (echo '{"execute":"qmp_capabilities"}'; sleep 0.3
  echo '{"execute":"qom-set","arguments":{"path":"/machine","property":"region","value":[{"name":"ram0","base":1073741824,"size":65536},{"name":"io","base":134217728,"size":4096,"kind":"unimp"}]}}'; sleep 0.3
  echo '{"execute":"qom-set","arguments":{"path":"/machine","property":"device","value":[{"type":"pl011","mmio":[150994944],"prop":{"chardev":"serial0"}}]}}'; sleep 0.3
  echo '{"execute":"x-exit-preconfig"}'; sleep 0.5
  echo '{"execute":"human-monitor-command","arguments":{"command-line":"info mtree -f"}}'; sleep 0.5
  echo '{"execute":"qom-set","arguments":{"path":"/machine","property":"entry","value":4096}}'; sleep 0.3
  echo '{"execute":"quit"}') | timeout 10 "$Q" -M configurable -cpu cortex-a53 -preconfig -qmp stdio -display none -serial null 2>&1)
if grep -q "0000000040000000-000000004000ffff (prio 0, ram): configurable.ram0" <<< "$out" &&
   grep -q "0000000009000000-0000000009000fff (prio 0, i/o): pl011" <<< "$out" &&
   grep -q "configurable.io" <<< "$out"; then echo "PASS QMP preconfig: qom-set region/device, x-exit-preconfig builds them"
else echo "FAIL QMP preconfig: $out"; fail=1; fi
if grep -q "'entry' can't change once the machine is built" <<< "$out"; then
  echo "PASS qom-set of a machine property after the machine is built is refused"
else echo "FAIL qom-set after init: $out"; fail=1; fi
rm -rf $T
exit $fail
