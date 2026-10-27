#!/bin/bash

# Windows only: the Linux tables come from linux_prototypes.py (regen.sh, MAINTENANCE.md).
./prototype_parser.py -t win2000:x86:volatility
./prototype_parser.py -t xp_sp2:x86:volatility
./prototype_parser.py -t xp_sp3:x86:volatility
./prototype_parser.py -t win2003_sp0:x86:volatility
./prototype_parser.py -t win2003_sp12:x64:volatility
./prototype_parser.py -t win2003_sp12:x86:volatility
./prototype_parser.py -t vista_sp0:x64:volatility
./prototype_parser.py -t vista_sp0:x86:volatility
./prototype_parser.py -t vista_sp12:x64:volatility
./prototype_parser.py -t vista_sp12:x86:volatility
./prototype_parser.py -t win7_sp01:x64:volatility
./prototype_parser.py -t win7_sp01:x86:volatility
./prototype_parser.py -t win8_sp0:x64:volatility
./prototype_parser.py -t win8_sp0:x86:volatility
./prototype_parser.py -t win8_sp1:x64:volatility
./prototype_parser.py -t win8_sp1:x86:volatility

# vim: set tabstop=4 softtabstop=4 noexpandtab :
