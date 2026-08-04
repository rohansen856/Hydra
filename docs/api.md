# API Reference

Base URL: `http://<host>:8080`

All JSON error responses follow this shape:

```json
{
  "status": 404,
  "error": {
    "code": "FUNCTION_NOT_FOUND",
    "message": "Function does not exist"
  }
}
```

## Health & metrics

### GET /healthz

Liveness probe. Returns 200 when the process is running.

```json
{ "status": "ok" }
```

### GET /readyz

Readiness probe. Returns 200 when scheduler and worker manager are initialized.

```json
{ "status": "ready" }
```

### GET /metrics

Prometheus text exposition format. Includes invocation counters, latency histograms, and worker gauges.

### GET /api/v1/metrics/json

Structured JSON metrics for dashboard consumption.

```json
{
  "counters": [{ "name": "invocations_total", "labels": {"function": "hello:1"}, "value": 42 }],
  "gauges": [{ "name": "queue_depth", "labels": {}, "value": 2 }],
  "histograms": [{ "name": "invocation_duration_seconds", "labels": {}, "count": 42, "sum": 0.52, "buckets": [] }]
}
```

---

## Dashboard APIs

All dashboard JSON endpoints include CORS headers (`Access-Control-Allow-Origin: *`) for local frontend dev.

### GET /api/v1/stats

Platform overview snapshot for live dashboards.

```json
{
  "platform": { "ready": true, "sqlite_ok": true, "uptime_ms": 123456 },
  "invocations": { "accepted": 1200, "completed": 1195 },
  "workers": { "total": 8, "idle": 5, "busy": 3 },
  "queue": { "total_depth": 2 },
  "rates": { "invocations_per_second": 12.4, "errors_per_second": 0.1 }
}
```

### GET /api/v1/stats/functions/{name}

Per-function worker and queue stats.

```json
{
  "name": "hello",
  "function_id": "hello:1",
  "queue_depth": 0,
  "workers": { "total": 2, "idle": 1, "busy": 1 }
}
```

### GET /api/v1/invocations

Query invocation history from SQLite.

Query params: `limit` (1–500, default 50), `offset`, `function`, `status`, `since_ms`

```json
{
  "invocations": [
    {
      "request_id": "req-...",
      "function_id": "hello:1",
      "status": "COMPLETED",
      "duration_ms": 12.5,
      "error_code": "",
      "started_at": 1756860000123,
      "finished_at": 1756860000136
    }
  ],
  "total": 1195
}
```

### GET /api/v1/nodes

List registered worker nodes (multi-node deployments).

```json
{
  "nodes": [
    {
      "id": "node-abc",
      "host": "127.0.0.1",
      "port": 9090,
      "cpu_capacity": 4,
      "memory_mb": 8192,
      "running_workers": 2,
      "available_workers": 6,
      "healthy": true
    }
  ]
}
```

---

## Functions

### POST /api/v1/functions

Register a new function.

**Request body:**

```json
{
  "name": "hello",
  "version": "1",
  "command": "/absolute/path/to/executable",
  "min_workers": 1,
  "max_workers": 10,
  "timeout_ms": 5000,
  "max_concurrency": 10,
  "memory_mb": 128
}
```

| Field | Required | Default | Description |
|-------|----------|---------|-------------|
| `name` | yes |  | Function name |
| `command` | yes |  | Path to executable or `run.sh` wrapper |
| `version` | no | `"1"` | Version string |
| `min_workers` | no | 0 | Minimum warm workers |
| `max_workers` | no | 10 | Maximum workers |
| `timeout_ms` | no | 5000 | Invocation timeout |
| `max_concurrency` | no | 10 | Max concurrent invocations |
| `memory_mb` | no | 128 | Memory hint (informational) |

**Response:** 201 with function metadata.

### GET /api/v1/functions

List all registered functions.

**Response:** 200 with `{ "functions": [ ... ] }` array of function objects.

### GET /api/v1/functions/{name}

Get function metadata by name (latest version or specific version via query param).

**Response:** 200 with function object, or 404.

### DELETE /api/v1/functions/{name}

Delete a function and its managed artifacts.

**Response:** 200 with `{ "deleted": true, "name": "hello" }`, or 404.

### POST /api/v1/functions/{name}/invoke

Invoke a function synchronously.

**Request body:** JSON object passed as `payload` to the function.

```bash
curl -X POST .../functions/hello/invoke \
  -H 'Content-Type: application/json' \
  -d '{"name":"Rohan"}'
```

**Success response:** 200

```json
{
  "request_id": "req-abc123",
  "status": 200,
  "body": { "message": "Hello Rohan" },
  "duration_ms": 12.5
}
```

**Error responses:**

| HTTP | Code | When |
|------|------|------|
| 404 | `FUNCTION_NOT_FOUND` | Function not registered |
| 502 | `FUNCTION_ERROR` | Handler returned error status or crashed |
| 504 | `TIMEOUT` | Exceeded `timeout_ms` |
| 503 | `QUEUE_FULL` | Scheduler queue at capacity |

---

## Workers

### GET /api/v1/workers

List active worker processes.

**Response:**

```json
{
  "workers": [
    {
      "id": "worker-1",
      "function_id": "hello:1",
      "node_id": "local",
      "state": "IDLE",
      "is_remote": false
    }
  ]
}
```

---

## Internal (multi-node)

### POST /internal/v1/nodes/register

Register a worker node with the control plane.

### POST /internal/v1/nodes/heartbeat

Worker node heartbeat.

---

## Function protocol (stdin/stdout)

When invoked, the platform writes this JSON to the function's stdin:

```json
{
  "request_id": "req-...",
  "function": "hello",
  "version": "1",
  "payload": {}
}
```

The function must write exactly one JSON document to stdout:

```json
{
  "request_id": "req-...",
  "status": 200,
  "body": {}
}
```

See [Function Protocol](function-protocol.md) for full details.
