# FAQ

## General

### What is this project?

A minimal serverless compute platform written in C++20. It provides function registration, worker pooling, process-based invocation, and an HTTP API  similar in spirit to AWS Lambda, but self-hosted and educational.

### Does it support Python/Node as first-class runtimes?

No. The platform executes any executable via `fork`/`exec`. Python and Node demos use `run.sh` wrappers that invoke interpreters. There is no built-in runtime management.

### What is the MVP scope?

Single-node control plane, SQLite registry, process-per-invocation workers, HTTP API, metrics, health checks, and sample C++/Python/Node functions.

---

## Build & run

### CMake can't find Boost

Install Boost system library:

```bash
# Ubuntu/Debian
sudo apt install libboost-system-dev
```

Or use Docker, which includes all dependencies.

### Port 8080 already in use

Stop the conflicting process or change `server.bind` in `config/config.yaml`.

### Server starts but `/readyz` returns not ready

The scheduler may still be initializing. Wait a few seconds. Check logs for SQLite or config errors.

---

## Functions

### Invoke returns 502 or timeout

- Verify the function binary exists at the registered path
- Test the handler locally with stdin JSON (see [Authoring Functions](authoring-functions.md))
- Check stderr output from the function process
- Increase `timeout_ms` on registration

### Python/Node function not found after register

For polyglot handlers, register the `run.sh` path. The platform copies the entire directory to `data/functions/{name}/{version}/`.

### "Hello" returns wrong name or empty message

Ensure the HTTP invoke body is valid JSON and the handler reads `payload`, not the top-level object.

### Function crashes with SIGABRT

Common cause: stdin not fully written before the child reads. This was fixed in the platform; rebuild if using an older binary.

---

## Docker

### Docker build is slow

Ensure `.dockerignore` excludes `build/`, `data/`, and `.git/`.

### E2E fails inside Docker

Check container logs:

```bash
docker compose logs control-plane
```

Verify health:

```bash
curl http://127.0.0.1:8080/healthz
```

Use in-container paths for registration (`/app/...`).

---

## API

### What's the difference between `/healthz` and `/readyz`?

- `/healthz`  process is alive
- `/readyz`  scheduler and worker manager are initialized and accepting work

### How do I list all functions?

`GET /api/v1/functions` returns registered functions.

### Error codes

See [API Reference](api.md) for HTTP status codes and error body format.

---

## Testing

### How do I run all tests?

```bash
./scripts/build.sh          # unit tests via ctest
./scripts/e2e-test.sh ...   # HTTP E2E
./scripts/docker-e2e.sh     # full Docker E2E
```

### Integration test skipped

The C++ integration test requires `SERVERLESS_E2E_URL` to be set. Without it, the test is intentionally skipped.

---

## Performance

### What's typical invoke latency?

~10–20 ms for the C++ hello function on a local machine (includes fork + exec overhead).

### How do I load test?

```bash
./scripts/load-test.sh http://127.0.0.1:8080 hello 100 4
```

See [Benchmarks](benchmark.md).
