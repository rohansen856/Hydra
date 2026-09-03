#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
URL="${SERVERLESS_URL:-http://127.0.0.1:8080}"
# API E2E only needs control-plane (+ seed). Skip dashboard to avoid
# pulling node:* from Docker Hub (transient 502s break CI).
E2E_SERVICES=(control-plane seed)

cleanup() {
  cd "${ROOT}"
  docker compose down -v --remove-orphans 2>/dev/null || true
}
trap cleanup EXIT

echo "=== Docker E2E: build and start (${E2E_SERVICES[*]}) ==="
cd "${ROOT}"

attempt=1
max_attempts=3
until docker compose up -d --build "${E2E_SERVICES[@]}"; do
  if (( attempt >= max_attempts )); then
    echo "docker compose up failed after ${max_attempts} attempts" >&2
    exit 1
  fi
  sleep_s=$(( attempt * 15 ))
  echo "Retrying docker compose up in ${sleep_s}s (attempt ${attempt}/${max_attempts}) ..."
  attempt=$((attempt + 1))
  sleep "${sleep_s}"
done

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
