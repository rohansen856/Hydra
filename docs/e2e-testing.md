# E2E Testing

## Shell harness (primary)

[`scripts/e2e-test.sh`](../scripts/e2e-test.sh) validates the full HTTP surface against a running control plane.

```bash
# Local server
./scripts/e2e-test.sh http://127.0.0.1:8080

# Docker (automated)
./scripts/docker-e2e.sh
```

## Test matrix

| Step | Endpoint | Assertion |
|------|----------|-----------|
| 1 | `GET /healthz` | 200, `status: ok` |
| 2 | `GET /readyz` | 200 |
| 3 | `POST /api/v1/functions` | Register C++ hello |
| 4 | `POST .../hello/invoke` | 200, message contains name |
| 5 | Register + invoke Python hello | message contains "Python" |
| 6 | Register + invoke Node hello | message contains "Node" |
| 7 | `GET /api/v1/workers` | 200 |
| 8 | `GET /metrics` | non-empty body |
| 9 | `GET /api/v1/functions/hello` | metadata returned |
| 10 | Invoke unknown function | 404 or 502 |
| 11 | Python + Node client demos | exit 0 |

## Environment overrides

```bash
export SERVERLESS_CPP_HELLO=/path/to/hello-function
export SERVERLESS_PY_HELLO=/path/to/hello_python/run.sh
export SERVERLESS_NODE_HELLO=/path/to/hello_node/run.sh
./scripts/e2e-test.sh http://127.0.0.1:8080
```

## C++ integration test (optional)

Set `SERVERLESS_E2E_URL` to delegate to the shell harness from `ctest`:

```bash
./scripts/docker-up.sh
export SERVERLESS_E2E_URL=http://127.0.0.1:8080
cd build && ctest -R "HTTP E2E" --output-on-failure
```

Without the env var, the test is skipped.

## CI

GitHub Actions job `docker-e2e` runs `./scripts/docker-e2e.sh` on every push/PR.

## Expected output (success)

```text
=== E2E tests against http://127.0.0.1:8080 ===
PASS: healthz
PASS: readyz
PASS: register C++ hello
PASS: invoke C++ hello
PASS: register Python hello
PASS: invoke Python hello
PASS: register Node hello
PASS: invoke Node hello
PASS: workers list
PASS: metrics
PASS: get function
PASS: unknown function error
PASS: python client demo
PASS: nodejs client demo
=== All E2E tests passed ===
```
