# Authoring Functions

Functions are executable programs that read JSON from stdin and write JSON to stdout. See [Function Protocol](function-protocol.md) for the contract.

## C++ functions

Example: [`functions/hello/main.cpp`](../functions/hello/main.cpp)

Build via CMake:

```bash
cmake --build build --target hello-function
# binary: build/functions/hello-function
```

Register:

```json
{
  "name": "hello",
  "version": "1",
  "command": "/absolute/path/to/build/functions/hello-function",
  "min_workers": 1,
  "max_workers": 4
}
```

## Python functions

Layout:

```text
functions/hello_python/
  run.sh       # wrapper copied on register
  main.py      # handler
  manifest.json
```

`run.sh`:

```bash
#!/bin/sh
exec python3 "$(dirname "$0")/main.py"
```

Handler reads full stdin, uses `payload` field, prints JSON to stdout. On register, the platform copies the entire directory into `data/functions/{name}/{version}/`.

Register with `command` pointing to `run.sh`:

```bash
curl -X POST .../functions -d '{
  "name": "hello-python",
  "version": "1",
  "command": "/app/functions/hello_python/run.sh"
}'
```

## Node.js functions

Same pattern with `run.sh`:

```bash
#!/bin/sh
exec node "$(dirname "$0")/main.js"
```

Handler in [`functions/hello_node/main.js`](../functions/hello_node/main.js).

## Registration API

`POST /api/v1/functions` required fields:

| Field | Default | Description |
|-------|---------|-------------|
| `name` |  | Function name (unique per version) |
| `command` |  | Path to executable or `run.sh` |
| `version` | `"1"` | Version string |
| `min_workers` | 0 | Warm pool minimum |
| `max_workers` | 10 | Autoscale ceiling |
| `timeout_ms` | 5000 | Wall-clock limit |
| `memory_mb` | 128 | Memory hint |
| `max_concurrency` | 10 | Per-function concurrency |

The platform copies the command artifact into `data/functions/{name}/{version}/` and stores the managed path in SQLite.

## Polyglot directory copy

When `command` ends with `run.sh`, the registry copies all files from the source directory (wrapper + handler) into the managed function directory.

## Testing a handler locally

```bash
echo '{"request_id":"t","function":"hello","version":"1","payload":{"name":"Test"}}' \
  | python3 functions/hello_python/main.py
```

Expected:

```json
{"request_id":"t","status":200,"body":{"message":"Hello Test from Python"}}
```

## Best practices

- Always read full stdin (not line-by-line only)
- Handle missing/null `payload` gracefully
- Keep stdout clean  one JSON object only
- Use stderr for debug logs
- Exit 0 on success, non-zero on failure
