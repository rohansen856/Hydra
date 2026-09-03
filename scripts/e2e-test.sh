#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${1:-http://127.0.0.1:8080}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

pass() { echo "PASS: $*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }

json_get() {
  python3 -c "import json,sys; d=json.load(sys.stdin); print(d$1)" 2>/dev/null
}

assert_status() {
  local expected="$1"
  local actual="$2"
  local label="$3"
  if [[ "$actual" != "$expected" ]]; then
    fail "${label}: expected HTTP ${expected}, got ${actual}"
  fi
}

register_function() {
  local name="$1"
  local payload="$2"
  local label="$3"
  curl -s -o /dev/null -X DELETE "${BASE_URL}/api/v1/functions/${name}" || true
  local code
  code=$(curl -s -o "/tmp/e2e_reg_${name}.json" -w "%{http_code}" \
    -X POST "${BASE_URL}/api/v1/functions" \
    -H 'Content-Type: application/json' \
    -d "${payload}")
  assert_status 201 "$code" "${label}"
}

echo "=== E2E tests against ${BASE_URL} ==="

# 1. healthz
code=$(curl -s -o /tmp/e2e_health.json -w "%{http_code}" "${BASE_URL}/healthz")
assert_status 200 "$code" "healthz"
status=$(json_get "['status']" < /tmp/e2e_health.json)
[[ "$status" == "ok" ]] || fail "healthz status not ok"
pass "healthz"

# 2. readyz
code=$(curl -s -o /tmp/e2e_ready.json -w "%{http_code}" "${BASE_URL}/readyz")
assert_status 200 "$code" "readyz"
pass "readyz"

# 3-4. Register and invoke C++ hello
CPP_CMD="${SERVERLESS_CPP_HELLO:-${ROOT}/build/functions/hello-function}"
if [[ ! "$CPP_CMD" == /app/* ]]; then
  # Local path — verify the binary exists on this host
  [[ -x "$CPP_CMD" ]] || fail "C++ hello-function not found at ${CPP_CMD}"
fi

register_function hello "{\"name\":\"hello\",\"version\":\"1\",\"command\":\"${CPP_CMD}\",\"min_workers\":1,\"max_workers\":4}" "register cpp hello"
pass "register C++ hello"

code=$(curl -s -o /tmp/e2e_inv_cpp.json -w "%{http_code}" \
  -X POST "${BASE_URL}/api/v1/functions/hello/invoke" \
  -H 'Content-Type: application/json' \
  -d '{"name":"Rohan"}')
assert_status 200 "$code" "invoke cpp hello"
msg=$(json_get "['body']['message']" < /tmp/e2e_inv_cpp.json)
[[ "$msg" == *"Rohan"* ]] || fail "cpp invoke message missing Rohan: ${msg}"
pass "invoke C++ hello"

# 5. Python function
PY_CMD="${SERVERLESS_PY_HELLO:-${ROOT}/functions/hello_python/run.sh}"
if [[ ! "$PY_CMD" == /app/* ]]; then
  [[ -x "$PY_CMD" ]] || fail "Python run.sh not found"
fi

register_function hello-python "{\"name\":\"hello-python\",\"version\":\"1\",\"command\":\"${PY_CMD}\",\"min_workers\":0,\"max_workers\":4}" "register python hello"
pass "register Python hello"

code=$(curl -s -o /tmp/e2e_inv_py.json -w "%{http_code}" \
  -X POST "${BASE_URL}/api/v1/functions/hello-python/invoke" \
  -H 'Content-Type: application/json' \
  -d '{"name":"Alice"}')
assert_status 200 "$code" "invoke python hello"
msg=$(json_get "['body']['message']" < /tmp/e2e_inv_py.json)
[[ "$msg" == *"Alice"* && "$msg" == *"Python"* ]] || fail "python invoke bad message: ${msg}"
pass "invoke Python hello"

# 6. Node function
NODE_CMD="${SERVERLESS_NODE_HELLO:-${ROOT}/functions/hello_node/run.sh}"
if [[ ! "$NODE_CMD" == /app/* ]]; then
  [[ -x "$NODE_CMD" ]] || fail "Node run.sh not found"
fi

register_function hello-node "{\"name\":\"hello-node\",\"version\":\"1\",\"command\":\"${NODE_CMD}\",\"min_workers\":0,\"max_workers\":4}" "register node hello"
pass "register Node hello"

code=$(curl -s -o /tmp/e2e_inv_node.json -w "%{http_code}" \
  -X POST "${BASE_URL}/api/v1/functions/hello-node/invoke" \
  -H 'Content-Type: application/json' \
  -d '{"name":"Bob"}')
assert_status 200 "$code" "invoke node hello"
msg=$(json_get "['body']['message']" < /tmp/e2e_inv_node.json)
[[ "$msg" == *"Bob"* && "$msg" == *"Node"* ]] || fail "node invoke bad message: ${msg}"
pass "invoke Node hello"

# 7. workers
code=$(curl -s -o /tmp/e2e_workers.json -w "%{http_code}" "${BASE_URL}/api/v1/workers")
assert_status 200 "$code" "workers"
pass "workers list"

# 8. metrics
code=$(curl -s -o /tmp/e2e_metrics.txt -w "%{http_code}" "${BASE_URL}/metrics")
assert_status 200 "$code" "metrics"
[[ -s /tmp/e2e_metrics.txt ]] || fail "metrics empty"
pass "metrics"

# 9. get function
code=$(curl -s -o /tmp/e2e_get_fn.json -w "%{http_code}" "${BASE_URL}/api/v1/functions/hello")
assert_status 200 "$code" "get function"
pass "get function"

# 10. unknown function
code=$(curl -s -o /tmp/e2e_404.json -w "%{http_code}" \
  -X POST "${BASE_URL}/api/v1/functions/does-not-exist/invoke" \
  -H 'Content-Type: application/json' -d '{}')
[[ "$code" == "404" || "$code" == "502" ]] || fail "expected 404 for unknown function, got ${code}"
pass "unknown function error"

# 11. client demos (optional if present)
if [[ -f "${ROOT}/demos/clients/python/invoke_demo.py" ]]; then
  python3 "${ROOT}/demos/clients/python/invoke_demo.py" --base-url "${BASE_URL}" --runtime cpp --name "DemoPy"
  pass "python client demo"
fi
if [[ -f "${ROOT}/demos/clients/nodejs/invoke_demo.mjs" ]]; then
  node "${ROOT}/demos/clients/nodejs/invoke_demo.mjs" --base-url "${BASE_URL}" --runtime node --name "DemoNode"
  pass "nodejs client demo"
fi

echo "=== All E2E tests passed ==="
