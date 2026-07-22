# C++ Serverless Compute Platform

A lightweight serverless compute platform in C++20 inspired by AWS Lambda / OpenFaaS.

## Features

- Function registration and HTTP invocation
- Process-isolated function execution (stdin/stdout JSON protocol)
- Worker pool with explicit state machine
- Bounded scheduler queue and autoscaling hooks
- Timeout enforcement and crash recovery paths
- Prometheus-style `/metrics`, `/healthz`, `/readyz`
- Polyglot demo handlers (C++, Python, Node.js)
- Docker deployment with full E2E verification

## Quick start (Docker)

```bash
chmod +x scripts/*.sh
./scripts/docker-e2e.sh
```

Builds the image, starts the control plane on port **8080**, runs the full E2E suite, then tears down.

Keep the server running:

```bash
./scripts/docker-up.sh
# or: docker compose up -d --build
```

- Control plane API: **http://localhost:8080**
- Hydra dashboard: **http://localhost:3000** (set `NEXT_PUBLIC_API_URL=http://localhost:8080`)
- Control plane welcome: **http://localhost:8080/**

Invoke:

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions/hello/invoke \
  -H 'Content-Type: application/json' \
  -d '{"name":"Rohan"}'
```

## Quick start (local)

```bash
chmod +x scripts/build.sh scripts/run-dev.sh
./scripts/run-dev.sh
```

In another terminal:

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions/hello/invoke \
  -H 'Content-Type: application/json' \
  -d '{"name":"Rohan"}'
```

## Build

```bash
./scripts/build.sh
```

Requirements: CMake 3.20+, C++20 compiler, Boost (system).

Dependencies are vendored in `third_party/` (nlohmann/json, spdlog, yaml-cpp, Catch2, SQLite amalgamation).

## API

Base path: `/api/v1`

| Method | Path | Description |
|--------|------|-------------|
| POST | `/api/v1/functions` | Register function |
| GET | `/api/v1/functions/{name}` | Get function |
| DELETE | `/api/v1/functions/{name}` | Delete function |
| POST | `/api/v1/functions/{name}/invoke` | Invoke function |
| GET | `/api/v1/workers` | List workers |
| GET | `/healthz` | Liveness |
| GET | `/readyz` | Readiness |
| GET | `/metrics` | Prometheus metrics |

## Documentation

Full documentation index: **[docs/README.md](docs/README.md)**

| Guide | Description |
|-------|-------------|
| [Getting Started](docs/getting-started.md) | Run locally or with Docker |
| [Docker](docs/docker.md) | Container deployment |
| [E2E Testing](docs/e2e-testing.md) | Automated verification |
| [Demo Walkthrough](docs/demo-walkthrough.md) | Full platform tour |
| [Function Protocol](docs/function-protocol.md) | stdin/stdout JSON contract |
| [Authoring Functions](docs/authoring-functions.md) | Write handlers in C++/Python/Node |
| [Architecture](docs/architecture.md) | System design |
| [API Reference](docs/api.md) | HTTP endpoints |
| [FAQ](docs/faq.md) | Troubleshooting |
| [Dashboard Spec](docs/dashboard-v0-spec.md) | v0 frontend design document |

## Client demos

```bash
python3 demos/clients/python/invoke_demo.py http://127.0.0.1:8080 hello '{"name":"Demo"}'
node demos/clients/nodejs/invoke_demo.mjs http://127.0.0.1:8080 hello '{"name":"Demo"}'
```

See [demos/clients/README.md](demos/clients/README.md).

## License

MIT  see [LICENSE](LICENSE).
