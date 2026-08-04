# Function Protocol

Every function  regardless of language  communicates with the platform via **stdin/stdout JSON**.

## Invocation input (stdin)

The control plane writes one JSON document per invocation:

```json
{
  "request_id": "req-abc123",
  "function": "hello",
  "version": "1",
  "payload": {
    "name": "Rohan"
  }
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `request_id` | yes | Unique invocation ID |
| `function` | yes | Function name |
| `version` | yes | Function version |
| `payload` | yes | HTTP invoke body (object) |

## Success output (stdout)

Exactly **one** JSON document on stdout:

```json
{
  "request_id": "req-abc123",
  "status": 200,
  "body": {
    "message": "Hello Rohan"
  }
}
```

## Error output (stdout)

```json
{
  "request_id": "req-abc123",
  "status": 500,
  "error": {
    "code": "FUNCTION_ERROR",
    "message": "description"
  }
}
```

## Rules

1. **stdout** is reserved for the protocol response only
2. Diagnostics go to **stderr**
3. Produce exactly one response document, then exit
4. Non-zero exit code marks the invocation as failed
5. Maximum payload sizes: 1 MiB request/response (configurable in `config.yaml`)

## HTTP mapping

The HTTP invoke body becomes `payload`:

```bash
curl -X POST .../functions/hello/invoke -d '{"name":"Rohan"}'
```

Maps to `payload: {"name":"Rohan"}` in the stdin JSON.

## Validation

The platform rejects:

- Malformed JSON on stdout
- Missing `status` field
- Responses exceeding `limits.max_response_bytes`
- Wall-clock timeouts (default 5s)

## Language examples

See [Authoring Functions](authoring-functions.md) for C++, Python, and Node.js templates.
