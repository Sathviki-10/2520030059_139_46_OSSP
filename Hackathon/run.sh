#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ ! -x ./dining_os_lab ]]; then make; fi
exec ./dining_os_lab "$@"
