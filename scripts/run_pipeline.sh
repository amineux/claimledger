#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${ROOT}/build/claimledger"
if [[ ! -x "${BUILD}" ]]; then
  cmake -B "${ROOT}/build" -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build "${ROOT}/build" --target claimledger
fi
"${BUILD}" --data "${ROOT}/data/fixtures" --out "${ROOT}/out" --docs "${ROOT}/docs/data" "$@"
python3 "${ROOT}/scripts/verify_ledger.py" --journal "${ROOT}/out/ledger/journal.csv"
echo "atlas: python3 -m http.server --directory ${ROOT}/docs 8000"
