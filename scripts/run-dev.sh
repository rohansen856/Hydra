#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${ROOT}/build"
CONFIG="${ROOT}/config/config.yaml"

"${ROOT}/scripts/build.sh"

mkdir -p "${ROOT}/data"
"${BUILD}/serverless" "${CONFIG}" &
SERVER_PID=$!
trap 'kill ${SERVER_PID} 2>/dev/null || true' EXIT

for i in $(seq 1 30); do
  if curl -sf http://127.0.0.1:8080/healthz >/dev/null; then
    break
  fi
  sleep 0.2
done

MANIFEST="${ROOT}/functions/hello/manifest.json"
sed "s|./build/functions/hello-function|${BUILD}/functions/hello-function|" "${MANIFEST}" > /tmp/hello-manifest.json
curl -sf -X POST http://127.0.0.1:8080/api/v1/functions \
  -H 'Content-Type: application/json' \
  -d @/tmp/hello-manifest.json

echo
echo "Server ready at http://127.0.0.1:8080"
echo "Try: curl -X POST http://127.0.0.1:8080/api/v1/functions/hello/invoke -H 'Content-Type: application/json' -d '{\"name\":\"Rohan\"}'"
wait ${SERVER_PID}
