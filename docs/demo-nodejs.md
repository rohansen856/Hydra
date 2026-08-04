# Node.js Demo

This demo registers and invokes a Node.js function handler, and shows how to call the platform from Node.

## Function handler

Location: [`functions/hello_node/`](../functions/hello_node/)

```text
hello_node/
  run.sh          # exec node main.js
  main.js         # reads stdin JSON, writes stdout JSON
  manifest.json   # metadata
```

The handler follows the [Function Protocol](function-protocol.md):

```javascript
// Simplified flow in main.js
const data = JSON.parse(input);
const name = (data.payload && data.payload.name) || 'World';
console.log(JSON.stringify({
  request_id: data.request_id,
  status: 200,
  body: { message: `Hello ${name} from Node.js` }
}));
```

## Test locally (no server)

```bash
echo '{"request_id":"t","function":"hello-node","version":"1","payload":{"name":"Local"}}' \
  | node functions/hello_node/main.js
```

## Register on the platform

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions \
  -H 'Content-Type: application/json' \
  -d "{
    \"name\": \"hello-node\",
    \"version\": \"1\",
    \"command\": \"$(pwd)/functions/hello_node/run.sh\"
  }"
```

## Invoke

```bash
curl -X POST http://127.0.0.1:8080/api/v1/functions/hello-node/invoke \
  -H 'Content-Type: application/json' \
  -d '{"name":"Rohan"}'
```

## Node.js HTTP client demo

[`demos/clients/nodejs/invoke_demo.mjs`](../demos/clients/nodejs/invoke_demo.mjs):

```bash
node demos/clients/nodejs/invoke_demo.mjs \
  http://127.0.0.1:8080 hello-node '{"name":"Rohan"}'
```

Uses only Node built-in `fetch` (Node 18+).

## Docker paths

Inside the container:

```bash
SERVERLESS_NODE_HELLO=/app/functions/hello_node/run.sh
```

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `node: not found` | Install Node 18+ or use Docker image |
| Permission denied on run.sh | `chmod +x functions/hello_node/run.sh` |
| JSON parse error | Ensure handler reads full stdin buffer |

See [FAQ](faq.md).
