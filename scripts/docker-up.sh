#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
URL="${SERVERLESS_URL:-http://127.0.0.1:8080}"

cd "${ROOT}"
docker compose up -d --build "$@"

echo "Waiting for ${URL}/healthz ..."
for _ in $(seq 1 60); do
  if curl -sf "${URL}/healthz" >/dev/null 2>&1; then
    break
  fi
  sleep 1
done

curl -sf "${URL}/healthz" | python3 -m json.tool
curl -sf "${URL}/readyz" | python3 -m json.tool

echo
echo "Control plane ready at ${URL}"
echo "Run E2E: ${ROOT}/scripts/e2e-test.sh ${URL}"
