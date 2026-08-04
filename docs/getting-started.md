# Getting Started

This guide gets the control plane running and invokes your first function in under five minutes.

## Prerequisites

| Mode | Requirements |
|------|----------------|
| Local | CMake 3.20+, C++20, Boost (system), `third_party/` deps vendored in repo |
| Docker | Docker 20+, Docker Compose v2 |
| Demos | Python 3.8+, Node.js 18+ (for client scripts) |

## Option A: Docker (recommended)

```bash
git clone <repo>
cd serverless-in-cpp
chmod +x scripts/*.sh
./scripts/docker-e2e.sh
```

This will:

1. Build a multi-stage Docker image
2. Start the control plane on port **8080**
3. Run the full E2E test suite (C++, Python, Node functions)
4. Tear down containers on exit

Keep the server running:

```bash
./scripts/docker-up.sh
```

Then invoke manually:

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions/hello/invoke \
  -H 'Content-Type: application/json' \
  -d '{"name":"Rohan"}'
```

Expected response:

```json
{
  "body": { "message": "Hello Rohan" },
  "request_id": "req-...",
  "status": 200,
  "duration_ms": 12.5
}
```

## Option B: Local build

```bash
./scripts/build.sh
mkdir -p data
./build/serverless config/config.yaml
```

Register the hello function:

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions \
  -H 'Content-Type: application/json' \
  -d "{
    \"name\": \"hello\",
    \"version\": \"1\",
    \"command\": \"$(pwd)/build/functions/hello-function\",
    \"min_workers\": 1,
    \"max_workers\": 4
  }"
```

Invoke:

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions/hello/invoke \
  -H 'Content-Type: application/json' \
  -d '{"name":"Rohan"}'
```

## Option C: One-command dev script

```bash
./scripts/run-dev.sh
```

Builds, starts the server, registers hello, and prints ready instructions.

## Verify health

```bash
curl http://127.0.0.1:8080/healthz   # {"status":"ok"}
curl http://127.0.0.1:8080/readyz   # {"status":"ready"}
curl http://127.0.0.1:8080/metrics  # Prometheus text
```

## Next steps

- [Demo Walkthrough](demo-walkthrough.md)  full platform tour
- [Docker](docker.md)  production-like deployment details
- [Python Demo](demo-python.md) / [Node.js Demo](demo-nodejs.md)  polyglot functions
