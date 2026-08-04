# Docker Deployment

## Overview

The platform ships with a multi-stage Dockerfile at [`Dockerfile`](../Dockerfile) and Compose file at [`docker-compose.yaml`](../docker-compose.yaml).

```text
builder stage  → compile serverless + C++ functions
runtime stage  → Ubuntu 22.04 + Boost + Python + Node + curl
```

## Build

```bash
./scripts/docker-build.sh
# or
docker compose build
```

## Run

```bash
./scripts/docker-up.sh
# or
docker compose up -d --build
```

Control plane listens on **0.0.0.0:8080**. Hydra dashboard UI on **http://localhost:3000**.
Data persists in Docker volume `serverless-data` mounted at `/app/data`.

The `seed` service registers `hello`, `hello-python`, and `hello-node` after the control plane is healthy.

Set the browser API URL (baked into the dashboard build) with:

```bash
NEXT_PUBLIC_API_URL=http://localhost:8080 docker compose up -d --build
```

The browser calls this URL directly (CORS is enabled on the control plane).

## Full E2E in Docker

```bash
./scripts/docker-e2e.sh
```

Runs build → start → [`scripts/e2e-test.sh`](../scripts/e2e-test.sh) → teardown.

## Image contents

| Path | Description |
|------|-------------|
| `/app/build/serverless` | Control plane binary |
| `/app/build/worker_node` | Worker node binary |
| `/app/build/functions/*` | Compiled C++ sample functions |
| `/app/functions/hello_python/` | Python demo handler |
| `/app/functions/hello_node/` | Node.js demo handler |
| `/app/config/config.yaml` | Runtime configuration |
| `/app/data/` | SQLite DB + managed function copies |

## Environment variables (E2E)

When running tests inside or against Docker:

```bash
export SERVERLESS_CPP_HELLO=/app/build/functions/hello-function
export SERVERLESS_PY_HELLO=/app/functions/hello_python/run.sh
export SERVERLESS_NODE_HELLO=/app/functions/hello_node/run.sh
```

## Multi-node profile (optional)

Worker nodes are gated behind a Compose profile:

```bash
docker compose --profile multi-node up
```

This starts `worker-node-a` and `worker-node-b` in addition to the control plane.

## Troubleshooting

| Symptom | Fix |
|---------|-----|
| Port 8080 in use | Stop local server or change host port in compose |
| Build slow | Ensure `.dockerignore` excludes `build/` and `data/` |
| Health check failing | Wait 10s; check logs: `docker compose logs control-plane` |
| Function not found in container | Use `/app/...` paths when registering inside container |

See [FAQ](faq.md) for more issues.
