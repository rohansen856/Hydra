#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
URL="${SERVERLESS_URL:-http://127.0.0.1:8080}"
# API E2E only needs control-plane (+ seed). Skip dashboard to avoid
# pulling node:* from Docker Hub (transient 502s break CI).
E2E_SERVICES=(control-plane seed)
DEBUG_LOG="${ROOT}/.cursor/debug-77a5c0.log"

# #region agent log
dbg() {
  local hyp="$1" msg="$2" data="$3"
  mkdir -p "$(dirname "${DEBUG_LOG}")"
  printf '{"sessionId":"77a5c0","runId":"%s","hypothesisId":"%s","location":"docker-e2e.sh","message":"%s","data":%s,"timestamp":%s}\n' \
    "${RUN_ID:-pre-fix}" "$hyp" "$msg" "$data" "$(date +%s000)" >>"${DEBUG_LOG}" 2>/dev/null || true
}
# #endregion

cleanup() {
  cd "${ROOT}"
  docker compose down -v --remove-orphans 2>/dev/null || true
}
trap cleanup EXIT

echo "=== Docker E2E: build and start (${E2E_SERVICES[*]}) ==="
cd "${ROOT}"

# #region agent log
dbg "C" "e2e_services" "{\"services\":\"${E2E_SERVICES[*]}\",\"skipDashboard\":true}"
# #endregion

attempt=1
max_attempts=3
until docker compose up -d --build "${E2E_SERVICES[@]}"; do
  # #region agent log
  dbg "A" "compose_up_failed" "{\"attempt\":${attempt},\"max\":${max_attempts}}"
  # #endregion
  if (( attempt >= max_attempts )); then
    echo "docker compose up failed after ${max_attempts} attempts" >&2
    exit 1
  fi
  sleep_s=$(( attempt * 15 ))
  echo "Retrying docker compose up in ${sleep_s}s (attempt ${attempt}/${max_attempts}) ..."
  attempt=$((attempt + 1))
  sleep "${sleep_s}"
done

# #region agent log
dbg "A" "compose_up_ok" "{\"attempt\":${attempt}}"
# #endregion

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

# #region agent log
dbg "C" "e2e_passed" "{\"url\":\"${URL}\"}"
# #endregion

echo "=== Docker E2E complete ==="
