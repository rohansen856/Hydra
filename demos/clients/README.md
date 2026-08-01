# Demo HTTP Clients

Small client programs that register and invoke functions against a running control plane.

## Python

```bash
python3 demos/clients/python/invoke_demo.py --base-url http://127.0.0.1:8080 --runtime cpp --name Rohan
python3 demos/clients/python/invoke_demo.py --base-url http://127.0.0.1:8080 --runtime python --name Alice
python3 demos/clients/python/invoke_demo.py --base-url http://127.0.0.1:8080 --runtime node --name Bob
```

## Node.js (18+)

```bash
node demos/clients/nodejs/invoke_demo.mjs --base-url http://127.0.0.1:8080 --runtime node --name Bob
```

## Docker

Start the stack first:

```bash
./scripts/docker-up.sh
./scripts/e2e-test.sh
```

Set command paths when invoking from inside the container context:

```bash
export SERVERLESS_CPP_HELLO=/app/build/functions/hello-function
export SERVERLESS_PY_HELLO=/app/functions/hello_python/run.sh
export SERVERLESS_NODE_HELLO=/app/functions/hello_node/run.sh
```
