# Documentation Index

Welcome to the C++ Serverless Compute Platform documentation.

## Learning path

1. [Getting Started](getting-started.md)  run locally or with Docker in 5 minutes
2. [Demo Walkthrough](demo-walkthrough.md)  register, invoke, inspect workers/metrics
3. [Function Protocol](function-protocol.md)  stdin/stdout JSON contract
4. [Authoring Functions](authoring-functions.md)  write handlers in C++, Python, Node.js
5. [Architecture](architecture.md)  control loop, components, threading

## Operations

- [Docker](docker.md)  build images, compose, volumes, troubleshooting
- [E2E Testing](e2e-testing.md)  automated verification harness
- [Operations Guide](operations.md)  config, logs, metrics
- [API Reference](api.md)  HTTP endpoints and error codes
- [Benchmarks](benchmark.md)  load testing template

## Polyglot demos

- [Python Demo](demo-python.md)  Python function + HTTP client
- [Node.js Demo](demo-nodejs.md)  Node function + HTTP client
- [Client demos](../demos/clients/README.md)  reusable invoke scripts

## Reference

- [FAQ](faq.md)  common failures and fixes
- [Dashboard v0 Spec](dashboard-v0-spec.md)  frontend design spec for v0.dev

## Quick commands

```bash
# Local build + test
./scripts/build.sh

# Local server
./build/serverless config/config.yaml

# Docker one-liner E2E
./scripts/docker-e2e.sh

# Against running server
./scripts/e2e-test.sh http://127.0.0.1:8080
```
