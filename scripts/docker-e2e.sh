#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
URL="${SERVERLESS_URL:-http://127.0.0.1:8080}"

cleanup() {
  cd "${ROOT}"
  docker compose down -v --remove-orphans 2>/dev/null || true
}
trap cleanup EXIT

echo "=== Docker E2E: build and start ==="
cd "${ROOT}"
docker compose up -d --build

echo "Waiting for health ..."
for _ in $(seq 1 120); do
  if curl -sf "${URL}/healthz" >/dev/null 2>&1; then
    break
  fi
  sleep 2
done

export SERVERLESS_CPP_HELLO="/app/build/functions/hello-function"
export SERVERLESS_PY_HELLO="/app/functions/hello_python/run.sh"
export SERVERLESS_NODE_HELLO="/app/functions/hello_node/run.sh"

"${ROOT}/scripts/e2e-test.sh" "${URL}"

echo "=== Docker E2E complete ==="
