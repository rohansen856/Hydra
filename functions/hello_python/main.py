#!/usr/bin/env python3
import json
import sys


def main() -> int:
    try:
        raw = sys.stdin.read()
        data = json.loads(raw)
        payload = data.get("payload")
        if not isinstance(payload, dict):
            payload = data
        name = payload.get("name", "World")
        output = {
            "request_id": data.get("request_id", ""),
            "status": 200,
            "body": {"message": f"Hello {name} from Python"},
        }
        print(json.dumps(output))
        return 0
    except Exception as ex:
        output = {
            "status": 500,
            "error": {"code": "FUNCTION_ERROR", "message": str(ex)},
        }
        print(json.dumps(output))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
