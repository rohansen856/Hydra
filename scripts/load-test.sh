#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCENARIO="${1:-warm}"
echo "Load test scenario: ${SCENARIO}"
if command -v k6 >/dev/null 2>&1; then
  k6 run "${ROOT}/benchmarks/scenarios/${SCENARIO}.js"
else
  echo "k6 not installed; running simple curl loop"
  for i in $(seq 1 100); do
    curl -sf -X POST http://127.0.0.1:8080/api/v1/functions/hello/invoke \
      -H 'Content-Type: application/json' -d '{"name":"load"}' >/dev/null &
  done
  wait
fi
