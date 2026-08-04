# Operations Guide

## Configuration

Edit [`config/config.yaml`](../config/config.yaml). Key settings:

```yaml
server:
  bind: "0.0.0.0:8080"

database:
  path: "data/serverless.db"

scheduler:
  max_queue_size: 1000

limits:
  max_request_bytes: 1048576    # 1 MiB
  max_response_bytes: 1048576

logging:
  level: info
```

Invalid config fails fast at startup with a descriptive error.

## Data directories

| Path | Purpose |
|------|---------|
| `data/serverless.db` | SQLite metadata (functions, workers) |
| `data/functions/` | Managed copies of registered executables |

Create `data/` before first run:

```bash
mkdir -p data
```

In Docker, data persists in the `serverless-data` volume.

## Running the server

**Local:**

```bash
./build/serverless config/config.yaml
```

**Dev script (build + register hello):**

```bash
./scripts/run-dev.sh
```

**Docker:**

```bash
./scripts/docker-up.sh
```

## Logs

Structured JSON logs via spdlog. Log level controlled by `logging.level` in config.

Watch Docker logs:

```bash
docker compose logs -f control-plane
```

## Metrics

Prometheus text format at `GET /metrics`:

```bash
curl -s http://127.0.0.1:8080/metrics
```

Key metrics:

- `serverless_invocations_total`  invocation count by function/status
- `serverless_invocation_duration_seconds`  latency histogram
- `serverless_workers_active`  current worker count

## Health checks

| Endpoint | Use |
|----------|-----|
| `/healthz` | Liveness  is the process alive? |
| `/readyz` | Readiness  can it accept work? |

Use these for load balancer and orchestrator probes.

## Load testing

Simple shell-based load test:

```bash
./scripts/load-test.sh http://127.0.0.1:8080 hello 100 4
# args: url function count concurrency
```

For advanced scenarios, install [k6](https://k6.io/) and use scripts under `benchmarks/scenarios/`.

See [Benchmarks](benchmark.md) for the report template.

## Docker operations

Build and run:

```bash
./scripts/docker-build.sh
./scripts/docker-up.sh
```

Full E2E verification:

```bash
./scripts/docker-e2e.sh
```

Multi-node profile:

```bash
docker compose --profile multi-node up
```

See [Docker](docker.md) for troubleshooting.

## Sanitizer builds

Address sanitizer:

```bash
cmake -S . -B build-asan -DENABLE_SANITIZERS=ON
cmake --build build-asan
./build-asan/serverless config/config.yaml
```

Thread sanitizer (separate build  do not combine with ASAN):

```bash
cmake -S . -B build-tsan -DENABLE_TSAN=ON
cmake --build build-tsan
```

## Backup & recovery

To reset all state:

```bash
rm -rf data/
mkdir -p data
# restart server and re-register functions
```

## Related docs

- [Getting Started](getting-started.md)
- [E2E Testing](e2e-testing.md)
- [FAQ](faq.md)
