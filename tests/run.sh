#!/usr/bin/env bash
# Guest matrix runner. See tests/README.md for controls and the versioned result format.
set -euo pipefail
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
exec python3 "$HERE/run_results.py" "$@"
