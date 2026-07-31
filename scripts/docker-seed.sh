#!/usr/bin/env sh
set -eu

BASE_URL="${SERVERLESS_URL:-http://control-plane:8080}"

echo "Seeding functions against ${BASE_URL} ..."

register() {
  name="$1"
  command="$2"
  curl -sf -X POST "${BASE_URL}/api/v1/functions" \
    -H 'Content-Type: application/json' \
    -d "{\"name\":\"${name}\",\"version\":\"1\",\"command\":\"${command}\",\"min_workers\":1,\"max_workers\":4}" \
    >/dev/null || true
}

# Wait for health
i=0
while [ "$i" -lt 60 ]; do
  if curl -sf "${BASE_URL}/healthz" >/dev/null; then
    break
  fi
  i=$((i + 1))
  sleep 2
done

curl -sf "${BASE_URL}/healthz" >/dev/null

register hello /app/build/functions/hello-function
register hello-python /app/functions/hello_python/run.sh
register hello-node /app/functions/hello_node/run.sh

# Warm one invoke so dashboard has samples
curl -sf -X POST "${BASE_URL}/api/v1/functions/hello/invoke" \
  -H 'Content-Type: application/json' \
  -d '{"name":"Seed"}' >/dev/null || true

echo "Seed complete."
curl -sf "${BASE_URL}/api/v1/functions" || true
