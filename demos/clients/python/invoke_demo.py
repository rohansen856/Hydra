#!/usr/bin/env python3
"""HTTP client demo: register and invoke a serverless function."""
from __future__ import annotations

import argparse
import json
import sys
import urllib.error
import urllib.request

RUNTIMES = {
    "cpp": {
        "name": "hello",
        "command_env": "SERVERLESS_CPP_HELLO",
        "default_command": "build/functions/hello-function",
        "invoke_name": "hello",
        "expect_substr": "Hello",
    },
    "python": {
        "name": "hello-python",
        "command_env": "SERVERLESS_PY_HELLO",
        "default_command": "functions/hello_python/run.sh",
        "invoke_name": "hello-python",
        "expect_substr": "Python",
    },
    "node": {
        "name": "hello-node",
        "command_env": "SERVERLESS_NODE_HELLO",
        "default_command": "functions/hello_node/run.sh",
        "invoke_name": "hello-node",
        "expect_substr": "Node",
    },
}


def request(method: str, url: str, body: dict | None = None) -> tuple[int, dict]:
    data = None
    headers = {}
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"
    req = urllib.request.Request(url, data=data, headers=headers, method=method)
    try:
        with urllib.request.urlopen(req, timeout=30) as resp:
            raw = resp.read().decode("utf-8")
            return resp.status, json.loads(raw) if raw else {}
    except urllib.error.HTTPError as err:
        raw = err.read().decode("utf-8")
        try:
            payload = json.loads(raw) if raw else {}
        except json.JSONDecodeError:
            payload = {"raw": raw}
        return err.code, payload


def main() -> int:
    parser = argparse.ArgumentParser(description="Serverless invoke demo client")
    parser.add_argument("--base-url", default="http://127.0.0.1:8080")
    parser.add_argument("--runtime", choices=list(RUNTIMES.keys()), default="cpp")
    parser.add_argument("--name", default="ClientUser")
    args = parser.parse_args()

    import os
    from pathlib import Path

    cfg = RUNTIMES[args.runtime]
    root = Path(__file__).resolve().parents[3]
    command = os.environ.get(cfg["command_env"], str(root / cfg["default_command"]))

    base = args.base_url.rstrip("/")
    reg = {
        "name": cfg["name"],
        "version": "1",
        "command": command,
        "min_workers": 0,
        "max_workers": 4,
    }
    status, out = request("POST", f"{base}/api/v1/functions", reg)
    if status not in (200, 201):
        code = out.get("error", {}).get("code")
        if status != 400 or code != "DUPLICATE_FUNCTION":
            print(json.dumps(out, indent=2))
            return 1

    status, out = request(
        "POST",
        f"{base}/api/v1/functions/{cfg['invoke_name']}/invoke",
        {"name": args.name},
    )
    print(json.dumps(out, indent=2))
    if status != 200:
        return 1
    body = out.get("body", {})
    message = body.get("message", "")
    if cfg["expect_substr"] not in message or args.name not in message:
        print(f"unexpected message: {message}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
