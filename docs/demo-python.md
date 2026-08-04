# Python Demo

This demo registers and invokes a Python function handler, and shows how to call the platform from Python.

## Function handler

Location: [`functions/hello_python/`](../functions/hello_python/)

```text
hello_python/
  run.sh          # exec python3 main.py
  main.py         # reads stdin JSON, writes stdout JSON
  manifest.json   # metadata
```

The handler follows the [Function Protocol](function-protocol.md):

```python
# Simplified flow in main.py
data = json.load(sys.stdin)
payload = data.get("payload") or {}
name = payload.get("name", "World")
print(json.dumps({
    "request_id": data["request_id"],
    "status": 200,
    "body": {"message": f"Hello {name} from Python"}
}))
```

## Test locally (no server)

```bash
echo '{"request_id":"t","function":"hello-python","version":"1","payload":{"name":"Local"}}' \
  | python3 functions/hello_python/main.py
```

## Register on the platform

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions \
  -H 'Content-Type: application/json' \
  -d "{
    \"name\": \"hello-python\",
    \"version\": \"1\",
    \"command\": \"$(pwd)/functions/hello_python/run.sh\",
    \"min_workers\": 1
  }"
```

## Invoke

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions/hello-python/invoke \
  -H 'Content-Type: application/json' \
  -d '{"name":"Rohan"}'
```

## Python HTTP client demo

[`demos/clients/python/invoke_demo.py`](../demos/clients/python/invoke_demo.py) registers (if needed) and invokes any function:

```bash
python3 demos/clients/python/invoke_demo.py \
  http://127.0.0.1:8080 hello-python '{"name":"Rohan"}'
```

Uses only the Python standard library (`urllib`).

## Docker paths

Inside the container:

```bash
SERVERLESS_PY_HELLO=/app/functions/hello_python/run.sh
```

The E2E harness uses this path automatically when running `./scripts/docker-e2e.sh`.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `python3: not found` | Install Python 3.8+ or use Docker image |
| Permission denied on run.sh | `chmod +x functions/hello_python/run.sh` |
| Empty response | Check stderr; ensure single JSON on stdout |

See [FAQ](faq.md).
